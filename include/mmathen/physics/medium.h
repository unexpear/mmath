#pragma once

// M6 — physical media: sound propagates through SOMETHING.
//
// Speed of sound is not a constant: for dry air (ideal-gas approximation)
// c = sqrt(gamma * R * T) with gamma = 1.4 and R = 287.058 J/(kg*K).
// R is the conventional engineering value (SI-derived R is 287.0550; the
// 10 ppm difference moves c by ~0.002 m/s, far below humidity effects).
// Stated accuracy scope: ordinary musical temperatures (roughly -20…+50 C);
// humidity (moist air is slightly FASTER, unquantified here), pressure
// corrections, and other media (water, steel, wood) are deferred with
// explicit formula strings, not silent constants.
// Temperature must be finite and > 0 K (the approximation has no meaning
// at or below absolute zero).

#include <cmath>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

namespace acoustic_constants {
inline constexpr double dryAirGamma = 1.4;
inline constexpr double dryAirGasConstant = 287.058;  // J/(kg*K)
}  // namespace acoustic_constants

struct Air {
  TemperatureKelvin temperature;
};

[[nodiscard]] inline Result<Air, QuantityError> makeAir(
    TemperatureKelvin temperature) {
  using R = Result<Air, QuantityError>;
  if (!std::isfinite(temperature.value()) || !(temperature.value() > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(Air{temperature});
}

// Dry-air speed of sound at the medium's temperature.
[[nodiscard]] inline Result<SpeedMetersPerSecond, QuantityError> speedOfSound(
    Air air) {
  using R = Result<SpeedMetersPerSecond, QuantityError>;
  const double t = air.temperature.value();
  if (!std::isfinite(t) || !(t > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double c = std::sqrt(acoustic_constants::dryAirGamma *
                             acoustic_constants::dryAirGasConstant * t);
  if (!std::isfinite(c)) return R::failure(QuantityError::RangeError);
  return R::success(SpeedMetersPerSecond{c});
}

[[nodiscard]] inline const char* mediumFormula() {
  return "c = sqrt(gamma * R * T), dry air";
}

}  // namespace mmathen
