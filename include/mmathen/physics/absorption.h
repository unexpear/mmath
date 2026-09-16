#pragma once

// D2 — atmospheric absorption (ISO 9613-1:1993 analytical method).
//
// PROVENANCE: the ISO primary is paywalled (CHF 135); this implements the
// equations as published in the open python-acoustics implementation
// (BSD-3, acoustics/standards/iso_9613_1_1993.py), which itself transcribes
// the standard, cross-checked here against ISO Table 1 spot values (see
// tests) and Harris 1966 / Smith (CCRMA) measurements. Any deviation found
// later against the primary belongs to the transcription, and the tests
// below will catch it: that is what they are for.
//
// Method: pure-tone attenuation coefficient alpha(f, T, humidity, pressure)
// in dB/m. Scope (standard's stated scope, enforced): f in (0, 20 kHz]
// (formulae exist wider per the standard), T in [-20, +50] C, relative
// humidity in [0, 1], positive finite pressure (standard scope is 1 atm).
//
// CRITICAL UNIT RULE: the standard's molar water-vapour concentration h is
// in MOLE PERCENT, not fraction. RelativeHumidity is stored as a fraction
// (never percent — see physical_units.h); the x100 conversion happens once,
// inside molarConcentration(), loudly documented. Getting this wrong
// compiles, runs, and silently shifts every relaxation frequency.

#include <cmath>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

namespace acoustic_constants {
inline constexpr double isoReferenceTemperatureK = 293.15;
inline constexpr double isoReferencePressureKPa = 101.325;
inline constexpr double waterTripleTemperatureK = 273.16;
}  // namespace acoustic_constants

namespace detail {
// ISO stated scope -20..+50 C, with a 1e-9 K tolerance band: the boundary
// is legitimately reached through arithmetic like -20.0 + 273.15, which need
// not round-trip to the literal 253.15 exactly. 1e-9 K is nine orders below
// any physical significance; anything further out is rejected as
// out-of-scope. NaN compares false on both sides, so it is rejected too.
inline bool inIsoTemperatureScope(double tKelvin) {
  return tKelvin >= 253.15 - 1e-9 && tKelvin <= 323.15 + 1e-9;
}
}  // namespace detail

struct HumidAir {
  TemperatureKelvin temperature;  // restricted to ISO scope at creation
  RelativeHumidity humidity;      // fraction in [0,1]
  PressurePascals pressure;       // > 0 (standard scope is 1 atm)
};

[[nodiscard]] inline Result<HumidAir, QuantityError> makeHumidAir(
    TemperatureKelvin temperature, RelativeHumidity humidity,
    PressurePascals pressure) {
  using R = Result<HumidAir, QuantityError>;
  const double t = temperature.value();
  const double p = pressure.value();
  if (!detail::inIsoTemperatureScope(t))
    return R::failure(QuantityError::InvalidArgument);  // ISO -20..+50 C
  if (!std::isfinite(humidity.value()) || humidity.value() < 0.0 ||
      humidity.value() > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(p) || !(p > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(HumidAir{temperature, humidity, pressure});
}

// Saturation vapour pressure: p_sat = p_r * 10^C,
// C = -6.8346*(T_01/T)^1.261 + 4.6151 (ISO 9613-1).
[[nodiscard]] inline Result<PressurePascals, QuantityError>
saturationVapourPressure(TemperatureKelvin temperature) {
  using R = Result<PressurePascals, QuantityError>;
  const double t = temperature.value();
  if (!detail::inIsoTemperatureScope(t))
    return R::failure(QuantityError::InvalidArgument);
  const double c =
      -6.8346 *
          std::pow(acoustic_constants::waterTripleTemperatureK / t, 1.261) +
      4.6151;
  const double psat =
      acoustic_constants::isoReferencePressureKPa * std::pow(10.0, c) * 1000.0;
  if (!std::isfinite(psat) || !(psat > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(PressurePascals{psat});
}

// Molar water-vapour concentration in MOLE PERCENT:
// h = 100 * h_r * p_sat / p_a. The x100 is the standard's unit, not a fudge.
[[nodiscard]] inline Result<double, QuantityError> molarConcentration(
    const HumidAir& air) {
  using R = Result<double, QuantityError>;
  auto psat = saturationVapourPressure(air.temperature);
  if (!psat.ok()) return R::failure(psat.error());
  const double h = 100.0 * air.humidity.value() * psat.value().value() /
                   air.pressure.value();
  if (!std::isfinite(h) || h < 0.0)
    return R::failure(QuantityError::RangeError);
  return R::success(h);
}

// Oxygen relaxation frequency:
// f_r,O = (p_a/p_r) * (24 + 4.04e4*h*(0.02+h)/(0.391+h)).
[[nodiscard]] inline Result<FrequencyHz, QuantityError>
relaxationFrequencyOxygen(const HumidAir& air) {
  using R = Result<FrequencyHz, QuantityError>;
  auto h = molarConcentration(air);
  if (!h.ok()) return R::failure(h.error());
  const double hv = h.value();
  const double pratio =
      air.pressure.value() / 1000.0 / acoustic_constants::isoReferencePressureKPa;
  const double fr =
      pratio * (24.0 + 4.04e4 * hv * (0.02 + hv) / (0.391 + hv));
  if (!std::isfinite(fr) || !(fr > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{fr});
}

// Nitrogen relaxation frequency:
// f_r,N = (p_a/p_r)*(T/T_0)^(-1/2) * (9 + 280*h*exp(-4.170*((T/T_0)^(-1/3)-1))).
[[nodiscard]] inline Result<FrequencyHz, QuantityError>
relaxationFrequencyNitrogen(const HumidAir& air) {
  using R = Result<FrequencyHz, QuantityError>;
  auto h = molarConcentration(air);
  if (!h.ok()) return R::failure(h.error());
  const double t = air.temperature.value();
  const double tRatio = t / acoustic_constants::isoReferenceTemperatureK;
  const double pratio =
      air.pressure.value() / 1000.0 / acoustic_constants::isoReferencePressureKPa;
  const double fr =
      pratio * std::pow(tRatio, -0.5) *
      (9.0 + 280.0 * h.value() *
                 std::exp(-4.170 * (std::pow(tRatio, -1.0 / 3.0) - 1.0)));
  if (!std::isfinite(fr) || !(fr > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{fr});
}

// Pure-tone attenuation coefficient in dB/m:
// alpha = 8.686*f^2 * [1.84e-11*(p_r/p_a)*(T/T_0)^0.5 + (T/T_0)^-2.5 *
//   (0.01275*e^(-2239.1/T)/(f_r,O + f^2/f_r,O)
//    + 0.1068*e^(-3352/T)/(f_r,N + f^2/f_r,N))].
// Frequency in (0, 20 kHz]; validated against ISO Table 1 spot values in
// tests (2% relative: table precision plus 1993-era computation).
[[nodiscard]] inline Result<AttenuationDbPerM, QuantityError>
attenuationCoefficient(const HumidAir& air, FrequencyHz frequency) {
  using R = Result<AttenuationDbPerM, QuantityError>;
  const double f = frequency.value();
  if (!std::isfinite(f) || !(f > 0.0) || f > 20000.0)
    return R::failure(QuantityError::InvalidArgument);
  auto frO = relaxationFrequencyOxygen(air);
  if (!frO.ok()) return R::failure(frO.error());
  auto frN = relaxationFrequencyNitrogen(air);
  if (!frN.ok()) return R::failure(frN.error());
  const double t = air.temperature.value();
  const double tRatio = t / acoustic_constants::isoReferenceTemperatureK;
  const double pRatio =
      (air.pressure.value() / 1000.0) / acoustic_constants::isoReferencePressureKPa;
  const double fO = frO.value().value();
  const double fN = frN.value().value();
  const double alpha =
      8.686 * f * f *
      (1.84e-11 * (1.0 / pRatio) * std::sqrt(tRatio) +
       std::pow(tRatio, -2.5) *
           (0.01275 * std::exp(-2239.1 / t) / (fO + f * f / fO) +
            0.1068 * std::exp(-3352.0 / t) / (fN + f * f / fN)));
  if (!std::isfinite(alpha) || alpha < 0.0)
    return R::failure(QuantityError::RangeError);
  return R::success(AttenuationDbPerM{alpha});
}

// Total absorption over a distance: A = alpha * d, in dB. Distance must be
// finite and >= 0.
[[nodiscard]] inline Result<Decibels, QuantityError> attenuationOverDistance(
    AttenuationDbPerM alpha, DistanceMeters distance) {
  using R = Result<Decibels, QuantityError>;
  const double d = distance.value();
  if (!std::isfinite(d) || d < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double a = alpha.value() * d;
  if (!std::isfinite(a)) return R::failure(QuantityError::RangeError);
  return R::success(Decibels{a});
}

}  // namespace mmathen
