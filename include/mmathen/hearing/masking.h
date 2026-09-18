#pragma once

// D18 — simultaneous masking threshold from a spreading function.
//
// Masking here means one thing: the SPL below which a probe tone at one
// frequency is inaudible next to a masker tone, via the MPEG-1
// psychoacoustic-model spreading function (ISO/IEC 11172-3) evaluated on
// the Bark scale:
//   S(dz) = 15.81 + 7.5 (dz + 0.474) - 17.5 sqrt(1 + (dz + 0.474)^2),
// dz = probe Bark − masker Bark. Upward spread (probe above masker) decays
// ~10 dB/Bark, downward ~25 dB/Bark — the asymmetry is IN the formula, not
// asserted beside it. The reported threshold is floored at the quiet
// threshold (ISO 226 T_f table read, not a contour computation), because a
// masker never makes silence less audible. Temporal masking, informational
// masking, and multi-masker additivity (power-sum) are deferred, stated.

#include <cmath>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "loudness.h"
#include "scales.h"

namespace mmathen {

// Spreading function value in dB at a Bark separation. Finite everywhere
// (both tails -> -inf smoothly); non-finite input is InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> mpegSpreadingDb(
    double barkSeparation) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(barkSeparation))
    return R::failure(QuantityError::InvalidArgument);
  const double u = barkSeparation + 0.474;
  const double s = 15.81 + 7.5 * u - 17.5 * std::sqrt(1.0 + u * u);
  if (!std::isfinite(s)) return R::failure(QuantityError::RangeError);
  return R::success(s);
}

// Quiet (unmasked) threshold in dB SPL: direct ISO 226 T_f table read with
// the module's log-frequency interpolation. No contour math involved.
[[nodiscard]] inline Result<Decibels, QuantityError> quietThresholdSpl(
    FrequencyHz frequency) {
  using R = Result<Decibels, QuantityError>;
  const double f = frequency.value();
  if (!std::isfinite(f) || f < 20.0 || f > 12500.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(Decibels{detail::iso226Params(f).threshold});
}

// Masked threshold in dB SPL for a probe next to a pure-tone masker:
// masker SPL + spreading, floored at the quiet threshold. Frequencies in
// [20, 12500] Hz; masker level any finite dB.
[[nodiscard]] inline Result<Decibels, QuantityError> maskingThresholdDb(
    FrequencyHz maskerFrequency, Decibels maskerSpl,
    FrequencyHz probeFrequency) {
  using R = Result<Decibels, QuantityError>;
  auto maskerBark = barkRate(maskerFrequency);
  if (!maskerBark.ok()) return R::failure(maskerBark.error());
  auto probeBark = barkRate(probeFrequency);
  if (!probeBark.ok()) return R::failure(probeBark.error());
  const double level = maskerSpl.value();
  if (!std::isfinite(level))
    return R::failure(QuantityError::InvalidArgument);
  auto spread =
      mpegSpreadingDb(probeBark.value() - maskerBark.value());
  if (!spread.ok()) return R::failure(spread.error());
  auto quiet = quietThresholdSpl(probeFrequency);
  if (!quiet.ok()) return R::failure(quiet.error());
  const double masked = level + spread.value();
  const double floor = quiet.value().value();
  return R::success(
      Decibels{masked > floor ? masked : floor});
}

}  // namespace mmathen
