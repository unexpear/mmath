#pragma once

// Hammer — hard narrow hammer on one ideal string, before the first
// reflection returns (docs/48).
//
// This is the point-mass collision on an infinite string, not a felt
// model and not a pluck. Julius O. Smith, Physical Audio Signal
// Processing, "The Mass-String Collision" (one-way wave impedance R,
// https://ccrma.stanford.edu/~jos/MassString/MassString.pdf): a point
// mass m hits at speed v0 and stays stuck to the string. With waves
// leaving both ways the driving-point impedance is 2R, so
//   v(t) = v0 exp(-2 R t / m),
//   F(t) = 2 R v(t),
//   y(t) = integral of v = v0 (m / (2 R)) (1 - exp(-2 R t / m)).
// Here R is the string's one-way impedance Z = μ c, the same Z as in
// bow.h. Time constant τ = m / (2 Z).
//
// On a finite fixed string that law holds only until a reflected wave
// gets back to the point in question. D. E. Hall, "Piano string
// excitation II: General solution for a hard narrow hammer", J. Acoust.
// Soc. Am. (1987): the first outgoing wave is this solution, and each
// later reflection is another wave, which can throw the hammer off.
// Those later waves are not summed here. Asking for a time at which a
// reflection has already arrived is DomainError, the same kind of refusal
// as a bow heavier than Schelleng's maximum.
//
// The first reflection returns to the contact point at 2 d / c, where d
// is the distance to the nearer end. A hard hammer on an infinite string
// never rebounds; this slice does not invent a contact-loss time.
//
// Not evaluated, because each one needs data or a summation this closed
// form does not have:
//   felt compliance F = K ξ^p (K and p are not a universal constant;
//   none is invented, and they are not arguments of this hard limit),
//   Hall's reflected-wave series and multiple contacts,
//   hammer width, shank rotation, gravity.
// Reed is not this model. The bowed-string factories are unchanged.
//
// VERIFIED against the force, velocity, displacement, momentum, and
// energy identities above. UNVALIDATED against a measured hammer force.

#include <cmath>

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kHardHammerSource =
    "Hard narrow hammer, infinite-string contact law: Julius O. Smith, "
    "Physical Audio Signal Processing, \"The Mass-String Collision\", "
    "https://ccrma.stanford.edu/~jos/MassString/MassString.pdf "
    "(v = v0 exp(-2 Z t / m), F = 2 Z v, Z the one-way string impedance). "
    "Valid only until the first reflection returns: D. E. Hall, \"Piano "
    "string excitation II: General solution for a hard narrow hammer\", "
    "J. Acoust. Soc. Am. (1987). Felt stiffness, reflected-wave sum, and "
    "loss of contact are not evaluated.";

// Position is the fraction of the vibrating length from the nut toward
// the bridge, as for a pluck. Mass is kilograms. Speed is the incoming
// transverse speed in m/s and must be strictly positive: a hard hammer
// strikes, it does not pull.
struct HammerStrike {
  double positionFraction;
  double massKg;
  double speedMetersPerSecond;
};

// Closed form for one blow. impedance is Z = μ c. initialForce is 2 Z v0.
struct HardHammerBlow {
  double positionMeters = 0.0;
  double lengthMeters = 0.0;
  double massKg = 0.0;
  double incomingSpeed = 0.0;
  double waveSpeed = 0.0;
  double impedance = 0.0;
  double timeConstant = 0.0;
  double firstReflectionTime = 0.0;
  double initialForce = 0.0;
  const char* source = nullptr;
};

// InvalidArgument: position not in (0, 1), mass or speed not strictly
// positive and finite, or the course is not a positive finite string.
// RangeError: Z, the time constant, or the initial force overflows.
[[nodiscard]] inline Result<HardHammerBlow, QuantityError> makeHardHammerBlow(
    const StringCourse& course, const HammerStrike& strike) {
  using R = Result<HardHammerBlow, QuantityError>;
  const double fraction = strike.positionFraction;
  const double mass = strike.massKg;
  const double speed = strike.speedMetersPerSecond;
  if (!std::isfinite(fraction) || !(fraction > 0.0) || !(fraction < 1.0) ||
      !std::isfinite(mass) || !(mass > 0.0) || !std::isfinite(speed) ||
      !(speed > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double length = course.geometry.vibratingLength.value();
  const double tension = course.tension.value();
  const double density = course.material.linearDensity.value();
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(tension) ||
      !(tension > 0.0) || !std::isfinite(density) || !(density > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double waveSpeed = std::sqrt(tension / density);
  const double impedance = density * waveSpeed;
  const double position = fraction * length;
  const double nearer = position < length - position ? position : length - position;
  const double timeConstant = mass / (2.0 * impedance);
  const double firstReflection = 2.0 * nearer / waveSpeed;
  const double initialForce = 2.0 * impedance * speed;
  if (!std::isfinite(waveSpeed) || !std::isfinite(impedance) ||
      !std::isfinite(timeConstant) || !std::isfinite(firstReflection) ||
      !std::isfinite(initialForce) || !(timeConstant > 0.0) ||
      !(firstReflection > 0.0))
    return R::failure(QuantityError::RangeError);
  HardHammerBlow blow;
  blow.positionMeters = position;
  blow.lengthMeters = length;
  blow.massKg = mass;
  blow.incomingSpeed = speed;
  blow.waveSpeed = waveSpeed;
  blow.impedance = impedance;
  blow.timeConstant = timeConstant;
  blow.firstReflectionTime = firstReflection;
  blow.initialForce = initialForce;
  blow.source = kHardHammerSource;
  return R::success(blow);
}

namespace hammer_detail {

// Earliest time a reflected wave reaches x. The contact point uses the
// stored firstReflectionTime; any other interior point uses the shorter
// of the two end-paths.
[[nodiscard]] inline double reflectionArrival(const HardHammerBlow& blow,
                                              double positionMeters) {
  const double fromNut = (positionMeters + blow.positionMeters) / blow.waveSpeed;
  const double fromBridge =
      (2.0 * blow.lengthMeters - positionMeters - blow.positionMeters) /
      blow.waveSpeed;
  return fromNut < fromBridge ? fromNut : fromBridge;
}

[[nodiscard]] inline bool contactTime(const HardHammerBlow& blow, double time) {
  return std::isfinite(time) && time >= 0.0 && time < blow.firstReflectionTime;
}

}  // namespace hammer_detail

// Contact-point speed (m/s). t in [0, firstReflectionTime) is the
// exponential. t < 0 or non-finite is InvalidArgument. t at or after the
// first reflection returns is DomainError.
[[nodiscard]] inline Result<double, QuantityError> hardHammerVelocity(
    const HardHammerBlow& blow, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(timeSeconds) || timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!(timeSeconds < blow.firstReflectionTime))
    return R::failure(QuantityError::DomainError);
  const double speed =
      blow.incomingSpeed * std::exp(-timeSeconds / blow.timeConstant);
  if (!std::isfinite(speed)) return R::failure(QuantityError::RangeError);
  return R::success(speed);
}

// Force on the string at the contact point (newtons), F = 2 Z v.
[[nodiscard]] inline Result<double, QuantityError> hardHammerForce(
    const HardHammerBlow& blow, double timeSeconds) {
  using R = Result<double, QuantityError>;
  auto speed = hardHammerVelocity(blow, timeSeconds);
  if (!speed.ok()) return R::failure(speed.error());
  const double force = 2.0 * blow.impedance * speed.value();
  if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
  return R::success(force);
}

// Contact-point displacement (metres), the integral of hardHammerVelocity
// from the instant of contact. Zero at t = 0.
[[nodiscard]] inline Result<double, QuantityError> hardHammerDisplacement(
    const HardHammerBlow& blow, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(timeSeconds) || timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!(timeSeconds < blow.firstReflectionTime))
    return R::failure(QuantityError::DomainError);
  const double decay = std::exp(-timeSeconds / blow.timeConstant);
  const double metres =
      blow.incomingSpeed * blow.timeConstant * (1.0 - decay);
  if (!std::isfinite(metres)) return R::failure(QuantityError::RangeError);
  return R::success(metres);
}

// Direct-wave displacement (metres) at a point on the open string.
// Zero until the outgoing wave arrives. DomainError once a reflection
// from either end has reached that point: the image sum is not applied.
// A position outside (0, L) is InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> hardHammerStringDisplacement(
    const HardHammerBlow& blow, double positionMeters, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(positionMeters) || !(positionMeters > 0.0) ||
      !(positionMeters < blow.lengthMeters) || !std::isfinite(timeSeconds) ||
      timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double reflected = hammer_detail::reflectionArrival(blow, positionMeters);
  if (!(timeSeconds < reflected)) return R::failure(QuantityError::DomainError);
  const double delay =
      std::fabs(positionMeters - blow.positionMeters) / blow.waveSpeed;
  if (timeSeconds < delay) return R::success(0.0);
  return hardHammerDisplacement(blow, timeSeconds - delay);
}

// Direct-wave transverse velocity (m/s). Same domain as the displacement.
[[nodiscard]] inline Result<double, QuantityError> hardHammerStringVelocity(
    const HardHammerBlow& blow, double positionMeters, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(positionMeters) || !(positionMeters > 0.0) ||
      !(positionMeters < blow.lengthMeters) || !std::isfinite(timeSeconds) ||
      timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double reflected = hammer_detail::reflectionArrival(blow, positionMeters);
  if (!(timeSeconds < reflected)) return R::failure(QuantityError::DomainError);
  const double delay =
      std::fabs(positionMeters - blow.positionMeters) / blow.waveSpeed;
  if (timeSeconds < delay) return R::success(0.0);
  return hardHammerVelocity(blow, timeSeconds - delay);
}

}  // namespace mmathen
