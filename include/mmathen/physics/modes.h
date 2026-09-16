#pragma once

// D2 — resonant modes from boundary conditions (derived, not quoted).
//
// Fixed-fixed string: separation of variables on the 1-D wave equation with
// Dirichlet ends gives k_n*L = n*pi, so lambda_n = 2L/n and (with v = f*λ)
// f_n = n*v/2L with v = sqrt(T/mu) (Feynman Lectures I Ch. 49; UMass/BU/UNCW
// lab derivations; FHWA cable report). Open-open pipes share the series;
// stopped (closed-open) pipes impose a pressure antinode at the closed end,
// keeping odd quarter wavelengths only: f_n = n*c/4L for odd n (UNSW,
// Rochester, LibreTexts, UCF, UCSB). End corrections (~0.61r per open end
// per LibreTexts) are deferred with citation, not fudged into L.

#include <cmath>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

// Wave speed on a string: v = sqrt(T/mu). Tension and linear density must
// be finite and > 0.
[[nodiscard]] inline Result<SpeedMetersPerSecond, QuantityError> stringWaveSpeed(
    TensionNewtons tension, LinearDensityKgPerM density) {
  using R = Result<SpeedMetersPerSecond, QuantityError>;
  const double t = tension.value();
  const double m = density.value();
  if (!std::isfinite(t) || !std::isfinite(m))
    return R::failure(QuantityError::InvalidArgument);
  if (!(t > 0.0) || !(m > 0.0))
    return R::failure(QuantityError::DomainError);
  const double v = std::sqrt(t / m);
  if (!std::isfinite(v)) return R::failure(QuantityError::RangeError);
  return R::success(SpeedMetersPerSecond{v});
}

// Fixed-fixed string mode n (1-based): f_n = n*v/2L.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> stringMode(
    DistanceMeters length, TensionNewtons tension, LinearDensityKgPerM density,
    int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  const double l = length.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto v = stringWaveSpeed(tension, density);
  if (!v.ok()) return R::failure(v.error());
  const double f = static_cast<double>(n) * v.value().value() / (2.0 * l);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// Open-open pipe mode n (1-based): f_n = n*c/2L (all integers).
[[nodiscard]] inline Result<FrequencyHz, QuantityError> openPipeMode(
    DistanceMeters length, SpeedMetersPerSecond speed, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  const double l = length.value();
  const double c = speed.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double f = static_cast<double>(n) * c / (2.0 * l);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// Stopped (closed-open) pipe mode: f_n = n*c/4L for ODD n only. Even n are
// not resonances of this geometry: DomainError, not a number.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> stoppedPipeMode(
    DistanceMeters length, SpeedMetersPerSecond speed, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  if (n % 2 == 0) return R::failure(QuantityError::DomainError);
  const double l = length.value();
  const double c = speed.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double f = static_cast<double>(n) * c / (4.0 * l);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

}  // namespace mmathen
