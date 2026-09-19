#pragma once

// D27 — time-domain fundamental-frequency (f0) estimation: normalised
// autocorrelation and YIN, plus a frame-by-frame tracker.
//
// Both search periods tau in [fs/fMax, fs/fMin] samples over one frame and
// refine the chosen lag by parabolic interpolation through its two
// neighbours (sub-sample period, so f0 is not quantised to fs/integer).
//
// Normalised autocorrelation (NACF):
//   r(tau) = sum_{j<W} x_j x_{j+tau} / sqrt(sum x_j^2 * sum x_{j+tau}^2),
//   W = N - tauMax (the same number of products at every lag), r in [-1, 1].
//   A periodic signal gives r ~ 1 at its period AND at every multiple, and
//   also near 1 at shorter lags when harmonics dominate; taking the global
//   maximum invites octave errors. The rule here: the SMALLEST lag whose
//   local maximum reaches `peakFraction` (0.9) of the global maximum.
//   confidence = r at the chosen lag; voiced when >= voicedThreshold.
//
// YIN (de Cheveigne & Kawahara, JASA 111(4), 2002), steps 1-5:
//   (2) difference  d(tau) = sum_{j<W} (x_j - x_{j+tau})^2
//   (3) cumulative mean normalised difference
//        d'(0) = 1,  d'(tau) = d(tau) / ((1/tau) sum_{k=1..tau} d(k))
//       which removes the dip at tau = 0 and favours the first period;
//   (4) absolute threshold: the first tau >= tauMin with d'(tau) < threshold
//       (0.1 by default), followed down to its local minimum; if no lag
//       qualifies, the global minimum is returned and marked unvoiced;
//   (5) parabolic interpolation around that minimum, on the RAW difference
//       d: the running mean in d' biases a parabola through d' (measured
//       here on a 1760 Hz sine: 5.6e-4 relative error on d', 2.7e-5 on d).
//   Step 6 (best local estimate across neighbouring frames) is not
//   implemented. aperiodicity = d' at the minimum (0 for a perfectly
//   periodic frame); confidence = 1 - aperiodicity.
// Cost is O(W * tauMax) per frame (direct sums): a stated limit, fine for
// frames of a few thousand samples at display rates.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

struct PitchEstimate {
  double frequencyHz = 0.0;   // 0 when no lag could be refined
  double periodSamples = 0.0; // sub-sample period
  double confidence = 0.0;    // NACF peak, or 1 - YIN aperiodicity
  bool voiced = false;
};

struct PitchSearch {
  double fMinHz = 50.0;
  double fMaxHz = 2000.0;
  double yinThreshold = 0.1;     // YIN step 4
  double peakFraction = 0.9;     // NACF: first peak within this of the max
  double voicedThreshold = 0.8;  // NACF voicing
};

namespace detail {
struct LagRange {
  std::size_t lo = 0, hi = 0, window = 0;
};
inline bool lagRange(std::size_t n, double fs, const PitchSearch& s,
                     LagRange& out) {
  if (!std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(s.fMinHz) ||
      !std::isfinite(s.fMaxHz) || !(s.fMinHz > 0.0) ||
      !(s.fMaxHz > s.fMinHz) || !(s.fMaxHz < fs / 2.0))
    return false;
  const auto lo = static_cast<std::size_t>(std::floor(fs / s.fMaxHz));
  const auto hi = static_cast<std::size_t>(std::ceil(fs / s.fMinHz));
  // Need hi + 1 for interpolation and a window at least as long as hi.
  if (lo < 2 || hi + 2 > n || n - (hi + 1) < hi) return false;
  out.lo = lo;
  out.hi = hi;
  out.window = n - (hi + 1);
  return true;
}
// Vertex offset of the parabola through (-1, a), (0, b), (1, c), in [-1, 1].
inline double parabolicOffset(double a, double b, double c) {
  const double den = a - 2.0 * b + c;
  if (!(std::fabs(den) > 0.0)) return 0.0;
  return std::clamp(0.5 * (a - c) / den, -1.0, 1.0);
}
}  // namespace detail

// Normalised autocorrelation estimate on one frame.
[[nodiscard]] inline Result<PitchEstimate, QuantityError> autocorrelationPitch(
    const std::vector<double>& frame, SampleRateHz rate,
    const PitchSearch& s = {}) {
  using R = Result<PitchEstimate, QuantityError>;
  detail::LagRange g;
  if (!detail::lagRange(frame.size(), rate.value(), s, g) ||
      !std::isfinite(s.peakFraction) || !(s.peakFraction > 0.0) ||
      s.peakFraction > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  for (double v : frame)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t w = g.window;
  std::vector<double> r(g.hi + 2, 0.0);
  double e0 = 0.0;
  for (std::size_t j = 0; j < w; ++j) e0 += frame[j] * frame[j];
  for (std::size_t tau = g.lo - 1; tau <= g.hi + 1; ++tau) {
    double num = 0.0, e1 = 0.0;
    for (std::size_t j = 0; j < w; ++j) {
      num += frame[j] * frame[j + tau];
      e1 += frame[j + tau] * frame[j + tau];
    }
    const double den = std::sqrt(e0 * e1);
    r[tau] = den > 0.0 ? num / den : 0.0;
  }
  PitchEstimate est;
  double best = -2.0;
  for (std::size_t tau = g.lo; tau <= g.hi; ++tau) best = std::max(best, r[tau]);
  if (!(best > 0.0)) return R::success(est);  // silence or no correlation
  std::size_t pick = 0;
  for (std::size_t tau = g.lo; tau <= g.hi; ++tau)
    if (r[tau] >= r[tau - 1] && r[tau] >= r[tau + 1] &&
        r[tau] >= s.peakFraction * best) {
      pick = tau;
      break;
    }
  if (pick == 0) return R::success(est);
  const double off = detail::parabolicOffset(r[pick - 1], r[pick], r[pick + 1]);
  est.periodSamples = static_cast<double>(pick) + off;
  est.frequencyHz = rate.value() / est.periodSamples;
  est.confidence = r[pick];
  est.voiced = r[pick] >= s.voicedThreshold;
  return R::success(est);
}

// YIN estimate on one frame (steps 1-5 above).
[[nodiscard]] inline Result<PitchEstimate, QuantityError> yinPitch(
    const std::vector<double>& frame, SampleRateHz rate,
    const PitchSearch& s = {}) {
  using R = Result<PitchEstimate, QuantityError>;
  detail::LagRange g;
  if (!detail::lagRange(frame.size(), rate.value(), s, g) ||
      !std::isfinite(s.yinThreshold) || !(s.yinThreshold > 0.0) ||
      s.yinThreshold >= 1.0)
    return R::failure(QuantityError::InvalidArgument);
  for (double v : frame)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t w = g.window;
  std::vector<double> dp(g.hi + 2, 1.0);  // d'(tau)
  std::vector<double> dr(g.hi + 2, 0.0);  // d(tau), for step 5
  double running = 0.0;
  for (std::size_t tau = 1; tau <= g.hi + 1; ++tau) {
    double d = 0.0;
    for (std::size_t j = 0; j < w; ++j) {
      const double e = frame[j] - frame[j + tau];
      d += e * e;
    }
    dr[tau] = d;
    running += d;
    dp[tau] = running > 0.0 ? d * static_cast<double>(tau) / running : 1.0;
  }
  PitchEstimate est;
  std::size_t pick = 0;
  for (std::size_t tau = g.lo; tau <= g.hi; ++tau)
    if (dp[tau] < s.yinThreshold) {
      while (tau + 1 <= g.hi && dp[tau + 1] < dp[tau]) ++tau;
      pick = tau;
      break;
    }
  bool voiced = pick != 0;
  if (!voiced) {
    pick = g.lo;
    for (std::size_t tau = g.lo; tau <= g.hi; ++tau)
      if (dp[tau] < dp[pick]) pick = tau;
  }
  const double off =
      detail::parabolicOffset(dr[pick - 1], dr[pick], dr[pick + 1]);
  est.periodSamples = static_cast<double>(pick) + off;
  est.frequencyHz = rate.value() / est.periodSamples;
  est.confidence = std::clamp(1.0 - dp[pick], 0.0, 1.0);
  est.voiced = voiced;
  return R::success(est);
}

enum class PitchMethod { Autocorrelation, Yin };

struct PitchFrame {
  double timeSeconds = 0.0;  // frame centre
  PitchEstimate estimate;
};

// Track f0 over a signal: frames of frameSize every hop samples.
[[nodiscard]] inline Result<std::vector<PitchFrame>, QuantityError> trackPitch(
    const std::vector<double>& samples, SampleRateHz rate,
    std::size_t frameSize, std::size_t hop, PitchMethod method,
    const PitchSearch& s = {}) {
  using R = Result<std::vector<PitchFrame>, QuantityError>;
  if (hop < 1 || frameSize < 16 || samples.size() < frameSize)
    return R::failure(QuantityError::InvalidArgument);
  std::vector<PitchFrame> out;
  std::vector<double> frame(frameSize);
  for (std::size_t start = 0; start + frameSize <= samples.size();
       start += hop) {
    std::copy(samples.begin() + static_cast<std::ptrdiff_t>(start),
              samples.begin() + static_cast<std::ptrdiff_t>(start + frameSize),
              frame.begin());
    auto e = method == PitchMethod::Yin ? yinPitch(frame, rate, s)
                                        : autocorrelationPitch(frame, rate, s);
    if (!e.ok()) return R::failure(e.error());
    out.push_back(PitchFrame{
        (static_cast<double>(start) + 0.5 * static_cast<double>(frameSize)) /
            rate.value(),
        e.value()});
  }
  return R::success(std::move(out));
}

}  // namespace mmathen
