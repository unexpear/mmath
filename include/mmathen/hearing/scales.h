#pragma once

// D18 — perceptual frequency scales and weightings.
//
// Bark (Zwicker–Traunmüller–Schroeder form: 26.81 f/(1960+f) − 0.53 with the
// sub-2 and super-20.1 corrections) is the SAME mathematics as the Bark
// fit inside analysis/consonance.h (Essentia's transcription of it); that
// module is frozen-verified and untouched, this one owns the canonical
// definition going forward. ERB follows Glasberg & Moore 1990 — the modern
// notched-noise standard, narrower than classical CB (CCRMA: ~11% of center
// frequency high, leveling ~25 Hz low). A-weighting follows IEC 61672's
// analytical rational function (not a fitted curve).
// Scale values return plain doubles with documented units (Bark, Hz, dB),
// consistent with tenneyHeight/measureBeats precedent: these are scale
// readings, not physical quantities crossing module boundaries.

#include <cmath>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

// Bark rate in Bark for f > 0 Hz.
[[nodiscard]] inline Result<double, QuantityError> barkRate(
    FrequencyHz frequency) {
  using R = Result<double, QuantityError>;
  const double f = frequency.value();
  if (!std::isfinite(f) || !(f > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  double b = (26.81 * f) / (1960.0 + f) - 0.53;
  if (b < 2.0) b += 0.15 * (2.0 - b);
  if (b > 20.1) b += 0.22 * (b - 20.1);
  if (!std::isfinite(b)) return R::failure(QuantityError::RangeError);
  return R::success(b);
}

// ERB_N bandwidth in Hz (Glasberg & Moore 1990): 24.7 * (4.37 f/1000 + 1).
[[nodiscard]] inline Result<double, QuantityError> erbBandwidth(
    FrequencyHz frequency) {
  using R = Result<double, QuantityError>;
  const double f = frequency.value();
  if (!std::isfinite(f) || !(f > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double erb = 24.7 * (4.37 * f / 1000.0 + 1.0);
  if (!std::isfinite(erb)) return R::failure(QuantityError::RangeError);
  return R::success(erb);
}

// ERB-number scale (ERBs above 0 Hz): 21.4 * log10(4.37 f/1000 + 1).
[[nodiscard]] inline Result<double, QuantityError> erbNumber(
    FrequencyHz frequency) {
  using R = Result<double, QuantityError>;
  const double f = frequency.value();
  if (!std::isfinite(f) || !(f > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double e = 21.4 * std::log10(4.37 * f / 1000.0 + 1.0);
  if (!std::isfinite(e)) return R::failure(QuantityError::RangeError);
  return R::success(e);
}

// A-weighting in dB, IEC 61672 analytical form (0 dB at 1 kHz by
// construction: same expression subtracted, so exact, not fitted).
[[nodiscard]] inline Result<double, QuantityError> aWeightingDb(
    FrequencyHz frequency) {
  using R = Result<double, QuantityError>;
  const double f = frequency.value();
  if (!std::isfinite(f) || !(f > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const auto ra = [](double x) {
    const double x2 = x * x;
    return (12194.0 * 12194.0 * x2 * x2) /
           ((x2 + 20.6 * 20.6) * (x2 + 12194.0 * 12194.0) *
            std::sqrt((x2 + 107.7 * 107.7) * (x2 + 737.9 * 737.9)));
  };
  const double ref = ra(1000.0);
  const double val = ra(f);
  if (!(ref > 0.0) || !(val > 0.0) || !std::isfinite(val))
    return R::failure(QuantityError::RangeError);
  return R::success(20.0 * std::log10(val) - 20.0 * std::log10(ref));
}

}  // namespace mmathen
