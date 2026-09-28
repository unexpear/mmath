#pragma once

// A fast pYIN acquisition path with optional causal, stable-note refinement.
// The refinement follows demodulated phase advance (Smith, Spectral Audio
// Signal Processing), extrapolated to the current sample under a locally
// linear frequency model. No tuning target enters either detector.
#include <complex>
#include <numbers>
#include "streaming_tuner.h"
#include "analysis_profiles.h"

namespace app {
inline std::size_t precisionWindow(double rate) { return static_cast<std::size_t>(std::llround(rate * .0465)); }
inline std::size_t precisionHistorySize(double rate, PitchRange range = PitchRange::Guitar) {
  if (!std::isfinite(rate) || rate < 8000) return 2048;
  const auto n = precisionWindow(rate);
  return std::max(tunerFrameSize(rate, range), n + 2 * (n / 4));
}
class PrecisionTuner {
  StreamingTuner fast_;
  std::vector<double> history_;
  PitchRange range_ = PitchRange::Guitar;
public:
  double levelDb = -120;
  bool refined = false;
  mmathen::PitchEstimate raw;
  void reset() { fast_.reset(); history_.clear(); levelDb = -120; refined = false; raw = {}; }
  mmathen::PitchEstimate process(const std::vector<double>& context, double rate, PitchRange range = PitchRange::Guitar) {
    if (range != range_) { reset(); range_ = range; }
    refined = false;
    if (!std::isfinite(rate) || rate < 8000 || context.size() < tunerFrameSize(rate, range) ||
        std::any_of(context.begin(), context.end(), [](double x){ return !std::isfinite(x); })) {
      reset(); return {};
    }
    std::vector<double> shortFrame(context.end() - static_cast<std::ptrdiff_t>(tunerFrameSize(rate, range)), context.end());
    raw = fast_.process(shortFrame, rate, range); levelDb = fast_.levelDb;
    auto result = raw;
    if (!raw.voiced || raw.frequencyHz <= 0 || levelDb <= -55) { history_.clear(); return result; }
    history_.push_back(1200 * std::log2(raw.frequencyHz));
    if (history_.size() > 4) history_.erase(history_.begin());
    const auto n = precisionWindow(rate), hop = n / 4;
    if (history_.size() < 4 || context.size() < n + 2 * hop ||
        *std::max_element(history_.begin(), history_.end()) - *std::min_element(history_.begin(), history_.end()) > 15)
      return result;
    std::array<double,4> frequencies{}, magnitudes{}, offsets{};
    const double f = raw.frequencyHz;
    for (std::size_t harmonic = 1; harmonic <= 4; ++harmonic) {
      if (f * static_cast<double>(harmonic) >= rate * .45) return result;
      std::complex<double> older{}, left{}, right{};
      const double omega = 2 * std::numbers::pi * f * static_cast<double>(harmonic) / rate;
      // Direct demodulation avoids FFT-bin quantization and any future samples.
      for (std::size_t i = 0; i < n; ++i) {
        const double window = .5 - .5 * std::cos(2 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n));
        const auto kernel = std::polar(window, -omega * static_cast<double>(i));
        older += context[context.size()-n-2*hop+i] * kernel;
        left += context[context.size()-n-hop+i] * kernel;
        right += context[context.size()-n+i] * kernel;
      }
      const auto expected = std::polar(1.0, -omega * static_cast<double>(hop));
      double delta = std::arg(right * std::conj(left) * expected);
      const double previous = std::arg(left * std::conj(older) * expected);
      delta += (static_cast<double>(n) / (2 * static_cast<double>(hop)) + .5) * (delta - previous);
      const double hz = f + delta * rate / (2 * std::numbers::pi * static_cast<double>(hop * harmonic));
      frequencies[harmonic-1] = hz;
      magnitudes[harmonic-1] = std::min({std::abs(older),std::abs(left),std::abs(right)});
      offsets[harmonic-1] = hz > 0 ? 1200 * std::log2(hz / f) : 1e9;
    }
    const double maximum = *std::max_element(magnitudes.begin(),magnitudes.end());
    double sum = 0, weight = 0, lo = 1e9, hi = -1e9;
    int count = 0;
    for (std::size_t i = 0; i < 4; ++i) if (magnitudes[i] > .2 * maximum && std::abs(offsets[i]) < 15) {
      sum += frequencies[i] * magnitudes[i]; weight += magnitudes[i]; ++count;
      lo = std::min(lo, offsets[i]); hi = std::max(hi, offsets[i]);
    }
    if (count < 2 || hi-lo > 5 || !(weight > 0)) return result;
    result.frequencyHz = sum / weight; result.periodSamples = rate / result.frequencyHz; refined = true;
    return result;
  }
};
}
