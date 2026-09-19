#pragma once

// D31 — programme loudness and true peak: ITU-R BS.1770-5 (11/2023; the
// algorithm used here is unchanged from -4, checked against the -5 text),
// with EBU R128 momentary/short-term windows and EBU Tech 3342 loudness
// range.
//
// K-weighting: a high-shelf "pre-filter" then an RLB high-pass, both
// biquads. BS.1770 publishes their coefficients for 48 kHz only. They are
// derived here, for any rate, from the analog prototype that reproduces
// the published 48 kHz values (the construction used by libebur128):
//   shelf:     f0 = 1681.974450955533 Hz, G = 3.999843853973347 dB,
//              Q = 0.7071752369554196, K = tan(pi f0/fs),
//              Vh = 10^(G/20), Vb = Vh^0.4996667741545416,
//              a0 = 1 + K/Q + K^2,
//              b = {Vh + Vb K/Q + K^2, 2(K^2 - Vh), Vh - Vb K/Q + K^2}/a0,
//              a = {1, 2(K^2 - 1)/a0, (1 - K/Q + K^2)/a0}
//   high-pass: f0 = 38.13547087602444 Hz, Q = 0.5003270373238773,
//              b = {1, -2, 1} (unnormalised, as published),
//              a = {1, 2(K^2 - 1)/a0, (1 - K/Q + K^2)/a0}.
// The tests check the derivation against the published 48 kHz values
// (BS.1770-5 Tables 1 and 2, same values as -4), so an error in these
// constants cannot pass.
//
// Loudness of a block: L = -0.691 + 10 log10(sum_i G_i z_i), z_i the mean
// square of K-weighted channel i, G = 1 for L, R, C and 1.41 for Ls, Rs
// (the LFE channel is excluded by the standard; callers omit it).
// Blocks: 400 ms with 75 % overlap, built from 100 ms sub-blocks
// (fs/10 samples, rounded for rates that are not a multiple of 10).
//   momentary  = the last 400 ms; short-term = the last 3 s (EBU R128);
//   integrated = two-stage gating over every 400 ms block: absolute gate
//     -70 LUFS, then a relative gate 10 LU below the loudness of the
//     blocks that passed the absolute gate;
//   loudness range (EBU Tech 3342): short-term values every 100 ms, gated
//     at -70 LUFS absolute and 20 LU relative; LRA = 95th - 10th percentile.
// Readings before enough audio has arrived are NaN-free: they report
// kLoudnessFloor (-inf is never returned).
//
// True peak (BS.1770-5 Annex 2): 4x oversampling with a 48-tap polyphase
// low-pass (12 taps per phase; windowed sinc, Kaiser window beta = 5, each
// phase normalised to unit DC gain), max |x| over the original and
// interpolated points, in dBTP. 4x oversampling under-reads high
// frequencies (the standard notes this); measured for this filter over all
// sine phases: over-read <= 0.02 dB (measured 0.0150) at any frequency;
// under-read <= 0.17 dB up to fs/4, <= 0.5 dB up to 0.45 fs, 0.68 dB at
// 0.49 fs. Kaiser beta = 5 was chosen over Blackman-Harris (0.20 dB at
// fs/4, 0.37 dB at 0.4 fs) and beta = 4 (lower droop but 0.05 dB
// over-read). The limiter's true-peak mode (dynamics.h) uses the same
// interpolator (TruePeakInterpolator). The tests pin
// these bounds. They are steady-state bounds: a hard onset from silence
// is a genuine band-limited overshoot (up to about 1 dB for a sine switched
// on at full scale) and is reported as such.
//
// Push is bounded work per sample but APPENDS block history (for gating),
// so it allocates occasionally: run it on a UI/analysis thread, not in an
// audio callback.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../dsp/biquad.h"
#include "../units/physical_units.h"

namespace mmathen {

inline constexpr double kLoudnessFloor = -200.0;  // "no reading yet"

struct KWeighting {
  BiquadCoeffs shelf;
  BiquadCoeffs highpass;
};

// K-weighting filters for sample rate fs (8 kHz .. 384 kHz).
[[nodiscard]] inline Result<KWeighting, QuantityError> kWeighting(
    SampleRateHz rate) {
  using R = Result<KWeighting, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || fs < 8000.0 || fs > 384000.0)
    return R::failure(QuantityError::InvalidArgument);
  KWeighting k;
  {
    const double f0 = 1681.974450955533, g = 3.999843853973347,
                 q = 0.7071752369554196;
    const double kk = std::tan(std::numbers::pi * f0 / fs);
    const double vh = std::pow(10.0, g / 20.0);
    const double vb = std::pow(vh, 0.4996667741545416);
    const double a0 = 1.0 + kk / q + kk * kk;
    k.shelf = {(vh + vb * kk / q + kk * kk) / a0, 2.0 * (kk * kk - vh) / a0,
               (vh - vb * kk / q + kk * kk) / a0, 2.0 * (kk * kk - 1.0) / a0,
               (1.0 - kk / q + kk * kk) / a0};
  }
  {
    const double f0 = 38.13547087602444, q = 0.5003270373238773;
    const double kk = std::tan(std::numbers::pi * f0 / fs);
    const double a0 = 1.0 + kk / q + kk * kk;
    k.highpass = {1.0, -2.0, 1.0, 2.0 * (kk * kk - 1.0) / a0,
                  (1.0 - kk / q + kk * kk) / a0};
  }
  return R::success(k);
}

// One channel of the 4x true-peak interpolator (see header): push a
// sample, get the largest magnitude among it and the 4 interpolated points
// of the newest interval. The 48-tap filter's group delay is 47/8 = 5.875
// samples, so the interpolated points describe the signal about 6 samples
// back; the limiter's true-peak mode accounts for that. The coefficient
// table is built once (function-local static, thread-safe).
class TruePeakInterpolator {
 public:
  static constexpr int kPhases = 4;
  static constexpr int kTapsPerPhase = 12;
  // Group delay of the interpolated points, in whole samples (rounded up).
  static constexpr int kDelaySamples = 6;

  double push(double x) {
    std::copy_backward(h_.begin(), h_.end() - 1, h_.end());
    h_[0] = x;
    const auto& c = table();
    double peak = std::fabs(x);
    for (std::size_t p = 0; p < kPhases; ++p) {
      double y = 0.0;
      for (std::size_t j = 0; j < kTapsPerPhase; ++j) y += c[p][j] * h_[j];
      peak = std::max(peak, std::fabs(y));
    }
    return peak;
  }
  void reset() { h_.fill(0.0); }

 private:
  using Table = std::array<std::array<double, kTapsPerPhase>, kPhases>;
  static double besselI0(double x) {  // power series, converged for x <= 10
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 40; ++k) {
      term *= (x / (2.0 * k)) * (x / (2.0 * k));
      sum += term;
    }
    return sum;
  }
  static const Table& table() {
    static const Table t = [] {
      constexpr int n = kPhases * kTapsPerPhase;
      std::array<double, n> h{};
      for (int i = 0; i < n; ++i) {
        const double tt = (static_cast<double>(i) - (n - 1) / 2.0) / kPhases;
        const double sinc = tt == 0.0 ? 1.0
                                      : std::sin(std::numbers::pi * tt) /
                                            (std::numbers::pi * tt);
        const double r = 2.0 * i / (n - 1) - 1.0;
        h[static_cast<std::size_t>(i)] =
            sinc * besselI0(5.0 * std::sqrt(1.0 - r * r)) / besselI0(5.0);
      }
      Table out{};
      for (int p = 0; p < kPhases; ++p) {
        double sum = 0.0;
        for (int j = 0; j < kTapsPerPhase; ++j)
          sum += h[static_cast<std::size_t>(j * kPhases + p)];
        for (int j = 0; j < kTapsPerPhase; ++j)
          out[static_cast<std::size_t>(p)][static_cast<std::size_t>(j)] =
              h[static_cast<std::size_t>(j * kPhases + p)] / sum;
      }
      return out;
    }();
    return t;
  }
  std::array<double, kTapsPerPhase> h_{};
};

// Polyphase 4x true-peak detector (see header): the running maximum of
// TruePeakInterpolator over every channel, plus the plain sample peak.
class TruePeakDetector {
 public:
  [[nodiscard]] static Result<TruePeakDetector, QuantityError> create(
      int channels) {
    using R = Result<TruePeakDetector, QuantityError>;
    if (channels < 1 || channels > 8)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(TruePeakDetector{channels});
  }

  // Interleaved frames.
  void push(const double* x, std::size_t frames) {
    const std::size_t ch = interp_.size();
    for (std::size_t n = 0; n < frames; ++n)
      for (std::size_t c = 0; c < ch; ++c) {
        const double v = x[n * ch + c];
        peak_ = std::max(peak_, interp_[c].push(v));
        samplePeak_ = std::max(samplePeak_, std::fabs(v));
      }
  }

  void reset() {
    for (auto& i : interp_) i.reset();
    peak_ = samplePeak_ = 0.0;
  }
  // Forget the peaks, keep the filter history (a meter's peak-hold reset).
  void clearPeaks() { peak_ = samplePeak_ = 0.0; }
  [[nodiscard]] double truePeakDb() const {
    const double m = std::max(peak_, samplePeak_);
    return m > 0.0 ? 20.0 * std::log10(m) : kLoudnessFloor;
  }
  [[nodiscard]] double samplePeakDb() const {
    return samplePeak_ > 0.0 ? 20.0 * std::log10(samplePeak_) : kLoudnessFloor;
  }

 private:
  explicit TruePeakDetector(int channels)
      : interp_(static_cast<std::size_t>(channels)) {}
  std::vector<TruePeakInterpolator> interp_;
  double peak_ = 0.0;
  double samplePeak_ = 0.0;
};

class LoudnessMeter {
 public:
  // channels 1..5 in BS.1770 order L, R, C, Ls, Rs (no LFE).
  [[nodiscard]] static Result<LoudnessMeter, QuantityError> create(
      SampleRateHz rate, int channels) {
    using R = Result<LoudnessMeter, QuantityError>;
    auto k = kWeighting(rate);
    auto tp = TruePeakDetector::create(channels);
    if (!k.ok() || !tp.ok() || channels > 5)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(LoudnessMeter{rate, channels, k.value(), std::move(tp).value()});
  }

  void push(const double* x, std::size_t frames) {
    tp_.push(x, frames);
    const auto ch = static_cast<std::size_t>(channels_);
    for (std::size_t n = 0; n < frames; ++n) {
      for (std::size_t c = 0; c < ch; ++c) {
        double v = x[n * ch + c];
        filt_[c].first.processBlock(&v, &v, FrameCount{1});
        filt_[c].second.processBlock(&v, &v, FrameCount{1});
        acc_[c] += v * v;
      }
      if (++fill_ == subLen_) closeSubBlock();
    }
  }

  void reset() {
    for (auto& f : filt_) {
      f.first.reset();
      f.second.reset();
    }
    std::fill(acc_.begin(), acc_.end(), 0.0);
    fill_ = 0;
    sub_.clear();
    blocks_.clear();
    shortTerm_.clear();
    tp_.reset();
  }

  [[nodiscard]] double momentaryLufs() const { return windowLufs(4); }
  [[nodiscard]] double shortTermLufs() const { return windowLufs(30); }

  [[nodiscard]] double integratedLufs() const {
    double sum = 0.0;
    std::size_t n = 0;
    for (double e : blocks_)
      if (lufsOf(e) > -70.0) {
        sum += e;
        ++n;
      }
    if (n == 0) return kLoudnessFloor;
    const double gate = lufsOf(sum / static_cast<double>(n)) - 10.0;
    sum = 0.0;
    n = 0;
    for (double e : blocks_) {
      const double l = lufsOf(e);
      if (l > -70.0 && l > gate) {
        sum += e;
        ++n;
      }
    }
    return n == 0 ? kLoudnessFloor : lufsOf(sum / static_cast<double>(n));
  }

  // EBU Tech 3342 loudness range, LU (0 until enough short-term values).
  [[nodiscard]] double loudnessRangeLu() const {
    std::vector<double> e;
    for (double v : shortTerm_)
      if (lufsOf(v) > -70.0) e.push_back(v);
    if (e.empty()) return 0.0;
    double mean = 0.0;
    for (double v : e) mean += v;
    const double gate = lufsOf(mean / static_cast<double>(e.size())) - 20.0;
    std::vector<double> l;
    for (double v : e)
      if (lufsOf(v) > gate) l.push_back(lufsOf(v));
    if (l.size() < 2) return 0.0;
    std::sort(l.begin(), l.end());
    auto pct = [&](double p) {
      const double idx = p * static_cast<double>(l.size() - 1);
      const auto i = static_cast<std::size_t>(std::floor(idx));
      const double f = idx - static_cast<double>(i);
      return i + 1 < l.size() ? l[i] + f * (l[i + 1] - l[i]) : l[i];
    };
    return pct(0.95) - pct(0.10);
  }

  [[nodiscard]] double truePeakDbtp() const { return tp_.truePeakDb(); }
  [[nodiscard]] double samplePeakDbfs() const { return tp_.samplePeakDb(); }
  [[nodiscard]] std::size_t blocks() const noexcept { return blocks_.size(); }

  // Loudness from a (channel-weighted) mean square.
  [[nodiscard]] static double lufsOf(double weightedMeanSquare) {
    return weightedMeanSquare > 0.0
               ? -0.691 + 10.0 * std::log10(weightedMeanSquare)
               : kLoudnessFloor;
  }

 private:
  LoudnessMeter(SampleRateHz rate, int channels, const KWeighting& k,
                TruePeakDetector tp)
      : channels_(channels),
        subLen_(std::max<std::size_t>(
            1, static_cast<std::size_t>(std::llround(rate.value() / 10.0)))),
        acc_(static_cast<std::size_t>(channels), 0.0),
        tp_(std::move(tp)) {
    for (int c = 0; c < channels; ++c)
      filt_.emplace_back(Biquad{k.shelf}, Biquad{k.highpass});
  }

  double weight(std::size_t c) const { return c >= 3 ? 1.41 : 1.0; }

  void closeSubBlock() {
    // Weighted mean square of this 100 ms sub-block.
    double e = 0.0;
    for (std::size_t c = 0; c < acc_.size(); ++c) {
      e += weight(c) * acc_[c] / static_cast<double>(subLen_);
      acc_[c] = 0.0;
    }
    fill_ = 0;
    sub_.push_back(e);
    if (sub_.size() > 30) sub_.erase(sub_.begin());
    if (sub_.size() >= 4) blocks_.push_back(meanOfLast(4));
    if (sub_.size() >= 30) shortTerm_.push_back(meanOfLast(30));
  }

  double meanOfLast(std::size_t k) const {
    double s = 0.0;
    for (std::size_t i = sub_.size() - k; i < sub_.size(); ++i) s += sub_[i];
    return s / static_cast<double>(k);
  }

  double windowLufs(std::size_t k) const {
    return sub_.size() >= k ? lufsOf(meanOfLast(k)) : kLoudnessFloor;
  }

  int channels_;
  std::size_t subLen_;
  std::vector<std::pair<Biquad, Biquad>> filt_;
  std::vector<double> acc_;
  std::size_t fill_ = 0;
  std::vector<double> sub_;        // last <= 30 sub-block energies
  std::vector<double> blocks_;     // every 400 ms block (75 % overlap)
  std::vector<double> shortTerm_;  // every 3 s window, 10 per second
  TruePeakDetector tp_;
};

}  // namespace mmathen
