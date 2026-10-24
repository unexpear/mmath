#pragma once

// Clarinet — one open cylinder, Dalmont's slit, and the reed's inertia
// (docs/48).
//
// Not a tone-hole lattice, not a bell, and not a register key. The bore
// is the cylinder already in reed.h, with the far end fixed open: a
// clarinet bell is a pressure release, so the reflection is −1, the
// fundamental is c/(4L), and the next resonance is 3c/(4L). A closed far
// end is a different pipe and is not offered. Even multiples of the
// fundamental are finite impedances, not poles.
//
// Length, cross-section, effective jet area S0, closure pressure P_M,
// reed mass, tip area, closing displacement, and damping are arguments.
// Damping may be zero. Dalmont, Gilbert, and Ollivier, JASA 114(4)
// (2003), Table 1, summarise embouchure ranges. That table does not print
// a bore length or one jet area, so no midpoint of it is stored.
//
// The characteristic, the bore, and the inertia share one static pressure
// and temperature, so the reed density and the bore density are the same
// ideal-gas value. clarinetMouth is one instant. The caller supplies the
// mouth pressure, the outgoing bore pressure, the outgoing pressure from
// one round trip earlier, and the reed's displacement and velocity. ΔP is
// the mouth pressure minus the bore mouth pressure. Reed flow, bore flow,
// equilibrium displacement, and tip acceleration are reported side by
// side. They are not iterated into a matched note, and no time step is
// stored.
//
// The twelfth, 3c/(4L), is the cylinder's next impedance pole. It is not
// a speaker hole. A viscous channel is not evaluated.

#include "reed.h"

namespace mmathen {

inline constexpr const char* kClarinetSource =
    "Clarinet cylinder: Dalmont, Gilbert, and Ollivier quasi-static "
    "characteristic (JASA 114(4), 2003, eqs. (1) and (5)), an open bore "
    "f1 = c/(4L), and reed inertia on that characteristic. Length, "
    "cross-section, S0, P_M, mass, tip area, closing displacement, and "
    "damping are arguments. The far end is open. Dalmont's Table 1 is an "
    "embouchure range and does not print a bore length or a jet area, so "
    "it is not stored. No tone hole, bell correction, register key, or "
    "viscous channel is stored. The next resonance 3c/(4L) is the "
    "cylinder's next impedance pole, not a speaker hole.";

// Every geometric and reed number is an argument. There is no far-end
// field: the bell of this cylinder is open.
struct ClarinetSpec {
  double boreLengthMeters;
  double boreCrossSectionM2;
  double airPressurePa;
  double temperatureKelvin;
  double equilibriumAreaM2;
  double closurePressurePa;
  double reedMassKg;
  double tipAreaM2;
  double closingDisplacementMeters;
  double dampingNewtonSecondsPerMeter;
};

// reed, bore, and inertia are the existing objects. bore.farEnd is Open
// and bore.pressureReflection is −1. source names the assembly and the
// gaps. No second length is stored beside bore.lengthMeters.
struct Clarinet {
  ReedCharacteristic reed;
  ReedBore bore;
  ReedInertia inertia;
  const char* source = nullptr;
};

// InvalidArgument and DomainError propagate from the characteristic, the
// open bore, and the inertia: a non-positive length, area, S0, P_M, mass,
// tip area, or closing displacement; a negative damping; a non-positive
// air state. RangeError if a derived peak flow, impedance, stiffness, or
// resonance overflows, or if the three objects do not share one density
// and one closure pressure.
[[nodiscard]] inline Result<Clarinet, QuantityError> makeClarinet(
    const ClarinetSpec& spec) {
  using R = Result<Clarinet, QuantityError>;
  auto reed = makeReedCharacteristic(ReedSpec{
      spec.equilibriumAreaM2, spec.closurePressurePa, spec.airPressurePa,
      spec.temperatureKelvin});
  if (!reed.ok()) return R::failure(reed.error());
  auto bore = makeReedBore(ReedBoreSpec{
      spec.boreLengthMeters, spec.boreCrossSectionM2, spec.airPressurePa,
      spec.temperatureKelvin, BoreFarEnd::Open});
  if (!bore.ok()) return R::failure(bore.error());
  auto inertia = makeReedInertia(
      reed.value(),
      ReedInertiaSpec{spec.reedMassKg, spec.tipAreaM2,
                      spec.closingDisplacementMeters,
                      spec.dampingNewtonSecondsPerMeter});
  if (!inertia.ok()) return R::failure(inertia.error());
  if (reed.value().airDensityKgPerM3 != bore.value().airDensityKgPerM3 ||
      inertia.value().closurePressurePa != reed.value().closurePressurePa ||
      bore.value().farEnd != BoreFarEnd::Open ||
      bore.value().pressureReflection != -1.0)
    return R::failure(QuantityError::RangeError);
  Clarinet clarinet;
  clarinet.reed = reed.value();
  clarinet.bore = bore.value();
  clarinet.inertia = inertia.value();
  clarinet.source = kClarinetSource;
  return R::success(clarinet);
}

// Odd quarter-wave of this open cylinder, f_n = n c/(4L). n = 1 is the
// bore fundamental. n = 3 is the next pole, the twelfth. Even n are
// DomainError: they are not resonances of an open cylinder. This is
// stoppedPipeMode on the built length and sound speed. It does not open
// a register hole.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> clarinetCylinderMode(
    const Clarinet& clarinet, int n) {
  return stoppedPipeMode(
      DistanceMeters{clarinet.bore.lengthMeters},
      SpeedMetersPerSecond{clarinet.bore.soundSpeedMetersPerSecond}, n);
}

// One mouth sample. Flows are not required to match, and nothing is
// stepped forward.
struct ClarinetMouth {
  double pressureDifferencePa = 0.0;
  double reedFlowM3PerS = 0.0;
  double boreFlowM3PerS = 0.0;
  double equilibriumDisplacementMeters = 0.0;
  double tipAccelerationMetersPerSecondSquared = 0.0;
};

// ΔP = mouthPressurePa − bore mouth pressure. Reed flow is the Bernoulli
// jet. Bore flow is (p+ − p−) / Zc. Equilibrium is the static line, so
// ΔP outside [0, P_M], including ΔP < 0, is DomainError. Tip acceleration
// is the linear spring at the caller's displacement and velocity, which
// may lie past the lay while ΔP is still on the characteristic.
// InvalidArgument: a pressure or a reed state is non-finite, or the bore
// impedance is not strictly positive. RangeError if a product overflows.
[[nodiscard]] inline Result<ClarinetMouth, QuantityError> clarinetMouth(
    const Clarinet& clarinet, double mouthPressurePa,
    double outgoingPressurePa, double delayedOutgoingPressurePa,
    double displacementMeters, double velocityMetersPerSecond) {
  using R = Result<ClarinetMouth, QuantityError>;
  auto split = boreWaveSplit(clarinet.bore, outgoingPressurePa,
                             delayedOutgoingPressurePa);
  if (!split.ok()) return R::failure(split.error());
  const double difference = mouthPressurePa - split.value().mouthPressurePa;
  auto flow = reedVolumeFlow(clarinet.reed, difference);
  if (!flow.ok()) return R::failure(flow.error());
  auto equilibrium =
      reedEquilibriumDisplacement(clarinet.inertia, difference);
  if (!equilibrium.ok()) return R::failure(equilibrium.error());
  auto acceleration = reedTipAcceleration(
      clarinet.inertia, displacementMeters, velocityMetersPerSecond,
      difference);
  if (!acceleration.ok()) return R::failure(acceleration.error());
  ClarinetMouth mouth;
  mouth.pressureDifferencePa = difference;
  mouth.reedFlowM3PerS = flow.value();
  mouth.boreFlowM3PerS = split.value().volumeFlowM3PerS;
  mouth.equilibriumDisplacementMeters = equilibrium.value();
  mouth.tipAccelerationMetersPerSecondSquared = acceleration.value();
  return R::success(mouth);
}

}  // namespace mmathen
