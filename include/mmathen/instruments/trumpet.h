#pragma once

// Trumpet — a lip-driven cylinder with a mouthpiece cup (docs/48).
//
// Not a clarinet and not a flute. The bare bore is a cylinder open at
// the bell, so the far-end reflection is −1 and the impedance at the
// throat is j Zc tan(kL). Its poles are the odd quarter-waves
// (2n−1)c/(4L). A flute uses the zeros of that impedance. A clarinet
// reed looks straight into those poles. A trumpet puts a mouthpiece cup
// in parallel with the bore, so the lips see
//   Z = 1 / (1/Z_bore + j ω C_a),  C_a = V / (ρ c²).
// At a bare-cylinder pole, Z_bore is infinite and Z falls to the cup
// alone, −j/(ω C_a), which is finite. The peaks move off the
// quarter-waves. No bell flare is applied: a flare is what pulls a
// brass series toward n c/(2L), and no flare law is stored here.
//
// The lip is an outward-striking valve, not Dalmont's closing lay.
// Blowing pressure pushes the lip open. With stiffness k, pressure area
// A, and slit width w, the static opening is
//   y = A ΔP / k,  S = min(S_max, S_0 + w y),
// and the volume flow is the Bernoulli jet S sqrt(2 ΔP / ρ). Area grows
// with ΔP until S_max. It does not peak at ΔP/3 and then shut. k is the
// player's embouchure stiffness. It is not P_M A / y_close. The
// acceleration (A ΔP − k y − r v) / m stays the linear spring past
// S_max: there is no contact solver.
//
// Length, bore area, cup volume, rest area, maximum area, slit width,
// lip mass, stiffness, pressure area, and damping are arguments. No
// trumpet length, valve, or mouthpiece catalog is stored. The lip flow
// and the bore flow are reported and are not solved into a note.
// trombone.h uses this lip and this cupped cylinder. Its length is a
// closed air column plus twice the handslide travel.

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "../physics/modes.h"

namespace mmathen {

inline constexpr const char* kTrumpetSource =
    "Trumpet: cylindrical bore open at the bell, f_n = (2n-1) c/(4L), "
    "throat impedance j Zc tan(kL). A mouthpiece cup of volume V sits in "
    "parallel, C_a = V/(rho c^2), so Z = 1/(1/Z_bore + j omega C_a). "
    "At a bare quarter-wave pole the cup leaves Z = -j/(omega C_a). "
    "The lip is outward-striking: y = A dP/k, S = min(S_max, S0 + w y), "
    "flow S sqrt(2 dP/rho). Stiffness is an argument, not a lay closure. "
    "No bell flare, valve, or mouthpiece catalog is stored.";

// Cup volume may be zero: that is the bare cylinder. Every other
// geometric and lip number is strictly positive. Damping may be zero.
// maximumOpeningAreaM2 is at least the rest area.
struct TrumpetSpec {
  double boreLengthMeters;
  double boreCrossSectionM2;
  double airPressurePa;
  double temperatureKelvin;
  double cupVolumeM3;
  double restOpeningAreaM2;
  double maximumOpeningAreaM2;
  double slitWidthMeters;
  double lipMassKg;
  double lipStiffnessNewtonsPerMeter;
  double lipPressureAreaM2;
  double lipDampingNewtonSecondsPerMeter;
};

// fundamentalHz is the bare cylinder's c/(4L). acousticCompliance is
// C_a = V/(ρ c²). lipResonanceHz is (1/2π) sqrt(k/m).
struct Trumpet {
  double lengthMeters = 0.0;
  double crossSectionM2 = 0.0;
  double soundSpeedMetersPerSecond = 0.0;
  double airDensityKgPerM3 = 0.0;
  double acousticImpedance = 0.0;
  double roundTripSeconds = 0.0;
  double fundamentalHz = 0.0;
  double pressureReflection = 0.0;
  double cupVolumeM3 = 0.0;
  double acousticCompliance = 0.0;
  double restOpeningAreaM2 = 0.0;
  double maximumOpeningAreaM2 = 0.0;
  double slitWidthMeters = 0.0;
  double lipMassKg = 0.0;
  double lipStiffnessNewtonsPerMeter = 0.0;
  double lipPressureAreaM2 = 0.0;
  double lipDampingNewtonSecondsPerMeter = 0.0;
  double lipResonanceHz = 0.0;
  const char* source = nullptr;
};

// InvalidArgument: a non-positive length, bore area, rest area, slit
// width, mass, stiffness, or pressure area; a maximum opening below the
// rest opening; a negative cup volume or damping; or a non-finite input.
// A zero cup is kept. Air errors propagate. RangeError if Zc, C_a, the
// round trip, the fundamental, or the lip resonance overflows.
[[nodiscard]] inline Result<Trumpet, QuantityError> makeTrumpet(
    const TrumpetSpec& spec) {
  using R = Result<Trumpet, QuantityError>;
  const double length = spec.boreLengthMeters;
  const double area = spec.boreCrossSectionM2;
  const double cup = spec.cupVolumeM3;
  const double rest = spec.restOpeningAreaM2;
  const double maximum = spec.maximumOpeningAreaM2;
  const double width = spec.slitWidthMeters;
  const double mass = spec.lipMassKg;
  const double stiffness = spec.lipStiffnessNewtonsPerMeter;
  const double lipArea = spec.lipPressureAreaM2;
  const double damping = spec.lipDampingNewtonSecondsPerMeter;
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(area) ||
      !(area > 0.0) || !std::isfinite(cup) || cup < 0.0 ||
      !std::isfinite(rest) || !(rest > 0.0) || !std::isfinite(maximum) ||
      maximum < rest || !std::isfinite(width) || !(width > 0.0) ||
      !std::isfinite(mass) || !(mass > 0.0) || !std::isfinite(stiffness) ||
      !(stiffness > 0.0) || !std::isfinite(lipArea) || !(lipArea > 0.0) ||
      !std::isfinite(damping) || damping < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  auto density = airDensity(PressurePascals{spec.airPressurePa},
                            TemperatureKelvin{spec.temperatureKelvin});
  if (!density.ok()) return R::failure(density.error());
  auto air = makeAir(TemperatureKelvin{spec.temperatureKelvin});
  if (!air.ok()) return R::failure(air.error());
  auto speed = speedOfSound(air.value());
  if (!speed.ok()) return R::failure(speed.error());
  auto specific = characteristicImpedance(density.value(), speed.value());
  if (!specific.ok()) return R::failure(specific.error());
  const double impedance = specific.value().value() / area;
  const double roundTrip = 2.0 * length / speed.value().value();
  auto fundamental =
      stoppedPipeMode(DistanceMeters{length}, speed.value(), 1);
  if (!fundamental.ok()) return R::failure(fundamental.error());
  const double compliance =
      cup / (density.value().value() * speed.value().value() *
             speed.value().value());
  const double resonance = std::sqrt(stiffness / mass) / constants::twoPi;
  if (!std::isfinite(impedance) || !(impedance > 0.0) ||
      !std::isfinite(roundTrip) || !(roundTrip > 0.0) ||
      !std::isfinite(compliance) || compliance < 0.0 ||
      !std::isfinite(resonance) || !(resonance > 0.0))
    return R::failure(QuantityError::RangeError);
  Trumpet trumpet;
  trumpet.lengthMeters = length;
  trumpet.crossSectionM2 = area;
  trumpet.soundSpeedMetersPerSecond = speed.value().value();
  trumpet.airDensityKgPerM3 = density.value().value();
  trumpet.acousticImpedance = impedance;
  trumpet.roundTripSeconds = roundTrip;
  trumpet.fundamentalHz = fundamental.value().value();
  trumpet.pressureReflection = -1.0;
  trumpet.cupVolumeM3 = cup;
  trumpet.acousticCompliance = compliance;
  trumpet.restOpeningAreaM2 = rest;
  trumpet.maximumOpeningAreaM2 = maximum;
  trumpet.slitWidthMeters = width;
  trumpet.lipMassKg = mass;
  trumpet.lipStiffnessNewtonsPerMeter = stiffness;
  trumpet.lipPressureAreaM2 = lipArea;
  trumpet.lipDampingNewtonSecondsPerMeter = damping;
  trumpet.lipResonanceHz = resonance;
  trumpet.source = kTrumpetSource;
  return R::success(trumpet);
}

// Odd quarter-wave of the bare cylinder, f_n = n c/(4L). Even n are
// DomainError. A bell would move these; this function does not.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> trumpetCylinderMode(
    const Trumpet& trumpet, int n) {
  return stoppedPipeMode(
      DistanceMeters{trumpet.lengthMeters},
      SpeedMetersPerSecond{trumpet.soundSpeedMetersPerSecond}, n);
}

namespace trumpet_detail {

[[nodiscard]] inline bool oddQuarterPole(double quarterTurns) {
  if (!std::isfinite(quarterTurns)) return true;
  const double nearest = std::round(quarterTurns);
  const double scale = std::max(1.0, std::fabs(quarterTurns));
  const double slack =
      16.0 * std::numeric_limits<double>::epsilon() * scale;
  if (!(std::fabs(quarterTurns - nearest) <= slack)) return false;
  return std::fmod(std::fabs(nearest), 2.0) != 0.0;
}

}  // namespace trumpet_detail

// Bare-bore impedance j Zc tan(kL). Poles, the odd quarter-waves, are
// RangeError. Built as a pure-imaginary complex. Frequency 0 is a short.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
trumpetBoreImpedance(const Trumpet& trumpet, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(trumpet.lengthMeters) || !(trumpet.lengthMeters > 0.0) ||
      !std::isfinite(trumpet.soundSpeedMetersPerSecond) ||
      !(trumpet.soundSpeedMetersPerSecond > 0.0) ||
      !std::isfinite(trumpet.acousticImpedance) ||
      !(trumpet.acousticImpedance > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double kl = constants::twoPi * frequencyHz * trumpet.lengthMeters /
                    trumpet.soundSpeedMetersPerSecond;
  if (!std::isfinite(kl)) return R::failure(QuantityError::RangeError);
  const double quarterTurns = (2.0 * kl) / constants::pi;
  if (trumpet_detail::oddQuarterPole(quarterTurns))
    return R::failure(QuantityError::RangeError);
  const double tangent = std::tan(kl);
  if (!std::isfinite(tangent)) return R::failure(QuantityError::RangeError);
  const double imaginary = trumpet.acousticImpedance * tangent;
  if (!std::isfinite(imaginary)) return R::failure(QuantityError::RangeError);
  return R::success(std::complex<double>{0.0, imaginary});
}

// Impedance the lip sees: the bore in parallel with the cup compliance.
// A zero cup returns the bare bore. At a bare quarter-wave pole a
// positive cup returns −j/(ω C_a). A zero of the parallel sum is
// RangeError. No complex division: the imaginary part is −1/D with
// D = ω C_a − 1/X and X = Zc tan(kL).
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
trumpetMouthpieceImpedance(const Trumpet& trumpet, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(trumpet.acousticCompliance) ||
      trumpet.acousticCompliance < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!(trumpet.acousticCompliance > 0.0))
    return trumpetBoreImpedance(trumpet, frequencyHz);
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(trumpet.lengthMeters) || !(trumpet.lengthMeters > 0.0) ||
      !std::isfinite(trumpet.soundSpeedMetersPerSecond) ||
      !(trumpet.soundSpeedMetersPerSecond > 0.0) ||
      !std::isfinite(trumpet.acousticImpedance) ||
      !(trumpet.acousticImpedance > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double omega = constants::twoPi * frequencyHz;
  const double kl = omega * trumpet.lengthMeters /
                    trumpet.soundSpeedMetersPerSecond;
  if (!std::isfinite(kl)) return R::failure(QuantityError::RangeError);
  const double quarterTurns = (2.0 * kl) / constants::pi;
  if (trumpet_detail::oddQuarterPole(quarterTurns)) {
    if (!(omega > 0.0)) return R::failure(QuantityError::RangeError);
    const double imaginary =
        -1.0 / (omega * trumpet.acousticCompliance);
    if (!std::isfinite(imaginary)) return R::failure(QuantityError::RangeError);
    return R::success(std::complex<double>{0.0, imaginary});
  }
  const double tangent = std::tan(kl);
  if (!std::isfinite(tangent)) return R::failure(QuantityError::RangeError);
  const double reactance = trumpet.acousticImpedance * tangent;
  if (!std::isfinite(reactance)) return R::failure(QuantityError::RangeError);
  if (reactance == 0.0) return R::success(std::complex<double>{0.0, 0.0});
  const double denominator =
      omega * trumpet.acousticCompliance - 1.0 / reactance;
  if (!std::isfinite(denominator) || denominator == 0.0)
    return R::failure(QuantityError::RangeError);
  const double imaginary = -1.0 / denominator;
  if (!std::isfinite(imaginary)) return R::failure(QuantityError::RangeError);
  return R::success(std::complex<double>{0.0, imaginary});
}

// Static lip opening (m²). ΔP = 0 is the rest area. Area grows as
// S_0 + w A ΔP / k until S_max. ΔP < 0 is DomainError.
[[nodiscard]] inline Result<double, QuantityError> trumpetLipOpening(
    const Trumpet& trumpet, double overpressurePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(overpressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (overpressurePa < 0.0) return R::failure(QuantityError::DomainError);
  if (!std::isfinite(trumpet.restOpeningAreaM2) ||
      !(trumpet.restOpeningAreaM2 > 0.0) ||
      !std::isfinite(trumpet.maximumOpeningAreaM2) ||
      trumpet.maximumOpeningAreaM2 < trumpet.restOpeningAreaM2 ||
      !std::isfinite(trumpet.slitWidthMeters) ||
      !(trumpet.slitWidthMeters > 0.0) ||
      !std::isfinite(trumpet.lipPressureAreaM2) ||
      !(trumpet.lipPressureAreaM2 > 0.0) ||
      !std::isfinite(trumpet.lipStiffnessNewtonsPerMeter) ||
      !(trumpet.lipStiffnessNewtonsPerMeter > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double displacement = trumpet.lipPressureAreaM2 * overpressurePa /
                              trumpet.lipStiffnessNewtonsPerMeter;
  const double grown =
      trumpet.restOpeningAreaM2 + trumpet.slitWidthMeters * displacement;
  if (!std::isfinite(grown)) return R::failure(QuantityError::RangeError);
  const double opening = grown < trumpet.maximumOpeningAreaM2
                             ? grown
                             : trumpet.maximumOpeningAreaM2;
  return R::success(opening);
}

// Bernoulli volume flow (m³/s) through that opening. Zero at ΔP = 0.
// The flow keeps growing after the area has reached S_max, because the
// jet speed still grows. There is no closure at which the flow returns
// to zero.
[[nodiscard]] inline Result<double, QuantityError> trumpetLipFlow(
    const Trumpet& trumpet, double overpressurePa) {
  using R = Result<double, QuantityError>;
  auto opening = trumpetLipOpening(trumpet, overpressurePa);
  if (!opening.ok()) return R::failure(opening.error());
  if (!std::isfinite(trumpet.airDensityKgPerM3) ||
      !(trumpet.airDensityKgPerM3 > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (overpressurePa == 0.0 || opening.value() == 0.0) return R::success(0.0);
  const double flow =
      opening.value() *
      std::sqrt(2.0 * overpressurePa / trumpet.airDensityKgPerM3);
  if (!std::isfinite(flow)) return R::failure(QuantityError::RangeError);
  return R::success(flow);
}

// Spring equilibrium (m), y = A ΔP / k. Not clamped at S_max.
// ΔP < 0 is DomainError.
[[nodiscard]] inline Result<double, QuantityError> trumpetLipEquilibrium(
    const Trumpet& trumpet, double overpressurePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(overpressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (overpressurePa < 0.0) return R::failure(QuantityError::DomainError);
  if (!std::isfinite(trumpet.lipPressureAreaM2) ||
      !(trumpet.lipPressureAreaM2 > 0.0) ||
      !std::isfinite(trumpet.lipStiffnessNewtonsPerMeter) ||
      !(trumpet.lipStiffnessNewtonsPerMeter > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double displacement = trumpet.lipPressureAreaM2 * overpressurePa /
                              trumpet.lipStiffnessNewtonsPerMeter;
  if (!std::isfinite(displacement)) return R::failure(QuantityError::RangeError);
  return R::success(displacement);
}

// Linear-spring acceleration (m/s²), (A ΔP − k y − r v) / m. A negative
// ΔP is accepted here. The spring does not stop at S_max.
[[nodiscard]] inline Result<double, QuantityError> trumpetLipAcceleration(
    const Trumpet& trumpet, double displacementMeters,
    double velocityMetersPerSecond, double overpressurePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(displacementMeters) ||
      !std::isfinite(velocityMetersPerSecond) ||
      !std::isfinite(overpressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(trumpet.lipMassKg) || !(trumpet.lipMassKg > 0.0) ||
      !std::isfinite(trumpet.lipPressureAreaM2) ||
      !(trumpet.lipPressureAreaM2 > 0.0) ||
      !std::isfinite(trumpet.lipStiffnessNewtonsPerMeter) ||
      !(trumpet.lipStiffnessNewtonsPerMeter > 0.0) ||
      !std::isfinite(trumpet.lipDampingNewtonSecondsPerMeter) ||
      trumpet.lipDampingNewtonSecondsPerMeter < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double force =
      trumpet.lipPressureAreaM2 * overpressurePa -
      trumpet.lipStiffnessNewtonsPerMeter * displacementMeters -
      trumpet.lipDampingNewtonSecondsPerMeter * velocityMetersPerSecond;
  const double acceleration = force / trumpet.lipMassKg;
  if (!std::isfinite(acceleration)) return R::failure(QuantityError::RangeError);
  return R::success(acceleration);
}

// Far-end reflection −1. p− = −p+_delayed. Flow into the bore is
// (p+ − p−) / Zc. The cup is not in this split.
struct TrumpetWaveSplit {
  double outgoingPressurePa = 0.0;
  double incomingPressurePa = 0.0;
  double mouthPressurePa = 0.0;
  double volumeFlowM3PerS = 0.0;
};

[[nodiscard]] inline Result<TrumpetWaveSplit, QuantityError> trumpetWaveSplit(
    const Trumpet& trumpet, double outgoingPressurePa,
    double delayedOutgoingPressurePa) {
  using R = Result<TrumpetWaveSplit, QuantityError>;
  if (!std::isfinite(outgoingPressurePa) ||
      !std::isfinite(delayedOutgoingPressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(trumpet.acousticImpedance) ||
      !(trumpet.acousticImpedance > 0.0) ||
      trumpet.pressureReflection != -1.0)
    return R::failure(QuantityError::InvalidArgument);
  const double incoming =
      trumpet.pressureReflection * delayedOutgoingPressurePa;
  const double pressure = outgoingPressurePa + incoming;
  const double flow =
      (outgoingPressurePa - incoming) / trumpet.acousticImpedance;
  if (!std::isfinite(incoming) || !std::isfinite(pressure) ||
      !std::isfinite(flow))
    return R::failure(QuantityError::RangeError);
  return R::success(
      TrumpetWaveSplit{outgoingPressurePa, incoming, pressure, flow});
}

// One lip sample. Lip flow and bore flow are not required to match.
struct TrumpetEmbouchure {
  double lipOpeningAreaM2 = 0.0;
  double lipFlowM3PerS = 0.0;
  double boreFlowM3PerS = 0.0;
  double equilibriumDisplacementMeters = 0.0;
  double lipAccelerationMetersPerSecondSquared = 0.0;
};

// ΔP < 0 is DomainError. ΔP = 0 is a lip at rest: opening S_0, flow 0.
[[nodiscard]] inline Result<TrumpetEmbouchure, QuantityError> trumpetEmbouchure(
    const Trumpet& trumpet, double overpressurePa, double outgoingPressurePa,
    double delayedOutgoingPressurePa, double displacementMeters,
    double velocityMetersPerSecond) {
  using R = Result<TrumpetEmbouchure, QuantityError>;
  auto split = trumpetWaveSplit(trumpet, outgoingPressurePa,
                                delayedOutgoingPressurePa);
  if (!split.ok()) return R::failure(split.error());
  auto opening = trumpetLipOpening(trumpet, overpressurePa);
  if (!opening.ok()) return R::failure(opening.error());
  auto flow = trumpetLipFlow(trumpet, overpressurePa);
  if (!flow.ok()) return R::failure(flow.error());
  auto equilibrium = trumpetLipEquilibrium(trumpet, overpressurePa);
  if (!equilibrium.ok()) return R::failure(equilibrium.error());
  auto acceleration = trumpetLipAcceleration(
      trumpet, displacementMeters, velocityMetersPerSecond, overpressurePa);
  if (!acceleration.ok()) return R::failure(acceleration.error());
  TrumpetEmbouchure sample;
  sample.lipOpeningAreaM2 = opening.value();
  sample.lipFlowM3PerS = flow.value();
  sample.boreFlowM3PerS = split.value().volumeFlowM3PerS;
  sample.equilibriumDisplacementMeters = equilibrium.value();
  sample.lipAccelerationMetersPerSecondSquared = acceleration.value();
  return R::success(sample);
}

}  // namespace mmathen
