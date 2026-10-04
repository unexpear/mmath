#pragma once

// Reed — quasi-static single-reed characteristic (docs/48).
//
// Not a bore, not a note, and not a string. Dalmont, Gilbert, and
// Ollivier, JASA 114(4), 2253–2262 (2003), eqs. (1) and (5)
// (https://hal.science/hal-00474990), following Wilson and Beavers (1974)
// and Fletcher (1979): the reed is a spring-loaded slit. Its effective jet
// area falls linearly with the mouth-to-mouthpiece pressure difference
// ΔP until the lay closes it,
//   S(ΔP) = S0 (1 - ΔP / P_M)    for 0 ≤ ΔP ≤ P_M,
//   S(ΔP) = 0                    for ΔP > P_M,
// and the volume flow is Bernoulli's jet,
//   U = S sqrt(2 ΔP / ρ).
// S0 is the effective jet area at ΔP = 0. A vena-contracta coefficient is
// not applied on top of S0: Dalmont measures that coefficient and it is
// not a universal constant, so it stays inside the caller's area.
//
// Air density is the engine's ideal-gas value ρ = p / (R T) at the
// caller's static pressure and temperature (impedance.h). No 1.2 kg/m³
// is stored.
//
// The flow peaks at ΔP = P_M / 3,
//   U_max = (2/3) S0 sqrt(2 P_M / (3 ρ)).
// Equation (5) is stated for ΔP ≥ 0. A negative difference (mouthpiece
// pressure above mouth pressure) is DomainError: reverse flow is not this
// characteristic.
//
// Not evaluated, because each one needs a parameter or a summation this
// characteristic does not have:
//   self-sustained oscillation and playing frequency (a bore reflection
//   function, or a length and a cross-section; none is invented, and none
//   is read off a violin, viola, cello, or double bass),
//   reed inertia and damping (a reed mass and a time step; the paper's
//   quasi-static use assumes the playing frequency is small beside the
//   reed resonance, and it does not give a numerical ratio at which to
//   refuse, so no such ratio is invented),
//   the viscous channel (a Reynolds-number cutoff needs a viscosity and
//   the channel width separately from S0).
// The bowed-string factories do not carry a reed.
//
// VERIFIED against eqs. (1) and (5) and against the maximum at P_M/3.
// UNVALIDATED against a measured mouthpiece.

#include <cmath>

#include "../physics/impedance.h"
#include "../units/physical_units.h"

namespace mmathen {

inline constexpr const char* kReedCharacteristicSource =
    "Quasi-static single reed: J.-P. Dalmont, J. Gilbert, and S. Ollivier, "
    "\"Nonlinear characteristics of single-reed instruments: Quasistatic "
    "volume flow and reed opening measurements\", J. Acoust. Soc. Am. "
    "114(4), 2253-2262 (2003), eqs. (1) and (5), "
    "https://hal.science/hal-00474990 . "
    "S(dP) = S0 (1 - dP/P_M) until closure, else 0; "
    "U = S sqrt(2 dP/rho); maximum at dP = P_M/3. "
    "S0 is the effective jet area. Density is the ideal-gas value at the "
    "caller's static pressure and temperature. Negative dP, a bore, reed "
    "inertia, and a viscous cutoff are not evaluated.";

// S0 in square metres, closure pressure P_M in pascals, and the static
// air state that fixes ρ. All four are arguments.
struct ReedSpec {
  double equilibriumAreaM2;
  double closurePressurePa;
  double airPressurePa;
  double temperatureKelvin;
};

// Built characteristic. maximumFlowM3PerS is U at P_M/3.
struct ReedCharacteristic {
  double equilibriumAreaM2 = 0.0;
  double closurePressurePa = 0.0;
  double airDensityKgPerM3 = 0.0;
  double maximumFlowM3PerS = 0.0;
  const char* source = nullptr;
};

// InvalidArgument: area or closure pressure not strictly positive and
// finite. The air-density errors propagate: non-finite pressure or
// temperature is InvalidArgument, and a non-positive one is DomainError.
// RangeError if the peak flow overflows.
[[nodiscard]] inline Result<ReedCharacteristic, QuantityError>
makeReedCharacteristic(const ReedSpec& spec) {
  using R = Result<ReedCharacteristic, QuantityError>;
  const double area = spec.equilibriumAreaM2;
  const double closure = spec.closurePressurePa;
  if (!std::isfinite(area) || !(area > 0.0) || !std::isfinite(closure) ||
      !(closure > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto density = airDensity(PressurePascals{spec.airPressurePa},
                            TemperatureKelvin{spec.temperatureKelvin});
  if (!density.ok()) return R::failure(density.error());
  const double rho = density.value().value();
  const double peak =
      (2.0 / 3.0) * area * std::sqrt(2.0 * closure / (3.0 * rho));
  if (!std::isfinite(rho) || !(rho > 0.0) || !std::isfinite(peak) ||
      !(peak > 0.0))
    return R::failure(QuantityError::RangeError);
  ReedCharacteristic reed;
  reed.equilibriumAreaM2 = area;
  reed.closurePressurePa = closure;
  reed.airDensityKgPerM3 = rho;
  reed.maximumFlowM3PerS = peak;
  reed.source = kReedCharacteristicSource;
  return R::success(reed);
}

// Effective jet area (m²). 0 ≤ ΔP ≤ P_M is the linear slit. ΔP > P_M is
// the reed on the lay, area 0. ΔP < 0 is DomainError.
[[nodiscard]] inline Result<double, QuantityError> reedOpeningArea(
    const ReedCharacteristic& reed, double pressureDifferencePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(pressureDifferencePa))
    return R::failure(QuantityError::InvalidArgument);
  if (pressureDifferencePa < 0.0) return R::failure(QuantityError::DomainError);
  if (pressureDifferencePa >= reed.closurePressurePa) return R::success(0.0);
  const double area =
      reed.equilibriumAreaM2 *
      (1.0 - pressureDifferencePa / reed.closurePressurePa);
  if (!std::isfinite(area)) return R::failure(QuantityError::RangeError);
  return R::success(area);
}

// Volume flow (m³/s) from eq. (5). Zero at ΔP = 0, at closure, and while
// the reed is shut. The peak is reed.maximumFlowM3PerS at P_M/3.
[[nodiscard]] inline Result<double, QuantityError> reedVolumeFlow(
    const ReedCharacteristic& reed, double pressureDifferencePa) {
  using R = Result<double, QuantityError>;
  auto opening = reedOpeningArea(reed, pressureDifferencePa);
  if (!opening.ok()) return R::failure(opening.error());
  if (opening.value() == 0.0 || pressureDifferencePa == 0.0)
    return R::success(0.0);
  const double flow =
      opening.value() *
      std::sqrt(2.0 * pressureDifferencePa / reed.airDensityKgPerM3);
  if (!std::isfinite(flow)) return R::failure(QuantityError::RangeError);
  return R::success(flow);
}

}  // namespace mmathen
