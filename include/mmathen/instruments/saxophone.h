#pragma once

// Saxophone — Dalmont's inward reed on a truncated cone (docs/48).
//
// Not a clarinet. A clarinet is that same reed on a cylinder, so its
// impedance poles are the odd quarter-waves (2n−1)c/(4L). A saxophone
// bore is a cone: throat at distance x0 from the virtual apex, open at
// the wide end a length L further on. A spherical standing wave that is
// zero at the open end has throat impedance
//
//   Z = j Zc (k x0 sin(kL)) / (sin(kL) + k x0 cos(kL)),
//
// Zc = ρ c / S_throat. The denominator is zero when tan(kL) = −k x0.
// There is one such pole in each interval ((n−1/2)π, nπ). As x0/L grows
// the poles approach the clarinet series (2n−1)c/(4L). As x0/L shrinks
// they approach the complete-cone series n c/(2L), the same pitches as
// an open pipe, and the second partial approaches twice the first. A
// finite truncation is inharmonic: the ratio is neither 2 nor 3. x0 = 0
// is the apex itself, where the throat area is zero, and is refused.
//
// The bell area of a straight cone is S_throat ((x0+L)/x0)^2. It is
// that geometry, not a second argument. No radiation load and no end
// correction are added, so the open end is an ideal pressure release.
//
// The reed is the characteristic and the inertia in reed.h. Area falls
// as S0 (1 − ΔP/P_M) and the flow is zero at closure. It is not an
// outward-striking lip. Length, apex distance, throat area, S0, P_M,
// mass, tip area, closing displacement, and damping are arguments. No
// alto or tenor length, mouthpiece volume, or tone-hole lattice is
// stored. If a mouthpiece has already been absorbed into an effective
// apex distance, that distance is the argument; this header does not
// add a second cavity.
//
// saxophoneEmbouchure takes ΔP from the caller. A cone is not a
// constant-impedance delay line, so there is no plane-wave split and
// no stored round trip. The reed flow and the cone impedance are not
// solved into a note.

#include <cmath>
#include <complex>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "../physics/medium.h"
#include "reed.h"

namespace mmathen {

inline constexpr const char* kSaxophoneSource =
    "Saxophone: Dalmont, Gilbert, and Ollivier quasi-static "
    "characteristic on a truncated cone. Throat impedance "
    "Z = j Zc (k x0 sin(kL)) / (sin(kL) + k x0 cos(kL)), poles where "
    "tan(kL) = -k x0. A large apex distance approaches the clarinet "
    "quarter-waves; a small one approaches n c/(2L). Length, apex "
    "distance, throat area, S0, P_M, mass, tip area, closing "
    "displacement, and damping are arguments. No alto or tenor length, "
    "mouthpiece volume, tone hole, radiation load, or end correction "
    "is stored.";

// Bore length L, apex distance x0, throat area, the static air state,
// and the reed. Every field is an argument.
struct SaxophoneSpec {
  double boreLengthMeters;
  double apexDistanceMeters;
  double throatAreaM2;
  double airPressurePa;
  double temperatureKelvin;
  double equilibriumAreaM2;
  double closurePressurePa;
  double reedMassKg;
  double tipAreaM2;
  double closingDisplacementMeters;
  double dampingNewtonSecondsPerMeter;
};

// fundamentalHz is the first root of tan(kL) = −k x0, not c/(4L) and
// not c/(2L). bellAreaM2 is S_throat ((x0+L)/x0)^2. acousticImpedance
// is Zc at the throat. propagationSeconds is L/c, the travel time of
// the spherical wave along the physical cone, not a resonance period.
struct Saxophone {
  ReedCharacteristic reed;
  ReedInertia inertia;
  double lengthMeters = 0.0;
  double apexDistanceMeters = 0.0;
  double throatAreaM2 = 0.0;
  double bellAreaM2 = 0.0;
  double soundSpeedMetersPerSecond = 0.0;
  double airDensityKgPerM3 = 0.0;
  double acousticImpedance = 0.0;
  double propagationSeconds = 0.0;
  double fundamentalHz = 0.0;
  const char* source = nullptr;
};

namespace saxophone_detail {

// sin θ + α θ cos θ, with θ = kL and α = x0/L. A zero is a pole of Z.
[[nodiscard]] inline double coneDenominator(double theta, double alpha) {
  return std::sin(theta) + alpha * theta * std::cos(theta);
}

// The unique root in ((n−1/2)π, nπ). The left endpoint is ±1 and the
// right endpoint has the opposite sign, and the derivative does not
// change sign inside the interval.
[[nodiscard]] inline Result<double, QuantityError> coneModeAngle(double alpha,
                                                                 int n) {
  using R = Result<double, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(alpha) || !(alpha > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double nReal = static_cast<double>(n);
  const double left0 = (nReal - 0.5) * constants::pi;
  const double right0 = nReal * constants::pi;
  if (!std::isfinite(left0) || !std::isfinite(right0) || !(right0 > left0))
    return R::failure(QuantityError::RangeError);
  double left = left0;
  double gLeft = coneDenominator(left, alpha);
  double right = right0;
  const double gRight = coneDenominator(right, alpha);
  if (!std::isfinite(gLeft) || !std::isfinite(gRight) || !(gLeft * gRight < 0.0))
    return R::failure(QuantityError::RangeError);
  for (int step = 0; step < 64; ++step) {
    const double mid = 0.5 * (left + right);
    const double gMid = coneDenominator(mid, alpha);
    if (!std::isfinite(mid) || !std::isfinite(gMid))
      return R::failure(QuantityError::RangeError);
    if (gLeft * gMid <= 0.0) {
      right = mid;
    } else {
      left = mid;
      gLeft = gMid;
    }
  }
  const double theta = 0.5 * (left + right);
  const double residual = coneDenominator(theta, alpha);
  const double scale = 1.0 + alpha * theta;
  if (!std::isfinite(theta) || !(theta > left0) || !(theta < right0) ||
      !std::isfinite(residual) || !std::isfinite(scale) || !(scale > 0.0) ||
      !(std::abs(residual) <= 1e-8 * scale))
    return R::failure(QuantityError::RangeError);
  return R::success(theta);
}

// True when the denominator is inside a relative neighborhood of a zero.
// The returned frequency of coneModeAngle lands in that neighborhood.
[[nodiscard]] inline bool conePole(double denominator, double scale) {
  if (!std::isfinite(denominator) || !std::isfinite(scale) || !(scale > 0.0))
    return true;
  return !(std::abs(denominator) > 1e-8 * scale);
}

}  // namespace saxophone_detail

// InvalidArgument: a non-positive length, apex distance, throat area,
// S0, P_M, mass, tip area, or closing displacement; a negative damping.
// DomainError from a non-positive air state. RangeError if Zc, the bell
// area, the propagation time, or the first cone mode overflows, or if
// the reed and the bore do not share one density.
[[nodiscard]] inline Result<Saxophone, QuantityError> makeSaxophone(
    const SaxophoneSpec& spec) {
  using R = Result<Saxophone, QuantityError>;
  const double length = spec.boreLengthMeters;
  const double apex = spec.apexDistanceMeters;
  const double area = spec.throatAreaM2;
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(apex) ||
      !(apex > 0.0) || !std::isfinite(area) || !(area > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto reed = makeReedCharacteristic(ReedSpec{
      spec.equilibriumAreaM2, spec.closurePressurePa, spec.airPressurePa,
      spec.temperatureKelvin});
  if (!reed.ok()) return R::failure(reed.error());
  auto inertia = makeReedInertia(
      reed.value(),
      ReedInertiaSpec{spec.reedMassKg, spec.tipAreaM2,
                      spec.closingDisplacementMeters,
                      spec.dampingNewtonSecondsPerMeter});
  if (!inertia.ok()) return R::failure(inertia.error());
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
  const double flare = (apex + length) / apex;
  const double bell = area * flare * flare;
  const double propagation = length / speed.value().value();
  if (reed.value().airDensityKgPerM3 != density.value().value() ||
      inertia.value().closurePressurePa != reed.value().closurePressurePa)
    return R::failure(QuantityError::RangeError);
  if (!std::isfinite(impedance) || !(impedance > 0.0) ||
      !std::isfinite(bell) || !(bell > 0.0) || !std::isfinite(propagation) ||
      !(propagation > 0.0))
    return R::failure(QuantityError::RangeError);
  Saxophone saxophone;
  saxophone.reed = reed.value();
  saxophone.inertia = inertia.value();
  saxophone.lengthMeters = length;
  saxophone.apexDistanceMeters = apex;
  saxophone.throatAreaM2 = area;
  saxophone.bellAreaM2 = bell;
  saxophone.soundSpeedMetersPerSecond = speed.value().value();
  saxophone.airDensityKgPerM3 = density.value().value();
  saxophone.acousticImpedance = impedance;
  saxophone.propagationSeconds = propagation;
  const double alpha = apex / length;
  auto angle = saxophone_detail::coneModeAngle(alpha, 1);
  if (!angle.ok()) return R::failure(angle.error());
  const double fundamental = angle.value() * speed.value().value() /
                             (constants::twoPi * length);
  if (!std::isfinite(fundamental) || !(fundamental > 0.0))
    return R::failure(QuantityError::RangeError);
  saxophone.fundamentalHz = fundamental;
  saxophone.source = kSaxophoneSource;
  return R::success(saxophone);
}

// Cone mode n (1-based), the n-th positive root of tan(kL) = −k x0.
// n < 1 is InvalidArgument. Every positive n is a resonance: there is
// no even-index refusal. The frequency is not (2n−1)c/(4L) and not
// n c/(2L) except in those limits.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> saxophoneConeMode(
    const Saxophone& saxophone, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (!std::isfinite(saxophone.lengthMeters) ||
      !(saxophone.lengthMeters > 0.0) ||
      !std::isfinite(saxophone.apexDistanceMeters) ||
      !(saxophone.apexDistanceMeters > 0.0) ||
      !std::isfinite(saxophone.soundSpeedMetersPerSecond) ||
      !(saxophone.soundSpeedMetersPerSecond > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double alpha =
      saxophone.apexDistanceMeters / saxophone.lengthMeters;
  auto angle = saxophone_detail::coneModeAngle(alpha, n);
  if (!angle.ok()) return R::failure(angle.error());
  const double frequency = angle.value() * saxophone.soundSpeedMetersPerSecond /
                           (constants::twoPi * saxophone.lengthMeters);
  if (!std::isfinite(frequency) || !(frequency > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{frequency});
}

// Throat impedance of the open truncated cone. f = 0 is the removable
// point where numerator and denominator both vanish, and the limit is
// 0. A denominator zero, tan(kL) = −k x0, is RangeError. A negative
// frequency is InvalidArgument. The quotient is real: Z is purely
// imaginary, and no complex division is used.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
saxophoneConeImpedance(const Saxophone& saxophone, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(saxophone.lengthMeters) ||
      !(saxophone.lengthMeters > 0.0) ||
      !std::isfinite(saxophone.apexDistanceMeters) ||
      !(saxophone.apexDistanceMeters > 0.0) ||
      !std::isfinite(saxophone.soundSpeedMetersPerSecond) ||
      !(saxophone.soundSpeedMetersPerSecond > 0.0) ||
      !std::isfinite(saxophone.acousticImpedance) ||
      !(saxophone.acousticImpedance > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (frequencyHz == 0.0) return R::success(std::complex<double>{0.0, 0.0});
  const double omega = constants::twoPi * frequencyHz;
  const double k = omega / saxophone.soundSpeedMetersPerSecond;
  const double kl = k * saxophone.lengthMeters;
  const double kx = k * saxophone.apexDistanceMeters;
  if (!std::isfinite(kl) || !std::isfinite(kx))
    return R::failure(QuantityError::RangeError);
  const double sine = std::sin(kl);
  const double cosine = std::cos(kl);
  const double denominator = sine + kx * cosine;
  const double numerator = kx * sine;
  const double scale = 1.0 + std::abs(kx);
  if (!std::isfinite(denominator) || !std::isfinite(numerator) ||
      !std::isfinite(scale) ||
      saxophone_detail::conePole(denominator, scale))
    return R::failure(QuantityError::RangeError);
  const double imaginary =
      saxophone.acousticImpedance * numerator / denominator;
  if (!std::isfinite(imaginary)) return R::failure(QuantityError::RangeError);
  return R::success(std::complex<double>{0.0, imaginary});
}

// One reed sample at the caller's ΔP. The cone is not stepped.
struct SaxophoneEmbouchure {
  double pressureDifferencePa = 0.0;
  double reedFlowM3PerS = 0.0;
  double equilibriumDisplacementMeters = 0.0;
  double tipAccelerationMetersPerSecondSquared = 0.0;
};

// Reed flow is the Bernoulli jet. It is zero at ΔP = 0 and at closure.
// Equilibrium is the static line, so ΔP outside [0, P_M], including
// ΔP < 0, is DomainError. Tip acceleration is the linear spring at the
// caller's displacement and velocity. InvalidArgument: a non-finite
// pressure or reed state. RangeError if a product overflows.
[[nodiscard]] inline Result<SaxophoneEmbouchure, QuantityError>
saxophoneEmbouchure(const Saxophone& saxophone, double pressureDifferencePa,
                    double displacementMeters,
                    double velocityMetersPerSecond) {
  using R = Result<SaxophoneEmbouchure, QuantityError>;
  auto flow = reedVolumeFlow(saxophone.reed, pressureDifferencePa);
  if (!flow.ok()) return R::failure(flow.error());
  auto equilibrium =
      reedEquilibriumDisplacement(saxophone.inertia, pressureDifferencePa);
  if (!equilibrium.ok()) return R::failure(equilibrium.error());
  auto acceleration = reedTipAcceleration(
      saxophone.inertia, displacementMeters, velocityMetersPerSecond,
      pressureDifferencePa);
  if (!acceleration.ok()) return R::failure(acceleration.error());
  SaxophoneEmbouchure sample;
  sample.pressureDifferencePa = pressureDifferencePa;
  sample.reedFlowM3PerS = flow.value();
  sample.equilibriumDisplacementMeters = equilibrium.value();
  sample.tipAccelerationMetersPerSecondSquared = acceleration.value();
  return R::success(sample);
}

}  // namespace mmathen
