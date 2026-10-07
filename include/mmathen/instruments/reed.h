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
// A cylindrical bore is a separate object in this header. Length,
// cross-section, and the air state are arguments. The far end is either
// a pressure release (reflection −1, fundamental c/(4L), input impedance
// j Zc tan(kL)) or a rigid wall (reflection +1, fundamental c/(2L),
// input impedance −j Zc cot(kL)). Zc = ρc/S. c is speedOfSound and ρ is
// airDensity. Poles of those impedances are RangeError. The caller
// supplies the outgoing pressure and the outgoing pressure from one
// round trip earlier; there is no stored time step. clarinet.h wires an
// open bore to this characteristic. This header stores no length.
//
// Reed inertia is also separate. Mass, tip area, closing displacement,
// and damping are arguments. Damping may be zero; it has no hidden
// default. Stiffness is the value that puts the static equilibrium on
// Dalmont's line, k = P_M A_tip / y_close, so it is not a second
// published constant. The linear acceleration is
// (A ΔP − k y − r v) / m. Equilibrium displacement is that line for
// 0 ≤ ΔP ≤ P_M and DomainError outside it, including ΔP < 0. The
// acceleration itself stays the spring past the lay: there is no
// contact solver. Opening against displacement follows the same
// straight line and is zero on and beyond the lay.
//
// Still not evaluated: a viscous channel (a Reynolds-number cutoff needs
// a viscosity and the channel width separately from S0), and any ratio
// at which the quasi-static assumption fails. Dalmont does not give that
// ratio, so none is invented. Tone holes, a bell, and a register key are
// not in this header. The bowed-string factories do not carry a reed, a
// bore, or a reed mass.
//
// VERIFIED against eqs. (1) and (5) and against the maximum at P_M/3.
// UNVALIDATED against a measured mouthpiece.

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "../physics/modes.h"
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
    "caller's static pressure and temperature. Negative dP is not this "
    "characteristic. A bore and reed inertia are separate arguments in "
    "this header. A viscous cutoff is not evaluated.";

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

inline constexpr const char* kReedBoreSource =
    "Cylindrical bore on the reed's air: length and cross-section are "
    "arguments. c = sqrt(gamma R T), rho = p/(R T), Zc = rho c / S. "
    "Open far end: pressure reflection -1, f1 = c/(4L), "
    "Z = j Zc tan(kL). Closed far end: pressure reflection +1, "
    "f1 = c/(2L), Z = -j Zc cot(kL). Poles are RangeError. The caller "
    "passes the delayed outgoing pressure; no time step is stored.";

// Far end of a cylinder. Open is a pressure release. Closed is a rigid
// wall. Both coefficients are boundary conditions, not fits.
enum class BoreFarEnd { Open, Closed };

// Length (m), cross-section (m²), and the static air state. No bore
// table is consulted.
struct ReedBoreSpec {
  double lengthMeters;
  double crossSectionM2;
  double airPressurePa;
  double temperatureKelvin;
  BoreFarEnd farEnd;
};

// Zc is the acoustic impedance ρc/S (Pa·s/m³). pressureReflection is −1
// or +1. fundamentalHz is c/(4L) when the far end is open and c/(2L)
// when it is closed. roundTripSeconds is 2L/c.
struct ReedBore {
  double lengthMeters = 0.0;
  double crossSectionM2 = 0.0;
  double soundSpeedMetersPerSecond = 0.0;
  double airDensityKgPerM3 = 0.0;
  double acousticImpedance = 0.0;
  double roundTripSeconds = 0.0;
  double fundamentalHz = 0.0;
  double pressureReflection = 0.0;
  BoreFarEnd farEnd = BoreFarEnd::Open;
  const char* source = nullptr;
};

// InvalidArgument: length or cross-section not strictly positive and
// finite, or farEnd is not Open or Closed. Air-density and sound-speed
// errors propagate. RangeError if Zc, the round trip, or the fundamental
// overflows.
[[nodiscard]] inline Result<ReedBore, QuantityError> makeReedBore(
    const ReedBoreSpec& spec) {
  using R = Result<ReedBore, QuantityError>;
  const double length = spec.lengthMeters;
  const double area = spec.crossSectionM2;
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(area) ||
      !(area > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (spec.farEnd != BoreFarEnd::Open && spec.farEnd != BoreFarEnd::Closed)
    return R::failure(QuantityError::InvalidArgument);
  auto density = airDensity(PressurePascals{spec.airPressurePa},
                            TemperatureKelvin{spec.temperatureKelvin});
  if (!density.ok()) return R::failure(density.error());
  auto air = makeAir(TemperatureKelvin{spec.temperatureKelvin});
  if (!air.ok()) return R::failure(air.error());
  auto speed = speedOfSound(air.value());
  if (!speed.ok()) return R::failure(speed.error());
  const double rho = density.value().value();
  const double c = speed.value().value();
  auto specific = characteristicImpedance(density.value(), speed.value());
  if (!specific.ok()) return R::failure(specific.error());
  const double impedance = specific.value().value() / area;
  const double roundTrip = 2.0 * length / c;
  const DistanceMeters boreLength{length};
  auto fundamental = spec.farEnd == BoreFarEnd::Open
                         ? stoppedPipeMode(boreLength, speed.value(), 1)
                         : openPipeMode(boreLength, speed.value(), 1);
  if (!fundamental.ok()) return R::failure(fundamental.error());
  if (!std::isfinite(impedance) || !(impedance > 0.0) ||
      !std::isfinite(roundTrip) || !(roundTrip > 0.0))
    return R::failure(QuantityError::RangeError);
  ReedBore bore;
  bore.lengthMeters = length;
  bore.crossSectionM2 = area;
  bore.soundSpeedMetersPerSecond = c;
  bore.airDensityKgPerM3 = rho;
  bore.acousticImpedance = impedance;
  bore.roundTripSeconds = roundTrip;
  bore.fundamentalHz = fundamental.value().value();
  bore.pressureReflection = spec.farEnd == BoreFarEnd::Open ? -1.0 : 1.0;
  bore.farEnd = spec.farEnd;
  bore.source = kReedBoreSource;
  return R::success(bore);
}

namespace reed_detail {

// Quarter-wave count 4 f L / c. An open far end poles on the odd counts
// (cos(kL) = 0). A closed far end poles on the even counts, including
// zero (sin(kL) = 0). The comparison is a few ulps, because c/(4L)
// evaluated in floating point is not a bitwise integer; it is not a
// musical tolerance and not a cutoff frequency.
[[nodiscard]] inline bool borePole(double quarterTurns, bool oddPole) {
  if (!std::isfinite(quarterTurns)) return true;
  const double nearest = std::round(quarterTurns);
  const double scale = std::max(1.0, std::fabs(quarterTurns));
  const double slack =
      16.0 * std::numeric_limits<double>::epsilon() * scale;
  if (!(std::fabs(quarterTurns - nearest) <= slack)) return false;
  const bool odd = std::fmod(std::fabs(nearest), 2.0) != 0.0;
  return oddPole ? odd : !odd;
}

}  // namespace reed_detail

// Input impedance (Pa·s/m³) at frequencyHz. Open: j Zc tan(kL). Closed:
// −j Zc cot(kL). Built as a pure-imaginary complex, not as a quotient of
// complexes. Frequency 0 is a short (Z = 0) for an open far end and a
// pole for a closed far end. A pole, or a non-finite tangent, is
// RangeError. A negative or non-finite frequency is InvalidArgument.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
boreInputImpedance(const ReedBore& bore, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(bore.lengthMeters) || !(bore.lengthMeters > 0.0) ||
      !std::isfinite(bore.soundSpeedMetersPerSecond) ||
      !(bore.soundSpeedMetersPerSecond > 0.0) ||
      !std::isfinite(bore.acousticImpedance) ||
      !(bore.acousticImpedance > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double kl = constants::twoPi * frequencyHz * bore.lengthMeters /
                    bore.soundSpeedMetersPerSecond;
  if (!std::isfinite(kl)) return R::failure(QuantityError::RangeError);
  const double quarterTurns = (2.0 * kl) / constants::pi;
  const bool open = bore.farEnd == BoreFarEnd::Open;
  if (reed_detail::borePole(quarterTurns, open))
    return R::failure(QuantityError::RangeError);
  double imaginary = 0.0;
  if (open) {
    const double tangent = std::tan(kl);
    if (!std::isfinite(tangent)) return R::failure(QuantityError::RangeError);
    imaginary = bore.acousticImpedance * tangent;
  } else {
    const double sine = std::sin(kl);
    if (!(std::fabs(sine) > 0.0)) return R::failure(QuantityError::RangeError);
    const double cotangent = std::cos(kl) / sine;
    if (!std::isfinite(cotangent))
      return R::failure(QuantityError::RangeError);
    imaginary = -bore.acousticImpedance * cotangent;
  }
  if (!std::isfinite(imaginary)) return R::failure(QuantityError::RangeError);
  return R::success(std::complex<double>{0.0, imaginary});
}

// Mouth state from an outgoing pressure and the outgoing pressure one
// round trip earlier, both supplied by the caller. p− = R p+_delayed,
// p = p+ + p−, and the volume flow into the bore is (p+ − p−) / Zc.
struct BoreWaveSplit {
  double outgoingPressurePa = 0.0;
  double incomingPressurePa = 0.0;
  double mouthPressurePa = 0.0;
  double volumeFlowM3PerS = 0.0;
};

// InvalidArgument: a pressure is non-finite, or Zc is not strictly
// positive. RangeError if a product overflows. The mouth-to-bore
// pressure difference that enters the reed characteristic is the
// caller's mouth pressure minus mouthPressurePa; this function does
// not invent either pressure.
[[nodiscard]] inline Result<BoreWaveSplit, QuantityError> boreWaveSplit(
    const ReedBore& bore, double outgoingPressurePa,
    double delayedOutgoingPressurePa) {
  using R = Result<BoreWaveSplit, QuantityError>;
  if (!std::isfinite(outgoingPressurePa) ||
      !std::isfinite(delayedOutgoingPressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(bore.acousticImpedance) ||
      !(bore.acousticImpedance > 0.0) ||
      !std::isfinite(bore.pressureReflection))
    return R::failure(QuantityError::InvalidArgument);
  const double incoming = bore.pressureReflection * delayedOutgoingPressurePa;
  const double pressure = outgoingPressurePa + incoming;
  const double flow =
      (outgoingPressurePa - incoming) / bore.acousticImpedance;
  if (!std::isfinite(incoming) || !std::isfinite(pressure) ||
      !std::isfinite(flow))
    return R::failure(QuantityError::RangeError);
  return R::success(
      BoreWaveSplit{outgoingPressurePa, incoming, pressure, flow});
}

inline constexpr const char* kReedInertiaSource =
    "Reed inertia on Dalmont's static line. Mass, tip area, closing "
    "displacement, and damping are arguments. "
    "k = P_M * A_tip / y_close, so the equilibrium y = y_close * dP / P_M "
    "has zero spring acceleration. Resonance is (1/(2 pi)) sqrt(k/m). "
    "No reed-mass table and no lay-contact impact.";

// Mass (kg), the area the pressure acts on (m²), the displacement that
// meets the lay (m), and damping (N·s/m). Damping is required; pass 0
// for an undamped reed. None of the four is read from a table.
struct ReedInertiaSpec {
  double massKg;
  double tipAreaM2;
  double closingDisplacementMeters;
  double dampingNewtonSecondsPerMeter;
};

// stiffnessNewtonsPerMeter is k = P_M A_tip / y_close, copied from the
// characteristic's closure pressure at construction. resonanceHz is the
// undamped natural frequency.
struct ReedInertia {
  double massKg = 0.0;
  double tipAreaM2 = 0.0;
  double closingDisplacementMeters = 0.0;
  double dampingNewtonSecondsPerMeter = 0.0;
  double stiffnessNewtonsPerMeter = 0.0;
  double closurePressurePa = 0.0;
  double resonanceHz = 0.0;
  const char* source = nullptr;
};

// InvalidArgument: mass, tip area, or closing displacement not strictly
// positive and finite; damping negative or non-finite; or the
// characteristic's closure pressure is not strictly positive.
// RangeError if k or the resonance overflows.
[[nodiscard]] inline Result<ReedInertia, QuantityError> makeReedInertia(
    const ReedCharacteristic& reed, const ReedInertiaSpec& spec) {
  using R = Result<ReedInertia, QuantityError>;
  const double mass = spec.massKg;
  const double tip = spec.tipAreaM2;
  const double closing = spec.closingDisplacementMeters;
  const double damping = spec.dampingNewtonSecondsPerMeter;
  const double closure = reed.closurePressurePa;
  if (!std::isfinite(mass) || !(mass > 0.0) || !std::isfinite(tip) ||
      !(tip > 0.0) || !std::isfinite(closing) || !(closing > 0.0) ||
      !std::isfinite(damping) || damping < 0.0 || !std::isfinite(closure) ||
      !(closure > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double stiffness = closure * tip / closing;
  const double resonance =
      std::sqrt(stiffness / mass) / constants::twoPi;
  if (!std::isfinite(stiffness) || !(stiffness > 0.0) ||
      !std::isfinite(resonance) || !(resonance > 0.0))
    return R::failure(QuantityError::RangeError);
  ReedInertia inertia;
  inertia.massKg = mass;
  inertia.tipAreaM2 = tip;
  inertia.closingDisplacementMeters = closing;
  inertia.dampingNewtonSecondsPerMeter = damping;
  inertia.stiffnessNewtonsPerMeter = stiffness;
  inertia.closurePressurePa = closure;
  inertia.resonanceHz = resonance;
  inertia.source = kReedInertiaSource;
  return R::success(inertia);
}

// Static opening on the lay, y = y_close ΔP / P_M. Defined for
// 0 ≤ ΔP ≤ P_M. ΔP < 0 and ΔP above closure are DomainError: the
// equilibrium is not extrapolated past the lay. Non-finite ΔP is
// InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> reedEquilibriumDisplacement(
    const ReedInertia& inertia, double pressureDifferencePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(pressureDifferencePa))
    return R::failure(QuantityError::InvalidArgument);
  if (pressureDifferencePa < 0.0 ||
      pressureDifferencePa > inertia.closurePressurePa)
    return R::failure(QuantityError::DomainError);
  if (!std::isfinite(inertia.closingDisplacementMeters) ||
      !(inertia.closingDisplacementMeters > 0.0) ||
      !std::isfinite(inertia.closurePressurePa) ||
      !(inertia.closurePressurePa > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double displacement = inertia.closingDisplacementMeters *
                              pressureDifferencePa / inertia.closurePressurePa;
  if (!std::isfinite(displacement)) return R::failure(QuantityError::RangeError);
  return R::success(displacement);
}

// Linear-spring acceleration (m/s²), (A ΔP − k y − r v) / m. This is
// the spring on either side of the lay. It does not switch to an impact
// when y passes y_close, and it accepts a negative ΔP. Non-finite
// inputs, or a mass or stiffness that is not strictly positive, are
// InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> reedTipAcceleration(
    const ReedInertia& inertia, double displacementMeters,
    double velocityMetersPerSecond, double pressureDifferencePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(displacementMeters) ||
      !std::isfinite(velocityMetersPerSecond) ||
      !std::isfinite(pressureDifferencePa))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(inertia.massKg) || !(inertia.massKg > 0.0) ||
      !std::isfinite(inertia.tipAreaM2) || !(inertia.tipAreaM2 > 0.0) ||
      !std::isfinite(inertia.stiffnessNewtonsPerMeter) ||
      !(inertia.stiffnessNewtonsPerMeter > 0.0) ||
      !std::isfinite(inertia.dampingNewtonSecondsPerMeter) ||
      inertia.dampingNewtonSecondsPerMeter < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double force = inertia.tipAreaM2 * pressureDifferencePa -
                       inertia.stiffnessNewtonsPerMeter * displacementMeters -
                       inertia.dampingNewtonSecondsPerMeter *
                           velocityMetersPerSecond;
  const double acceleration = force / inertia.massKg;
  if (!std::isfinite(acceleration)) return R::failure(QuantityError::RangeError);
  return R::success(acceleration);
}

// Jet area (m²) from reed displacement. On and beyond the lay, area is
// 0. Short of the lay, S = S0 (1 − y / y_close), which is S0 at y = 0
// and greater than S0 when y is negative: the same straight line, not a
// second measured opening. Non-finite displacement is InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> reedAreaAtDisplacement(
    const ReedCharacteristic& reed, const ReedInertia& inertia,
    double displacementMeters) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(displacementMeters))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(reed.equilibriumAreaM2) ||
      !(reed.equilibriumAreaM2 > 0.0) ||
      !std::isfinite(inertia.closingDisplacementMeters) ||
      !(inertia.closingDisplacementMeters > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (displacementMeters >= inertia.closingDisplacementMeters)
    return R::success(0.0);
  const double area =
      reed.equilibriumAreaM2 *
      (1.0 - displacementMeters / inertia.closingDisplacementMeters);
  if (!std::isfinite(area)) return R::failure(QuantityError::RangeError);
  return R::success(area);
}

}  // namespace mmathen
