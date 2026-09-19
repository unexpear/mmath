#pragma once

// D19 — spectrum analysis: amplitude and power spectra, fractional-octave
// band levels ("EQ bars"), and an STFT spectrogram. Built on D11's fft.h.
//
// Scaling (one-sided, window-corrected; N samples, window w):
//   amplitude  A_0 = |X_0|/sum w,  A_k = 2|X_k|/sum w (0 < k < N/2),
//              A_{N/2} = |X_{N/2}|/sum w
//              -> a sine of amplitude A centred on a bin reads exactly A
//                 (20 log10 A dBFS: full scale = 0 dB).
//   power      P_0 = |X_0|^2/(N sum w^2), P_k = 2|X_k|^2/(N sum w^2),
//              P_{N/2} = |X_{N/2}|^2/(N sum w^2)
//              -> by Parseval, sum_k P_k = sum (w x)^2 / sum w^2 EXACTLY:
//                 the window-weighted mean square of the signal. A steady
//                 sine of amplitude A has mean square A^2/2 (-3.01 dB for
//                 full scale).
// Levels in dB use 10 log10(power) / 20 log10(amplitude); exact zeros map
// to kSpectrumFloorDb (a stated floor, not -infinity).
//
// Fractional-octave bands follow IEC 61260-1:2014's base-ten system:
// G = 10^(3/10), exact mid-band f_m = 1000 Hz * G^(x/b) for b = 1 (octave)
// or b = 3 (third-octave), band edges f_m G^(-1/(2b)) and f_m G^(+1/(2b)).
// The exact f_m are returned (the standard's nominal labels such as 31.5,
// 63, 125 are rounded names, not provided). A bin belongs to the band that
// contains its centre frequency, half-open [lower, upper), so adjacent
// bands never double-count; each result reports how many bins it summed,
// because at coarse resolution a narrow low band can contain none.

#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "fft.h"

namespace mmathen {

inline constexpr double kSpectrumFloorDb = -300.0;

struct Spectrum {
  double binHz = 0.0;              // fs / N
  std::vector<double> amplitude;   // bins 0 .. N/2
  std::vector<double> power;       // bins 0 .. N/2, one-sided mean square
};

namespace detail {
inline double ampDb(double a) {
  return a > 0.0 ? 20.0 * std::log10(a) : kSpectrumFloorDb;
}
inline double powDb(double p) {
  return p > 0.0 ? 10.0 * std::log10(p) : kSpectrumFloorDb;
}
}  // namespace detail

// Amplitude and power spectrum of one frame. samples: power-of-two length
// >= 16, all finite; rate > 0. Otherwise InvalidArgument.
[[nodiscard]] inline Result<Spectrum, QuantityError> computeSpectrum(
    const std::vector<double>& samples, SampleRateHz rate, Window window) {
  using R = Result<Spectrum, QuantityError>;
  const std::size_t n = samples.size();
  const double fs = rate.value();
  if (n < 16 || !isPowerOfTwo(n) || n > kMaxFftSize || !std::isfinite(fs) ||
      !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto w = windowCoefficients(window, n);
  if (!w.ok()) return R::failure(w.error());
  std::vector<std::complex<double>> buf(n);
  double sumW = 0.0;
  double sumW2 = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!std::isfinite(samples[i]))
      return R::failure(QuantityError::InvalidArgument);
    const double wi = w.value()[i];
    buf[i] = wi * samples[i];
    sumW += wi;
    sumW2 += wi * wi;
  }
  detail::fftInPlace(buf, false);
  Spectrum s;
  s.binHz = fs / static_cast<double>(n);
  s.amplitude.resize(n / 2 + 1);
  s.power.resize(n / 2 + 1);
  const double nn = static_cast<double>(n);
  for (std::size_t k = 0; k <= n / 2; ++k) {
    const double mag = std::abs(buf[k]);
    const bool edge = k == 0 || k == n / 2;
    s.amplitude[k] = (edge ? 1.0 : 2.0) * mag / sumW;
    s.power[k] = (edge ? 1.0 : 2.0) * mag * mag / (nn * sumW2);
  }
  return R::success(std::move(s));
}

// Amplitude spectrum in dBFS (20 log10 A), bins 0 .. N/2.
[[nodiscard]] inline std::vector<double> amplitudeDb(const Spectrum& s) {
  std::vector<double> out(s.amplitude.size());
  for (std::size_t k = 0; k < out.size(); ++k)
    out[k] = detail::ampDb(s.amplitude[k]);
  return out;
}

struct FrequencyBand {
  FrequencyHz lower;
  FrequencyHz center;
  FrequencyHz upper;
};

// IEC 61260-1 base-ten fractional-octave bands (b = 1 or 3) whose exact
// mid-band frequency lies in [fMin, fMax]. Invalid b, non-positive or
// non-finite limits, or fMin > fMax are InvalidArgument.
[[nodiscard]] inline Result<std::vector<FrequencyBand>, QuantityError>
fractionalOctaveBands(int bandsPerOctave, FrequencyHz fMin, FrequencyHz fMax) {
  using R = Result<std::vector<FrequencyBand>, QuantityError>;
  const double lo = fMin.value();
  const double hi = fMax.value();
  if ((bandsPerOctave != 1 && bandsPerOctave != 3) || !std::isfinite(lo) ||
      !std::isfinite(hi) || !(lo > 0.0) || lo > hi)
    return R::failure(QuantityError::InvalidArgument);
  const double b = static_cast<double>(bandsPerOctave);
  const double g = std::pow(10.0, 0.3);
  // Band index x with 1000 G^(x/b) in [lo, hi].
  const double xLo = std::ceil(b * std::log(lo / 1000.0) / std::log(g) - 1e-9);
  const double xHi = std::floor(b * std::log(hi / 1000.0) / std::log(g) + 1e-9);
  std::vector<FrequencyBand> bands;
  for (double x = xLo; x <= xHi; x += 1.0) {
    const double fm = 1000.0 * std::pow(g, x / b);
    bands.push_back(FrequencyBand{FrequencyHz{fm * std::pow(g, -0.5 / b)},
                                  FrequencyHz{fm},
                                  FrequencyHz{fm * std::pow(g, 0.5 / b)}});
  }
  return R::success(std::move(bands));
}

struct BandLevel {
  FrequencyBand band;
  double power;     // summed one-sided mean square of the bins in the band
  double levelDb;   // 10 log10(power), floor kSpectrumFloorDb
  std::size_t bins; // how many bins the band summed (0: too narrow here)
};

// Sum the spectrum's power into bands (bin centre in [lower, upper)).
[[nodiscard]] inline std::vector<BandLevel> bandLevels(
    const Spectrum& s, const std::vector<FrequencyBand>& bands) {
  std::vector<BandLevel> out;
  out.reserve(bands.size());
  for (const auto& b : bands) {
    double p = 0.0;
    std::size_t count = 0;
    for (std::size_t k = 0; k < s.power.size(); ++k) {
      const double f = static_cast<double>(k) * s.binHz;
      if (f >= b.lower.value() && f < b.upper.value()) {
        p += s.power[k];
        ++count;
      }
    }
    out.push_back(BandLevel{b, p, detail::powDb(p), count});
  }
  return out;
}

struct Spectrogram {
  std::size_t frames = 0;
  std::size_t bins = 0;          // frameSize/2 + 1
  double hopSeconds = 0.0;
  double binHz = 0.0;
  double frameSeconds = 0.0;     // frame length
  std::vector<double> levelDb;   // row-major: levelDb[frame * bins + bin]
  [[nodiscard]] double at(std::size_t frame, std::size_t bin) const {
    return levelDb[frame * bins + bin];
  }
};

// Short-time amplitude spectra (dBFS): frames of frameSize samples (power of
// two >= 16) every hop samples (1 <= hop <= frameSize); frame i covers
// samples [i*hop, i*hop + frameSize). At least one full frame is required.
// Otherwise InvalidArgument.
[[nodiscard]] inline Result<Spectrogram, QuantityError> computeSpectrogram(
    const std::vector<double>& samples, SampleRateHz rate, Window window,
    std::size_t frameSize, std::size_t hop) {
  using R = Result<Spectrogram, QuantityError>;
  const double fs = rate.value();
  if (frameSize < 16 || !isPowerOfTwo(frameSize) || frameSize > kMaxFftSize ||
      hop < 1 || hop > frameSize || samples.size() < frameSize ||
      !std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  Spectrogram g;
  g.frames = 1 + (samples.size() - frameSize) / hop;
  g.bins = frameSize / 2 + 1;
  g.hopSeconds = static_cast<double>(hop) / fs;
  g.binHz = fs / static_cast<double>(frameSize);
  g.frameSeconds = static_cast<double>(frameSize) / fs;
  g.levelDb.resize(g.frames * g.bins);
  std::vector<double> frame(frameSize);
  for (std::size_t i = 0; i < g.frames; ++i) {
    for (std::size_t j = 0; j < frameSize; ++j)
      frame[j] = samples[i * hop + j];
    auto s = computeSpectrum(frame, rate, window);
    if (!s.ok()) return R::failure(s.error());
    for (std::size_t k = 0; k < g.bins; ++k)
      g.levelDb[i * g.bins + k] = detail::ampDb(s.value().amplitude[k]);
  }
  return R::success(std::move(g));
}

}  // namespace mmathen
