#pragma once

// Flute — an open-open cylinder and an embouchure jet (docs/48).
//
// Not a clarinet. A clarinet is a quarter-wave pipe: the reed end is
// driven at high impedance and the bell is open, so the resonances are
// the poles of j Zc tan(kL), at (2n−1)c/(4L). A flute is open at the
// embouchure and open at the far end. Its resonances are the zeros of
// that same bore impedance, f_n = n c/(2L), every positive integer,
// including the octave. The far-end pressure reflection is −1. No end
// correction is added; a caller who wants one puts it inside L.
//
// The drive is not a reed. Blowing overpressure ΔP accelerates an ideal
// jet, U = sqrt(2 ΔP / ρ), with ρ = p/(R T) from the air already in
// physics/. The jet area S_j is not the bore area. There is no closure
// pressure and no reed mass.
//
// A disturbance on the jet convects at u = σ U across the flue-to-edge
// distance W. σ is the caller's ratio of wave speed to centerline speed.
// It is not a stored Coltman or Fletcher fraction: those measurements
// are not one constant. Regime m is the travel time of an odd number of
// half periods,
//   f_m = (2m−1) σ U / (2 W),
// the kinematic edge-tone condition. These frequencies are not the pipe
// modes, and they are not solved against the pipe into a sounding note.
//
// The jet is a top-hat slab of thickness h. Positive centerline
// displacement y is into the bore. The fraction of the jet on the bore
// side of the edge is the overlap of [y−h/2, y+h/2] with that side:
// one half at y = 0, the whole jet for y ≥ h/2, and none for y ≤ −h/2.
// h is not derived from S_j. No spatial growth rate is applied.
//
// Not in this model: tone holes, a chimney, embouchure-hole radiation,
// and a viscous wall boundary layer. No sounding length is stored.

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "../physics/modes.h"

namespace mmathen {

inline constexpr const char* kFluteSource =
    "Flute: open-open cylinder, f_n = n c/(2L), far-end reflection -1. "
    "Bore impedance j Zc tan(kL) is zero on those modes and a pole on "
    "the odd quarter-waves (2n-1)c/(4L), which are not flute resonances. "
    "Jet speed U = sqrt(2 dP/rho). A disturbance convects at sigma*U "
    "across the flue-to-edge distance W; regime m is the odd half-period "
    "travel time (2m-1)*sigma*U/(2W). sigma is an argument. The jet is a "
    "top-hat of thickness h, and the bore fraction is the geometric "
    "overlap with the edge. Length, bore area, jet area, thickness, W, "
    "and sigma are arguments. No end correction, tone hole, embouchure "
    "radiation, or jet growth rate is stored.";

// Bore length and area, the static air state, and the jet. sigma is
// u_wave / U and lies in (0, 1]. Every field is an argument.
struct FluteSpec {
  double boreLengthMeters;
  double boreCrossSectionM2;
  double airPressurePa;
  double temperatureKelvin;
  double jetAreaM2;
  double jetThicknessMeters;
  double flueToEdgeMeters;
  double convectionRatio;
};

// fundamentalHz is c/(2L). Its period equals roundTripSeconds. The
// acoustic impedance is Zc = ρ c / S_bore. pressureReflection is −1.
struct Flute {
  double lengthMeters = 0.0;
  double crossSectionM2 = 0.0;
  double soundSpeedMetersPerSecond = 0.0;
  double airDensityKgPerM3 = 0.0;
  double acousticImpedance = 0.0;
  double roundTripSeconds = 0.0;
  double fundamentalHz = 0.0;
  double pressureReflection = 0.0;
  double jetAreaM2 = 0.0;
  double jetThicknessMeters = 0.0;
  double flueToEdgeMeters = 0.0;
  double convectionRatio = 0.0;
  const char* source = nullptr;
};

// InvalidArgument: a length, area, jet area, thickness, or flue-to-edge
// distance that is not strictly positive and finite, or a convection
// ratio outside (0, 1]. Air-density and sound-speed errors propagate.
// RangeError if Zc, the round trip, or the fundamental overflows.
[[nodiscard]] inline Result<Flute, QuantityError> makeFlute(
    const FluteSpec& spec) {
  using R = Result<Flute, QuantityError>;
  const double length = spec.boreLengthMeters;
  const double area = spec.boreCrossSectionM2;
  const double jetArea = spec.jetAreaM2;
  const double thickness = spec.jetThicknessMeters;
  const double span = spec.flueToEdgeMeters;
  const double sigma = spec.convectionRatio;
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(area) ||
      !(area > 0.0) || !std::isfinite(jetArea) || !(jetArea > 0.0) ||
      !std::isfinite(thickness) || !(thickness > 0.0) ||
      !std::isfinite(span) || !(span > 0.0) || !std::isfinite(sigma) ||
      !(sigma > 0.0) || sigma > 1.0)
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
      openPipeMode(DistanceMeters{length}, speed.value(), 1);
  if (!fundamental.ok()) return R::failure(fundamental.error());
  if (!std::isfinite(impedance) || !(impedance > 0.0) ||
      !std::isfinite(roundTrip) || !(roundTrip > 0.0))
    return R::failure(QuantityError::RangeError);
  Flute flute;
  flute.lengthMeters = length;
  flute.crossSectionM2 = area;
  flute.soundSpeedMetersPerSecond = speed.value().value();
  flute.airDensityKgPerM3 = density.value().value();
  flute.acousticImpedance = impedance;
  flute.roundTripSeconds = roundTrip;
  flute.fundamentalHz = fundamental.value().value();
  flute.pressureReflection = -1.0;
  flute.jetAreaM2 = jetArea;
  flute.jetThicknessMeters = thickness;
  flute.flueToEdgeMeters = span;
  flute.convectionRatio = sigma;
  flute.source = kFluteSource;
  return R::success(flute);
}

// Open-pipe mode n (1-based): f_n = n c/(2L). Every positive integer is
// a resonance, including the octave. This is openPipeMode on the built
// length and sound speed.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> flutePipeMode(
    const Flute& flute, int n) {
  return openPipeMode(DistanceMeters{flute.lengthMeters},
                      SpeedMetersPerSecond{flute.soundSpeedMetersPerSecond},
                      n);
}

namespace flute_detail {

// Odd quarter-wave count 4 f L / c. j Zc tan(kL) poles when that count
// is an odd integer (cos(kL) = 0). The comparison is a few ulps, because
// c/(4L) is not a bitwise integer. Even counts are the open-pipe zeros.
[[nodiscard]] inline bool oddQuarterPole(double quarterTurns) {
  if (!std::isfinite(quarterTurns)) return true;
  const double nearest = std::round(quarterTurns);
  const double scale = std::max(1.0, std::fabs(quarterTurns));
  const double slack =
      16.0 * std::numeric_limits<double>::epsilon() * scale;
  if (!(std::fabs(quarterTurns - nearest) <= slack)) return false;
  return std::fmod(std::fabs(nearest), 2.0) != 0.0;
}

}  // namespace flute_detail

// Bore impedance (Pa·s/m³) at the embouchure, far end open:
// j Zc tan(kL), built as a pure-imaginary complex. The flute's modes
// are the zeros. A pole, where a quarter-wave pipe would speak, is
// RangeError. Frequency 0 is a short, Z = 0. A negative or non-finite
// frequency is InvalidArgument.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
fluteBoreImpedance(const Flute& flute, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(flute.lengthMeters) || !(flute.lengthMeters > 0.0) ||
      !std::isfinite(flute.soundSpeedMetersPerSecond) ||
      !(flute.soundSpeedMetersPerSecond > 0.0) ||
      !std::isfinite(flute.acousticImpedance) ||
      !(flute.acousticImpedance > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double kl = constants::twoPi * frequencyHz * flute.lengthMeters /
                    flute.soundSpeedMetersPerSecond;
  if (!std::isfinite(kl)) return R::failure(QuantityError::RangeError);
  const double quarterTurns = (2.0 * kl) / constants::pi;
  if (flute_detail::oddQuarterPole(quarterTurns))
    return R::failure(QuantityError::RangeError);
  const double tangent = std::tan(kl);
  if (!std::isfinite(tangent)) return R::failure(QuantityError::RangeError);
  const double imaginary = flute.acousticImpedance * tangent;
  if (!std::isfinite(imaginary)) return R::failure(QuantityError::RangeError);
  return R::success(std::complex<double>{0.0, imaginary});
}

// Centerline speed (m/s), U = sqrt(2 ΔP / ρ). ΔP = 0 is a jet at rest.
// ΔP < 0 is DomainError: this is not a reverse flow. Non-finite ΔP is
// InvalidArgument. RangeError if the square root overflows.
[[nodiscard]] inline Result<double, QuantityError> fluteJetSpeed(
    const Flute& flute, double overpressurePa) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(overpressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (overpressurePa < 0.0) return R::failure(QuantityError::DomainError);
  if (!std::isfinite(flute.airDensityKgPerM3) ||
      !(flute.airDensityKgPerM3 > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (overpressurePa == 0.0) return R::success(0.0);
  const double speed =
      std::sqrt(2.0 * overpressurePa / flute.airDensityKgPerM3);
  if (!std::isfinite(speed) || !(speed > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(speed);
}

// Steady jet volume flow (m³/s), S_j U. Zero when the jet is at rest.
[[nodiscard]] inline Result<double, QuantityError> fluteJetFlow(
    const Flute& flute, double overpressurePa) {
  using R = Result<double, QuantityError>;
  auto speed = fluteJetSpeed(flute, overpressurePa);
  if (!speed.ok()) return R::failure(speed.error());
  if (!std::isfinite(flute.jetAreaM2) || !(flute.jetAreaM2 > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double flow = flute.jetAreaM2 * speed.value();
  if (!std::isfinite(flow)) return R::failure(QuantityError::RangeError);
  return R::success(flow);
}

// Travel time (s) of the convected disturbance, W / (σ U). ΔP ≤ 0 is
// DomainError: a jet at rest has no travel time. The pipe length does
// not enter.
[[nodiscard]] inline Result<double, QuantityError> fluteConvectionTime(
    const Flute& flute, double overpressurePa) {
  using R = Result<double, QuantityError>;
  auto speed = fluteJetSpeed(flute, overpressurePa);
  if (!speed.ok()) return R::failure(speed.error());
  if (!(speed.value() > 0.0)) return R::failure(QuantityError::DomainError);
  if (!std::isfinite(flute.flueToEdgeMeters) ||
      !(flute.flueToEdgeMeters > 0.0) ||
      !std::isfinite(flute.convectionRatio) ||
      !(flute.convectionRatio > 0.0) || flute.convectionRatio > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  const double time =
      flute.flueToEdgeMeters / (flute.convectionRatio * speed.value());
  if (!std::isfinite(time) || !(time > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(time);
}

// Hydrodynamic regime m ≥ 1: f_m = (2m−1) / (2 τ). m = 1 is one half
// period across W. This is not a pipe mode.
[[nodiscard]] inline Result<double, QuantityError> fluteJetRegimeFrequency(
    const Flute& flute, double overpressurePa, int regime) {
  using R = Result<double, QuantityError>;
  if (regime < 1) return R::failure(QuantityError::InvalidArgument);
  auto time = fluteConvectionTime(flute, overpressurePa);
  if (!time.ok()) return R::failure(time.error());
  const double frequency =
      (2.0 * static_cast<double>(regime) - 1.0) / (2.0 * time.value());
  if (!std::isfinite(frequency) || !(frequency > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(frequency);
}

// Fraction of the top-hat jet on the bore side of the edge. Non-finite
// displacement is InvalidArgument. Outside the slab the fraction is 0
// or 1, not an error.
[[nodiscard]] inline Result<double, QuantityError> fluteJetFraction(
    const Flute& flute, double centerlineDisplacementMeters) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(centerlineDisplacementMeters))
    return R::failure(QuantityError::InvalidArgument);
  const double thickness = flute.jetThicknessMeters;
  if (!std::isfinite(thickness) || !(thickness > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double half = 0.5 * thickness;
  if (centerlineDisplacementMeters >= half) return R::success(1.0);
  if (centerlineDisplacementMeters <= -half) return R::success(0.0);
  const double fraction =
      0.5 + centerlineDisplacementMeters / thickness;
  if (!std::isfinite(fraction)) return R::failure(QuantityError::RangeError);
  return R::success(fraction);
}

// Far-end state from an outgoing pressure and the outgoing pressure one
// round trip earlier. p− = −p+_delayed, p = p+ + p−, and the volume
// flow into the bore is (p+ − p−) / Zc. No time step is stored.
struct FluteWaveSplit {
  double outgoingPressurePa = 0.0;
  double incomingPressurePa = 0.0;
  double mouthPressurePa = 0.0;
  double volumeFlowM3PerS = 0.0;
};

// InvalidArgument: a pressure is non-finite, or Zc is not strictly
// positive. RangeError if a product overflows.
[[nodiscard]] inline Result<FluteWaveSplit, QuantityError> fluteWaveSplit(
    const Flute& flute, double outgoingPressurePa,
    double delayedOutgoingPressurePa) {
  using R = Result<FluteWaveSplit, QuantityError>;
  if (!std::isfinite(outgoingPressurePa) ||
      !std::isfinite(delayedOutgoingPressurePa))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(flute.acousticImpedance) ||
      !(flute.acousticImpedance > 0.0) ||
      flute.pressureReflection != -1.0)
    return R::failure(QuantityError::InvalidArgument);
  const double incoming =
      flute.pressureReflection * delayedOutgoingPressurePa;
  const double pressure = outgoingPressurePa + incoming;
  const double flow =
      (outgoingPressurePa - incoming) / flute.acousticImpedance;
  if (!std::isfinite(incoming) || !std::isfinite(pressure) ||
      !std::isfinite(flow))
    return R::failure(QuantityError::RangeError);
  return R::success(
      FluteWaveSplit{outgoingPressurePa, incoming, pressure, flow});
}

// One embouchure sample. The jet flow and the bore flow are both
// reported. They are not required to match, and nothing is stepped.
struct FluteEmbouchure {
  double jetSpeedMetersPerSecond = 0.0;
  double jetFlowM3PerS = 0.0;
  double boreFlowM3PerS = 0.0;
  double jetIntoBoreM3PerS = 0.0;
  double jetFraction = 0.0;
  double convectionSeconds = 0.0;
  double firstRegimeHz = 0.0;
  double pipeFundamentalHz = 0.0;
};

// ΔP ≤ 0 is DomainError: the sample includes a travel time. The pipe
// fundamental is copied from the flute and does not depend on ΔP.
[[nodiscard]] inline Result<FluteEmbouchure, QuantityError> fluteEmbouchure(
    const Flute& flute, double overpressurePa, double outgoingPressurePa,
    double delayedOutgoingPressurePa, double centerlineDisplacementMeters) {
  using R = Result<FluteEmbouchure, QuantityError>;
  auto split = fluteWaveSplit(flute, outgoingPressurePa,
                              delayedOutgoingPressurePa);
  if (!split.ok()) return R::failure(split.error());
  auto speed = fluteJetSpeed(flute, overpressurePa);
  if (!speed.ok()) return R::failure(speed.error());
  if (!(speed.value() > 0.0)) return R::failure(QuantityError::DomainError);
  auto flow = fluteJetFlow(flute, overpressurePa);
  if (!flow.ok()) return R::failure(flow.error());
  auto fraction = fluteJetFraction(flute, centerlineDisplacementMeters);
  if (!fraction.ok()) return R::failure(fraction.error());
  auto time = fluteConvectionTime(flute, overpressurePa);
  if (!time.ok()) return R::failure(time.error());
  auto regime = fluteJetRegimeFrequency(flute, overpressurePa, 1);
  if (!regime.ok()) return R::failure(regime.error());
  const double intoBore = fraction.value() * flow.value();
  if (!std::isfinite(intoBore)) return R::failure(QuantityError::RangeError);
  FluteEmbouchure sample;
  sample.jetSpeedMetersPerSecond = speed.value();
  sample.jetFlowM3PerS = flow.value();
  sample.boreFlowM3PerS = split.value().volumeFlowM3PerS;
  sample.jetIntoBoreM3PerS = intoBore;
  sample.jetFraction = fraction.value();
  sample.convectionSeconds = time.value();
  sample.firstRegimeHz = regime.value();
  sample.pipeFundamentalHz = flute.fundamentalHz;
  return R::success(sample);
}

}  // namespace mmathen
