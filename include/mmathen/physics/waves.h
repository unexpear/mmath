#pragma once

// M6 — traveling-wave kinematics (analytical only).
//
// A wave varies over space AND time: y(x,t) = A*sin(k*x - w*t + phi) with
// k = 2*pi/lambda (angular wavenumber, rad/m — explicit, never bare 1/m),
// w = 2*pi*f, v = f*lambda. This header is pure kinematics: no media, no
// solvers, no PDE discretization. The bounded 1-D wave equation (strings,
// pipe pressure) and its analytical/modal/FDTD/waveguide solvers live in
// wave1d.h, fdtd1d.h and waveguide1d.h (D8); FEM and higher dimensions
// stay deferred.

#include <cmath>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"

namespace mmathen {

// k = 2*pi/lambda. Wavelength must be finite and > 0.
[[nodiscard]] inline Result<WavenumberRadPerMeter, QuantityError> wavenumber(
    DistanceMeters wavelength) {
  using R = Result<WavenumberRadPerMeter, QuantityError>;
  const double l = wavelength.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::DomainError);
  return R::success(WavenumberRadPerMeter{constants::twoPi / l});
}

// v = f * lambda. Both strictly positive and finite.
[[nodiscard]] inline Result<SpeedMetersPerSecond, QuantityError> phaseVelocity(
    FrequencyHz f, DistanceMeters wavelength) {
  using R = Result<SpeedMetersPerSecond, QuantityError>;
  const double fv = f.value();
  const double l = wavelength.value();
  if (!std::isfinite(fv) || !std::isfinite(l))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fv > 0.0) || !(l > 0.0))
    return R::failure(QuantityError::DomainError);
  const double v = fv * l;
  if (!std::isfinite(v)) return R::failure(QuantityError::RangeError);
  return R::success(SpeedMetersPerSecond{v});
}

// lambda = c / f. Speed and frequency strictly positive and finite.
[[nodiscard]] inline Result<DistanceMeters, QuantityError>
wavelengthFromFrequency(FrequencyHz f, SpeedMetersPerSecond speed) {
  using R = Result<DistanceMeters, QuantityError>;
  const double fv = f.value();
  const double c = speed.value();
  if (!std::isfinite(fv) || !std::isfinite(c))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fv > 0.0) || !(c > 0.0))
    return R::failure(QuantityError::DomainError);
  return R::success(DistanceMeters{c / fv});
}

// f = c / lambda. Mirror of the above.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> frequencyFromWavelength(
    DistanceMeters wavelength, SpeedMetersPerSecond speed) {
  using R = Result<FrequencyHz, QuantityError>;
  const double l = wavelength.value();
  const double c = speed.value();
  if (!std::isfinite(l) || !std::isfinite(c))
    return R::failure(QuantityError::InvalidArgument);
  if (!(l > 0.0) || !(c > 0.0))
    return R::failure(QuantityError::DomainError);
  const double f = c / l;
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// Propagation delay t = d / c. Speed must be finite and > 0; the displacement
// may be signed (position offset) and the sign carries into the duration —
// callers wanting a magnitude delay use a non-negative distance.
[[nodiscard]] inline Result<DurationSeconds, QuantityError> travelTime(
    DistanceMeters distance, SpeedMetersPerSecond speed) {
  using R = Result<DurationSeconds, QuantityError>;
  const double d = distance.value();
  const double c = speed.value();
  if (!std::isfinite(d))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double t = d / c;
  if (!std::isfinite(t)) return R::failure(QuantityError::RangeError);
  return R::success(DurationSeconds{t});
}

// y(x,t) = A * sin(k*x - w*t + phi). All inputs finite (checked); the
// relation itself is the analytical solution, not a solver.
[[nodiscard]] inline Result<DisplacementMeters, QuantityError> travelingWave(
    DisplacementMeters amplitude, WavenumberRadPerMeter k,
    AngularFrequencyRadPerSec omega, PhaseRadians phase, DistanceMeters x,
    TimePointSeconds t) {
  using R = Result<DisplacementMeters, QuantityError>;
  const double a = amplitude.value();
  const double kv = k.value();
  const double w = omega.value();
  const double p = phase.value();
  const double xv = x.value();
  const double tv = t.value();
  if (!std::isfinite(a) || !std::isfinite(kv) || !std::isfinite(w) ||
      !std::isfinite(p) || !std::isfinite(xv) || !std::isfinite(tv))
    return R::failure(QuantityError::InvalidArgument);
  const double y = a * std::sin(kv * xv - w * tv + p);
  if (!std::isfinite(y)) return R::failure(QuantityError::RangeError);
  return R::success(DisplacementMeters{y});
}

}  // namespace mmathen
