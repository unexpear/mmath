#pragma once

// Trombone — the trumpet lip and cupped cylinder, on a U-slide (docs/48).
//
// Not a valve trumpet with a new name. The lip, the mouthpiece cup, and
// the open cylinder are the objects in trumpet.h. What changes is the
// air-column length. A handslide is two parallel legs joined by a bow.
// Extending the outer slide by x lengthens each leg by x, so
//
//   L = L0 + 2 x.
//
// L0 is the caller's air-column length with the slide closed. x is the
// handslide travel, zero when closed, and never negative. The two is
// the pair of legs, not a fitted end correction and not a position
// chart. No tenor length, no seven positions, and no F attachment are
// stored. An attachment is another tube, not this slide.
//
// At a fixed x the bore is that trumpet: odd quarter-waves
// (2n−1)c/(4L), throat impedance j Zc tan(kL), cup compliance
// C_a = V/(ρ c²) in parallel, and an outward-striking lip. Moving the
// slide does not change Zc, the cup, or the lip. It does change L, the
// round trip 2L/c, and the frequencies. The caller supplies the delayed
// outgoing pressure for whatever round trip the current slide has.
// No bell flare is applied.

#include "trumpet.h"

namespace mmathen {

inline constexpr const char* kTromboneSource =
    "Trombone: trumpet lip and cupped cylinder on a U-slide. "
    "Air column L = L0 + 2 x, where x is the handslide travel and the "
    "two counts the two legs. Modes (2n-1)c/(4L), throat impedance "
    "j Zc tan(kL), cup C_a = V/(rho c^2), outward-striking lip. "
    "L0, x, bore area, cup volume, and the lip parameters are arguments. "
    "No position chart, F attachment, bell flare, or stored length.";

// L0 and the handslide travel x. x may be zero. The remaining fields
// are the trumpet's bore area, air, cup, and lip.
struct TromboneSpec {
  double closedLengthMeters;
  double slideExtensionMeters;
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

// bore is the trumpet cylinder, cup, and lip at L = L0 + 2 x.
// closedLengthMeters and slideExtensionMeters are the arguments that
// produced bore.lengthMeters. bore.source remains the trumpet law.
struct Trombone {
  Trumpet bore;
  double closedLengthMeters = 0.0;
  double slideExtensionMeters = 0.0;
  const char* source = nullptr;
};

// L = L0 + 2 x. L0 must be strictly positive. x must be finite and
// >= 0. A non-finite sum is RangeError.
[[nodiscard]] inline Result<double, QuantityError> tromboneAirColumnMeters(
    double closedLengthMeters, double slideExtensionMeters) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(closedLengthMeters) || !(closedLengthMeters > 0.0) ||
      !std::isfinite(slideExtensionMeters) || slideExtensionMeters < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double length = closedLengthMeters + 2.0 * slideExtensionMeters;
  if (!std::isfinite(length) || !(length > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(length);
}

// InvalidArgument from a non-positive closed length, a negative slide
// extension, or from the trumpet built at L = L0 + 2 x. DomainError
// from a non-positive air state. RangeError if the air column or the
// trumpet overflows.
[[nodiscard]] inline Result<Trombone, QuantityError> makeTrombone(
    const TromboneSpec& spec) {
  using R = Result<Trombone, QuantityError>;
  auto length = tromboneAirColumnMeters(spec.closedLengthMeters,
                                        spec.slideExtensionMeters);
  if (!length.ok()) return R::failure(length.error());
  auto bore = makeTrumpet(TrumpetSpec{
      length.value(), spec.boreCrossSectionM2, spec.airPressurePa,
      spec.temperatureKelvin, spec.cupVolumeM3, spec.restOpeningAreaM2,
      spec.maximumOpeningAreaM2, spec.slitWidthMeters, spec.lipMassKg,
      spec.lipStiffnessNewtonsPerMeter, spec.lipPressureAreaM2,
      spec.lipDampingNewtonSecondsPerMeter});
  if (!bore.ok()) return R::failure(bore.error());
  if (bore.value().lengthMeters != length.value() ||
      bore.value().pressureReflection != -1.0)
    return R::failure(QuantityError::RangeError);
  Trombone trombone;
  trombone.bore = bore.value();
  trombone.closedLengthMeters = spec.closedLengthMeters;
  trombone.slideExtensionMeters = spec.slideExtensionMeters;
  trombone.source = kTromboneSource;
  return R::success(trombone);
}

// Same lip, cup, bore area, and air, with a new handslide travel.
// Zc and C_a are unchanged. L, the round trip, and the fundamental
// follow L0 + 2 x. A negative travel is InvalidArgument.
[[nodiscard]] inline Result<Trombone, QuantityError> tromboneAtSlide(
    const Trombone& trombone, double slideExtensionMeters) {
  using R = Result<Trombone, QuantityError>;
  auto length = tromboneAirColumnMeters(trombone.closedLengthMeters,
                                        slideExtensionMeters);
  if (!length.ok()) return R::failure(length.error());
  if (!std::isfinite(trombone.bore.soundSpeedMetersPerSecond) ||
      !(trombone.bore.soundSpeedMetersPerSecond > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto fundamental = stoppedPipeMode(
      DistanceMeters{length.value()},
      SpeedMetersPerSecond{trombone.bore.soundSpeedMetersPerSecond}, 1);
  if (!fundamental.ok()) return R::failure(fundamental.error());
  const double roundTrip =
      2.0 * length.value() / trombone.bore.soundSpeedMetersPerSecond;
  if (!std::isfinite(roundTrip) || !(roundTrip > 0.0))
    return R::failure(QuantityError::RangeError);
  Trombone moved = trombone;
  moved.slideExtensionMeters = slideExtensionMeters;
  moved.bore.lengthMeters = length.value();
  moved.bore.roundTripSeconds = roundTrip;
  moved.bore.fundamentalHz = fundamental.value().value();
  return R::success(moved);
}

// Odd quarter-wave of the current air column. Even n are DomainError.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> tromboneCylinderMode(
    const Trombone& trombone, int n) {
  return trumpetCylinderMode(trombone.bore, n);
}

[[nodiscard]] inline Result<std::complex<double>, QuantityError>
tromboneBoreImpedance(const Trombone& trombone, double frequencyHz) {
  return trumpetBoreImpedance(trombone.bore, frequencyHz);
}

[[nodiscard]] inline Result<std::complex<double>, QuantityError>
tromboneMouthpieceImpedance(const Trombone& trombone, double frequencyHz) {
  return trumpetMouthpieceImpedance(trombone.bore, frequencyHz);
}

[[nodiscard]] inline Result<double, QuantityError> tromboneLipOpening(
    const Trombone& trombone, double overpressurePa) {
  return trumpetLipOpening(trombone.bore, overpressurePa);
}

[[nodiscard]] inline Result<double, QuantityError> tromboneLipFlow(
    const Trombone& trombone, double overpressurePa) {
  return trumpetLipFlow(trombone.bore, overpressurePa);
}

[[nodiscard]] inline Result<double, QuantityError> tromboneLipEquilibrium(
    const Trombone& trombone, double overpressurePa) {
  return trumpetLipEquilibrium(trombone.bore, overpressurePa);
}

[[nodiscard]] inline Result<double, QuantityError> tromboneLipAcceleration(
    const Trombone& trombone, double displacementMeters,
    double velocityMetersPerSecond, double overpressurePa) {
  return trumpetLipAcceleration(trombone.bore, displacementMeters,
                                velocityMetersPerSecond, overpressurePa);
}

[[nodiscard]] inline Result<TrumpetWaveSplit, QuantityError> tromboneWaveSplit(
    const Trombone& trombone, double outgoingPressurePa,
    double delayedOutgoingPressurePa) {
  return trumpetWaveSplit(trombone.bore, outgoingPressurePa,
                          delayedOutgoingPressurePa);
}

[[nodiscard]] inline Result<TrumpetEmbouchure, QuantityError> tromboneEmbouchure(
    const Trombone& trombone, double overpressurePa, double outgoingPressurePa,
    double delayedOutgoingPressurePa, double displacementMeters,
    double velocityMetersPerSecond) {
  return trumpetEmbouchure(trombone.bore, overpressurePa, outgoingPressurePa,
                           delayedOutgoingPressurePa, displacementMeters,
                           velocityMetersPerSecond);
}

}  // namespace mmathen
