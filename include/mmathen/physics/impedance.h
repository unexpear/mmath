#pragma once

// D2 — characteristic impedance and interface coefficients.
//
// Specific acoustic impedance z = rho*c (pressure/particle-velocity; UNSW
// Physclips derivation; MIT 6.014: ~425 rayl for air at 20 C). Pressure
// amplitude coefficients at a Z1 -> Z2 discontinuity (ComPADRE acoustic lab;
// same algebra as the RF case, NIST): R = (Z2-Z1)/(Z2+Z1),
// T = 2*Z2/(Z2+Z1), with energy identity R^2 + (Z1/Z2)*T^2 = 1. Reference
// values: air ~413 rayl at 20 C (density and speed both temperature
// dependent), water 1.48 MRayl (UNSW: rho = 1000 kg/m^3, c = 1480 m/s).

#include <cmath>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "medium.h"

namespace mmathen {

// Air density from the ideal gas law: rho = p/(R*T) with the engine's
// conventional dry-air R (see medium.h provenance note).
[[nodiscard]] inline Result<DensityKgPerM3, QuantityError> airDensity(
    PressurePascals pressure, TemperatureKelvin temperature) {
  using R = Result<DensityKgPerM3, QuantityError>;
  const double p = pressure.value();
  const double t = temperature.value();
  if (!std::isfinite(p) || !std::isfinite(t))
    return R::failure(QuantityError::InvalidArgument);
  if (!(p > 0.0) || !(t > 0.0))
    return R::failure(QuantityError::DomainError);
  const double rho = p / (acoustic_constants::dryAirGasConstant * t);
  if (!std::isfinite(rho)) return R::failure(QuantityError::RangeError);
  return R::success(DensityKgPerM3{rho});
}

// z = rho*c. Both strictly positive and finite.
[[nodiscard]] inline Result<ImpedancePasPerMs, QuantityError>
characteristicImpedance(DensityKgPerM3 density, SpeedMetersPerSecond speed) {
  using R = Result<ImpedancePasPerMs, QuantityError>;
  const double rho = density.value();
  const double c = speed.value();
  if (!std::isfinite(rho) || !std::isfinite(c))
    return R::failure(QuantityError::InvalidArgument);
  if (!(rho > 0.0) || !(c > 0.0))
    return R::failure(QuantityError::DomainError);
  const double z = rho * c;
  if (!std::isfinite(z)) return R::failure(QuantityError::RangeError);
  return R::success(ImpedancePasPerMs{z});
}

// Air impedance at standard pressure (101325 Pa) and given temperature.
[[nodiscard]] inline Result<ImpedancePasPerMs, QuantityError> airImpedanceAt(
    TemperatureKelvin temperature) {
  auto rho = airDensity(PressurePascals{101325.0}, temperature);
  if (!rho.ok())
    return Result<ImpedancePasPerMs, QuantityError>::failure(rho.error());
  auto c = speedOfSound(Air{temperature});
  if (!c.ok())
    return Result<ImpedancePasPerMs, QuantityError>::failure(c.error());
  return characteristicImpedance(std::move(rho).value(),
                                 std::move(c).value());
}

// Fresh-water reference (UNSW: rho = 1000 kg/m^3, c = 1480 m/s).
[[nodiscard]] inline ImpedancePasPerMs waterImpedance() {
  return ImpedancePasPerMs{1000.0 * 1480.0};
}

// Pressure amplitude reflection coefficient R = (Z2-Z1)/(Z2+Z1), in [-1,1].
[[nodiscard]] inline Result<ReflectionCoefficient, QuantityError>
reflectionCoefficient(ImpedancePasPerMs z1, ImpedancePasPerMs z2) {
  using R = Result<ReflectionCoefficient, QuantityError>;
  const double a = z1.value();
  const double b = z2.value();
  if (!std::isfinite(a) || !std::isfinite(b))
    return R::failure(QuantityError::InvalidArgument);
  if (!(a > 0.0) || !(b > 0.0))
    return R::failure(QuantityError::DomainError);
  return R::success(ReflectionCoefficient{(b - a) / (b + a)});
}

// Pressure amplitude transmission coefficient T = 2*Z2/(Z2+Z1).
[[nodiscard]] inline Result<TransmissionCoefficient, QuantityError>
transmissionCoefficient(ImpedancePasPerMs z1, ImpedancePasPerMs z2) {
  using R = Result<TransmissionCoefficient, QuantityError>;
  const double a = z1.value();
  const double b = z2.value();
  if (!std::isfinite(a) || !std::isfinite(b))
    return R::failure(QuantityError::InvalidArgument);
  if (!(a > 0.0) || !(b > 0.0))
    return R::failure(QuantityError::DomainError);
  const double t = 2.0 * b / (b + a);
  if (!std::isfinite(t)) return R::failure(QuantityError::RangeError);
  return R::success(TransmissionCoefficient{t});
}

}  // namespace mmathen
