#pragma once

// D1 — sensory dissonance from partial pairs. First analysis/ module.
//
// Model (user-selected, Essentia-primary): Plomp & Levelt 1965 data via the
// quintic fit published in Essentia's dissonance.cpp (MTG, AGPL — we
// reimplement the published fit with attribution; no Essentia code is
// incorporated). Consonance C(df) with df in critical-bandwidth units:
//   C = -6.58977878*df^5 + 28.58224226*df^4 - 47.36739986*df^3
//       + 35.70679761*df^2 - 10.36526344*df + 1.00026609,
// evaluated in Horner form, clamped to [0,1], with C = 1 outside (0, 1.18].
// The clamp at df = 0 (fit value 1.00026609 -> 1) IS the spike fix noted in
// the EPJP 2024 critique of unclamped parameterizations.
// Critical bandwidth follows the curve source: Essentia's Bark formulation
// (hz2bark Schroeder fit + barkCriticalBandwidth rational), min() of both
// partials. Dissonance D = 1 - C, summed over partial pairs weighted by raw
// amplitude products. Documented deviations from Essentia: no dBA-weighting
// (ours are synthesized linear amplitudes, not measured spectra — perceptual
// weighting belongs to the later hearing milestone), no loudness
// normalization, no 50 Hz / 10 kHz gates (our spectra are exact partial
// lists, not peak-picked audio).

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "../units/quantity.h"

namespace mmathen {

namespace tags {
struct DissonanceScore {};
}  // namespace tags

// Normalized sensory dissonance in [0,1] for the pairwise-sum model.
using Dissonance = Quantity<double, DimDimensionless, tags::DissonanceScore>;

struct Partial {
  FrequencyHz frequency;
  AmplitudeLinear level;

  bool operator==(const Partial&) const = default;
};

[[nodiscard]] inline Result<Partial, QuantityError> makePartial(
    FrequencyHz frequency, AmplitudeLinear level) {
  using R = Result<Partial, QuantityError>;
  if (!std::isfinite(frequency.value()) || !(frequency.value() > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(level.value()) || level.value() < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(Partial{frequency, level});
}

namespace detail {
// Bark scale, Schroeder-form fit (Essentia essentiamath.h, verbatim math).
inline double hz2bark(double f) {
  double b = (26.81 * f) / (1960.0 + f) - 0.53;
  if (b < 2.0) b += 0.15 * (2.0 - b);
  if (b > 20.1) b += 0.22 * (b - 20.1);
  return b;
}
// Critical bandwidth in Hz from Bark value (Essentia, verbatim math).
// Denominator zeros at z ~= 25.78/26.78, outside the Bark range.
inline double barkCriticalBandwidth(double z) {
  return 52548.0 / (z * z - 52.56 * z + 690.39);
}
// Plomp-Levelt consonance fit, Horner evaluation of the published
// coefficients; the [0,1] clamp is the zero-spike fix (see header note).
inline double plompLeveltConsonance(double df) {
  if (df < 0.0 || df > 1.18) return 1.0;
  double r = -6.58977878;
  r = r * df + 28.58224226;
  r = r * df - 47.36739986;
  r = r * df + 35.70679761;
  r = r * df - 10.36526344;
  r = r * df + 1.00026609;
  if (r < 0.0) return 0.0;
  if (r > 1.0) return 1.0;
  return r;
}
}  // namespace detail

// Critical bandwidth in Hz at a frequency (finite, > 0 input required).
[[nodiscard]] inline Result<FrequencyHz, QuantityError> criticalBandwidth(
    FrequencyHz f) {
  using R = Result<FrequencyHz, QuantityError>;
  const double fv = f.value();
  if (!std::isfinite(fv) || !(fv > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double cbw =
      detail::barkCriticalBandwidth(detail::hz2bark(fv));
  if (!std::isfinite(cbw) || !(cbw > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{cbw});
}

// Dissonance between two partials: 1 - C(|f2-f1| / min(CBW1, CBW2)).
[[nodiscard]] inline Result<Dissonance, QuantityError> pairDissonance(
    const Partial& a, const Partial& b) {
  using R = Result<Dissonance, QuantityError>;
  auto cbwA = criticalBandwidth(a.frequency);
  auto cbwB = criticalBandwidth(b.frequency);
  if (!cbwA.ok()) return R::failure(cbwA.error());
  if (!cbwB.ok()) return R::failure(cbwB.error());
  const double cbw =
      cbwA.value().value() < cbwB.value().value() ? cbwA.value().value()
                                                  : cbwB.value().value();
  const double d = detail::plompLeveltConsonance(
      std::fabs(b.frequency.value() - a.frequency.value()) / cbw);
  return R::success(Dissonance{1.0 - d});
}

// Dyad dissonance: amplitude-weighted pairwise sum over both spectra.
// Empty spectra are InvalidArgument; result overflow is RangeError.
[[nodiscard]] inline Result<Dissonance, QuantityError> dyadDissonance(
    const std::vector<Partial>& spectrumA,
    const std::vector<Partial>& spectrumB) {
  using R = Result<Dissonance, QuantityError>;
  if (spectrumA.empty() || spectrumB.empty())
    return R::failure(QuantityError::InvalidArgument);
  double total = 0.0;
  for (const auto& a : spectrumA) {
    for (const auto& b : spectrumB) {
      auto d = pairDissonance(a, b);
      if (!d.ok()) return R::failure(d.error());
      total += a.level.value() * b.level.value() * d.value().value();
    }
  }
  if (!std::isfinite(total)) return R::failure(QuantityError::RangeError);
  return R::success(Dissonance{total});
}

}  // namespace mmathen
