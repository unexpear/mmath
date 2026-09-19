#pragma once

// D24 — dynamics: a feed-forward compressor and a look-ahead peak limiter.
//
// Compressor (Giannoulis, Massberg & Reiss, "Digital Dynamic Range
// Compressor Design — A Tutorial and Analysis", JAES 60(6), 2012):
//   detector level x_dB: peak, 20 log10|x| per sample; or RMS, 10 log10 of
//     a one-pole mean square with time constant rmsMs;
//   static gain computer with threshold T, ratio R, knee width W (dB):
//     2(x - T) < -W      : y = x
//     |2(x - T)| <= W    : y = x + (1/R - 1)(x - T + W/2)^2 / (2W)
//     2(x - T) > W       : y = T + (x - T)/R
//     (continuous with continuous slope: 1 below, 1/R above; W = 0 is the
//     hard knee);
//   gain reduction g_c = y - x (<= 0 dB), smoothed in the log domain by the
//     paper's smooth branching detector:
//     g_s[n] = a_A g_s[n-1] + (1 - a_A) g_c  if g_c < g_s[n-1] (attack)
//            = a_R g_s[n-1] + (1 - a_R) g_c  otherwise         (release)
//     a = exp(-1 / (tau fs)): the step response reaches 1 - 1/e at tau;
//   output = input * 10^((g_s + makeup)/20). Stereo linking uses the larger
//     channel level, so both channels get the same gain (no image shift).
//   autoMakeupDb(p) = -G(0 dBFS) + 0: a full-scale input level comes back to
//     full scale (a stated convention, not a loudness match).
//
// Limiter: guarantees |output| <= ceiling for every sample, by
// construction, not by tuning:
//   required gain r[m] = min(1, c / peak[m]) (peak over the linked channels);
//   h[m]  = min of r over the last L+1 samples (sliding minimum);
//   g0[m] = mean of h over the last L+1 samples (box filter: linear ramps);
//   the audio is delayed by L samples, so g0[m] multiplies x[m - L].
//   Every h[j] in that mean (j in [m-L, m]) is a minimum over a window that
//   contains m - L, hence h[j] <= r[m-L] and g0[m] <= r[m-L]: the ceiling
//   holds exactly (up to the rounding of c/peak, <= a few ulp). Release is a
//   one-pole in the linear gain that only ever slows the rise
//   (g[m] = min(g0[m], smoothed)), which can only lower the gain, so the
//   guarantee survives; a final clamp g <= r[m-L] removes running-sum
//   roundoff. Latency: L = round(lookahead * fs) samples, declared. A signal
//   that never exceeds the ceiling passes as an exact delay (gain exactly 1).
//   Sample peaks only in the plain mode: inter-sample (true) peaks can
//   exceed the ceiling after reconstruction (measured: +3.0 dB on a fs/4
//   sine). LimiterParams::truePeak (D31) detects on the 4x interpolated
//   signal and widens the windows by kTruePeakExtra: sample peaks stay
//   guaranteed, true peaks are held to within 0.2 dB of the ceiling
//   (measured against an independent 16x reference).
//
// Both processors allocate only at create() and are realtime-safe after.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "../analysis/bs1770.h"
#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

enum class LevelDetector { Peak, Rms };

struct CompressorParams {
  double thresholdDb = -20.0;
  double ratio = 4.0;       // >= 1
  double kneeDb = 6.0;      // >= 0
  double attackMs = 10.0;   // > 0
  double releaseMs = 120.0; // > 0
  double makeupDb = 0.0;    // |makeup| <= 60
  LevelDetector detector = LevelDetector::Peak;
  double rmsMs = 10.0;      // > 0, RMS detector time constant
};

namespace detail {
inline constexpr double kDynFloorDb = -300.0;
inline bool validCompressor(const CompressorParams& p) {
  return std::isfinite(p.thresholdDb) && p.thresholdDb <= 0.0 &&
         p.thresholdDb >= -120.0 && std::isfinite(p.ratio) && p.ratio >= 1.0 &&
         p.ratio <= 1e6 && std::isfinite(p.kneeDb) && p.kneeDb >= 0.0 &&
         p.kneeDb <= 48.0 && std::isfinite(p.attackMs) && p.attackMs > 0.0 &&
         std::isfinite(p.releaseMs) && p.releaseMs > 0.0 &&
         std::isfinite(p.makeupDb) && std::fabs(p.makeupDb) <= 60.0 &&
         std::isfinite(p.rmsMs) && p.rmsMs > 0.0;
}
inline double smoothingCoeff(double ms, double fs) {
  const double tauSamples = ms * 1e-3 * fs;
  if (!(tauSamples > 0.0)) return 0.0;  // callers validate; plain guard
  return std::exp(-1.0 / tauSamples);
}
}  // namespace detail

// The static curve: output level (dB) for a detector level x (dB).
[[nodiscard]] inline double compressorCurveDb(const CompressorParams& p,
                                              double x) {
  const double t = p.thresholdDb, w = p.kneeDb, r = p.ratio;
  const double d = 2.0 * (x - t);
  if (d < -w) return x;
  if (w > 0.0 && std::fabs(d) <= w) {
    const double u = x - t + w / 2.0;
    // max() is a no-op here (w > 0); it keeps MSVC's C4723 constant-fold
    // check quiet when a caller inlines w = 0 into the dead branch.
    const double w2 = 2.0 * std::max(w, std::numeric_limits<double>::min());
    return x + (1.0 / r - 1.0) * u * u / w2;
  }
  return t + (x - t) / r;
}

[[nodiscard]] inline double autoMakeupDb(const CompressorParams& p) {
  return -compressorCurveDb(p, 0.0);
}

class Compressor {
 public:
  [[nodiscard]] static Result<Compressor, QuantityError> create(
      const CompressorParams& p, SampleRateHz rate) {
    using R = Result<Compressor, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !detail::validCompressor(p))
      return R::failure(QuantityError::InvalidArgument);
    Compressor c;
    c.fs_ = fs;
    c.apply(p);
    return R::success(c);
  }

  // Allocation-free parameter change; state (smoothed gain, RMS) is kept.
  [[nodiscard]] Result<CompressorParams, QuantityError> setParams(
      const CompressorParams& p) {
    using R = Result<CompressorParams, QuantityError>;
    if (!detail::validCompressor(p))
      return R::failure(QuantityError::InvalidArgument);
    apply(p);
    return R::success(p);
  }

  // Mono. Buffers may alias.
  void processBlock(const double* in, double* out, FrameCount n) {
    const auto count = static_cast<std::size_t>(n.value());
    for (std::size_t i = 0; i < count; ++i) {
      const double g = step(std::fabs(in[i]));
      out[i] = in[i] * g;
    }
  }

  // Linked stereo: one gain from the larger channel level.
  void processStereo(const double* inL, const double* inR, double* outL,
                     double* outR, FrameCount n) {
    const auto count = static_cast<std::size_t>(n.value());
    for (std::size_t i = 0; i < count; ++i) {
      const double g = step(std::max(std::fabs(inL[i]), std::fabs(inR[i])));
      outL[i] = inL[i] * g;
      outR[i] = inR[i] * g;
    }
  }

  void reset() noexcept {
    gs_ = 0.0;
    ms_ = 0.0;
  }

  // Current smoothed gain reduction (dB, <= 0), makeup excluded.
  [[nodiscard]] double gainReductionDb() const noexcept { return gs_; }
  [[nodiscard]] const CompressorParams& params() const noexcept { return p_; }

 private:
  Compressor() = default;

  void apply(const CompressorParams& p) {
    p_ = p;
    aA_ = detail::smoothingCoeff(p.attackMs, fs_);
    aR_ = detail::smoothingCoeff(p.releaseMs, fs_);
    aRms_ = detail::smoothingCoeff(p.rmsMs, fs_);
    makeup_ = p.makeupDb;
  }

  double step(double level) {
    double xDb;
    if (p_.detector == LevelDetector::Rms) {
      ms_ = aRms_ * ms_ + (1.0 - aRms_) * level * level;
      xDb = ms_ > 0.0 ? 10.0 * std::log10(ms_) : detail::kDynFloorDb;
    } else {
      xDb = level > 0.0 ? 20.0 * std::log10(level) : detail::kDynFloorDb;
    }
    const double gc = compressorCurveDb(p_, xDb) - xDb;
    gs_ = gc < gs_ ? aA_ * gs_ + (1.0 - aA_) * gc
                   : aR_ * gs_ + (1.0 - aR_) * gc;
    return std::pow(10.0, (gs_ + makeup_) / 20.0);
  }

  CompressorParams p_;
  double fs_ = 48000.0;
  double aA_ = 0.0, aR_ = 0.0, aRms_ = 0.0, makeup_ = 0.0;
  double gs_ = 0.0;
  double ms_ = 0.0;
};

struct LimiterParams {
  double ceilingDb = -1.0;  // in [-60, 0]
  double lookaheadMs = 5.0; // (0, 100]
  double releaseMs = 80.0;  // > 0
  // D31 true-peak mode (fixed at creation: it adds latency). Detection runs
  // on the 4x interpolated signal (bs1770.h), and the look-ahead and the
  // minimum window grow by kTruePeakExtra samples so each output sample's
  // gain also covers the inter-sample peaks next to it. Sample peaks stay
  // GUARANTEED as in the plain mode; true peaks are bounded approximately
  // (the gain change itself reshapes the waveform between samples), with
  // the residual measured and pinned by the tests.
  bool truePeak = false;
};

class Limiter {
 public:
  // Interpolated points made at step t describe the signal around
  // t - 6.6 .. t - 5.9 (TruePeakInterpolator), so the intervals either side
  // of sample m are seen at steps m+5 .. m+7: a window of m .. m+8 covers
  // them with slack.
  static constexpr std::size_t kTruePeakExtra = 8;

  // Lookahead is fixed at creation because it sets the latency.
  [[nodiscard]] static Result<Limiter, QuantityError> create(
      const LimiterParams& p, SampleRateHz rate) {
    using R = Result<Limiter, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !validParams(p) ||
        !std::isfinite(p.lookaheadMs) || !(p.lookaheadMs > 0.0) ||
        p.lookaheadMs > 100.0)
      return R::failure(QuantityError::InvalidArgument);
    const auto l = static_cast<std::size_t>(
        std::llround(p.lookaheadMs * 1e-3 * fs));
    if (l < 1) return R::failure(QuantityError::InvalidArgument);
    Limiter lim(l, p.truePeak ? kTruePeakExtra : 0);
    lim.fs_ = fs;
    lim.apply(p);
    return R::success(std::move(lim));
  }

  // Ceiling and release only (lookahead and mode are fixed); allocation-free.
  [[nodiscard]] Result<LimiterParams, QuantityError> setParams(
      const LimiterParams& p) {
    using R = Result<LimiterParams, QuantityError>;
    if (!validParams(p)) return R::failure(QuantityError::InvalidArgument);
    LimiterParams q = p;
    q.lookaheadMs = p_.lookaheadMs;
    q.truePeak = p_.truePeak;
    apply(q);
    return R::success(q);
  }

  void processBlock(const double* in, double* out, FrameCount n) {
    const auto count = static_cast<std::size_t>(n.value());
    for (std::size_t i = 0; i < count; ++i) {
      const double x = in[i];
      const double g = step(detect(x, x));
      const double d = delayL_[pos_];
      delayL_[pos_] = x;
      advance();
      out[i] = d * g;
    }
  }

  void processStereo(const double* inL, const double* inR, double* outL,
                     double* outR, FrameCount n) {
    const auto count = static_cast<std::size_t>(n.value());
    for (std::size_t i = 0; i < count; ++i) {
      const double xl = inL[i], xr = inR[i];
      const double g = step(detect(xl, xr));
      const double dl = delayL_[pos_], dr = delayR_[pos_];
      delayL_[pos_] = xl;
      delayR_[pos_] = xr;
      advance();
      outL[i] = dl * g;
      outR[i] = dr * g;
    }
  }

  void reset() noexcept {
    std::fill(delayL_.begin(), delayL_.end(), 0.0);
    std::fill(delayR_.begin(), delayR_.end(), 0.0);
    std::fill(rDelay_.begin(), rDelay_.end(), 1.0);
    std::fill(hRing_.begin(), hRing_.end(), 1.0);
    hSum_ = static_cast<double>(hRing_.size());
    qHead_ = qSize_ = 0;
    pos_ = 0;
    t_ = 0;
    g_ = 1.0;
    ipL_.reset();
    ipR_.reset();
  }

  [[nodiscard]] std::size_t latencyFrames() const noexcept { return l_ + e_; }
  [[nodiscard]] double gainReductionDb() const noexcept {
    return g_ > 0.0 ? 20.0 * std::log10(g_) : detail::kDynFloorDb;
  }
  [[nodiscard]] const LimiterParams& params() const noexcept { return p_; }

 private:
  Limiter(std::size_t l, std::size_t e)
      : l_(l),
        e_(e),
        delayL_(l + e, 0.0),
        delayR_(l + e, 0.0),
        rDelay_(l + e, 1.0),
        hRing_(l + 1, 1.0),
        qVal_(l + 1 + e, 1.0),
        qIdx_(l + 1 + e, 0) {
    hSum_ = static_cast<double>(l + 1);
  }

  static bool validParams(const LimiterParams& p) {
    return std::isfinite(p.ceilingDb) && p.ceilingDb <= 0.0 &&
           p.ceilingDb >= -60.0 && std::isfinite(p.releaseMs) &&
           p.releaseMs > 0.0;
  }

  void apply(const LimiterParams& p) {
    p_ = p;
    ceiling_ = std::pow(10.0, p.ceilingDb / 20.0);
    aR_ = std::exp(-1.0 / (p.releaseMs * 1e-3 * fs_));
  }

  void advance() { pos_ = (pos_ + 1) % (l_ + e_); }

  // Linked detection: sample peak, or (true-peak mode) the 4x interpolated
  // peak, which includes the sample itself.
  double detect(double xl, double xr) {
    if (e_ == 0) return std::max(std::fabs(xl), std::fabs(xr));
    return std::max(ipL_.push(xl), ipR_.push(xr));
  }

  // One sample of the gain path; returns the gain for the sample leaving
  // the delay line now (x[t - L - E]).
  double step(double peak) {
    const double r = peak > ceiling_ ? ceiling_ / peak : 1.0;
    const std::size_t qcap = qVal_.size();  // L + 1 + E
    // Sliding minimum over [t - L - E, t]: monotonic queue in a fixed ring.
    while (qSize_ > 0 && qVal_[(qHead_ + qSize_ - 1) % qcap] >= r) --qSize_;
    qVal_[(qHead_ + qSize_) % qcap] = r;
    qIdx_[(qHead_ + qSize_) % qcap] = t_;
    ++qSize_;
    while (qIdx_[qHead_] + l_ + e_ < t_) {
      qHead_ = (qHead_ + 1) % qcap;
      --qSize_;
    }
    const double h = qVal_[qHead_];
    // Box mean of h over the last L+1 values (running sum, re-summed
    // exactly once per window to bound roundoff).
    const std::size_t hcap = hRing_.size();  // L + 1
    const std::size_t hp = t_ % hcap;
    hSum_ += h - hRing_[hp];
    hRing_[hp] = h;
    if (hp == hcap - 1) {
      double s = 0.0;
      for (double v : hRing_) s += v;
      hSum_ = s;
    }
    const double box = hSum_ / static_cast<double>(hcap);
    // Release: the gain may fall at once, and rises by a one-pole.
    double g = box < g_ ? box : g_ + (box - g_) * (1.0 - aR_);
    g = std::min(g, box);
    // r for the sample leaving the delay; the clamp removes roundoff.
    const double rOut = rDelay_[pos_];
    rDelay_[pos_] = r;
    g = std::min(g, rOut);
    g_ = g;
    ++t_;
    return g;
  }

  LimiterParams p_;
  double fs_ = 48000.0;
  double ceiling_ = 1.0;
  double aR_ = 0.0;
  std::size_t l_;
  std::size_t e_;
  std::vector<double> delayL_, delayR_, rDelay_;
  std::vector<double> hRing_;
  double hSum_ = 0.0;
  std::vector<double> qVal_;
  std::vector<std::size_t> qIdx_;
  std::size_t qHead_ = 0, qSize_ = 0;
  std::size_t pos_ = 0;
  std::size_t t_ = 0;
  double g_ = 1.0;
  TruePeakInterpolator ipL_, ipR_;
};

}  // namespace mmathen
