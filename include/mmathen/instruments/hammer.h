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
// hardHammerVelocity, hardHammerForce, hardHammerDisplacement, and the
// direct-wave string copies stay on the pre-reflection side of that
// cutoff. Asking them for a time at which a reflection has already
// arrived is DomainError, the same kind of refusal as a bow heavier
// than Schelleng's maximum.
//
// The first reflection returns to the contact point at 2 d / c, where d
// is the distance to the nearer end. A hard hammer on an infinite string
// never rebounds; those four functions do not invent a contact-loss time.
//
// reflectedHammerVelocity, reflectedHammerForce, and
// reflectedHammerDisplacement add the first fixed-end image from the nut
// and from the bridge. Velocity reflection at a fixed end is −1. With
// incoming velocity waves the contact force is F = 2 Z v − 2 Z (v_il+v_ir).
// Round trip to the nut is t_nut = 2x/c and to the bridge is
// t_bridge = 2(L−x)/c. Until the nearer end's second image at
// 2 * firstReflectionTime, every delayed sample still lies in the pure
// exponential, so the closed form is valid on
// [0, 2 * firstReflectionTime) and DomainError at and after that horizon.
// The pure contact velocity plus, for each arrived delay t_d,
//   extra = v0 (t_d − t) / τ * exp(−(t − t_d) / τ),
// satisfies extra' + extra/τ = −v_pure(t−t_d)/τ. A centered strike has
// t_nut = t_bridge, so both images are added. The extras vanish at the
// arrival, so velocity and displacement stay continuous; the force jumps
// because the returning wave is the strike's velocity step.
//
// Felt compliance is a separate instantaneous law, F = K ξ^p for ξ > 0
// and F = 0 for ξ ≤ 0 (loss of contact is zero force, not the hard-hammer
// DomainError). K and p are arguments. feltHammerDerivative uses the
// blow's mass and impedance together with the caller's hammer and string
// state: a_h = −F/m and v_string = F/(2Z) + v_incoming. It does not step
// time, and it does not move the hard-hammer cutoff.
//
// Still not evaluated: hammer width, shank rotation, gravity, and any
// image past the first from each end. No felt table and no hammer mass
// is stored on a factory. Reed is not this model.
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
    "J. Acoust. Soc. Am. (1987). Felt compliance and the first-image sum "
    "are separate functions in this header and are not applied here.";

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

namespace hammer_detail {

struct ReflectionImage {
  double pureVelocity = 0.0;
  double extraVelocity = 0.0;
  double extraDisplacement = 0.0;
};

// First fixed-end image of delay `delay`. Zero until the wave arrives.
// At the arrival the extra velocity and displacement are zero.
[[nodiscard]] inline ReflectionImage arrivedImage(const HardHammerBlow& blow,
                                                  double time, double delay) {
  ReflectionImage image;
  if (!(time >= delay)) return image;
  const double age = time - delay;
  const double tau = blow.timeConstant;
  const double decay = std::exp(-age / tau);
  image.pureVelocity = blow.incomingSpeed * decay;
  image.extraVelocity = blow.incomingSpeed * (-age / tau) * decay;
  image.extraDisplacement =
      blow.incomingSpeed * ((age + tau) * decay - tau);
  return image;
}

[[nodiscard]] inline bool reflectionBlowUsable(const HardHammerBlow& blow) {
  const double length = blow.lengthMeters;
  const double position = blow.positionMeters;
  return std::isfinite(blow.incomingSpeed) && blow.incomingSpeed > 0.0 &&
         std::isfinite(blow.timeConstant) && blow.timeConstant > 0.0 &&
         std::isfinite(blow.impedance) && blow.impedance > 0.0 &&
         std::isfinite(blow.waveSpeed) && blow.waveSpeed > 0.0 &&
         std::isfinite(blow.firstReflectionTime) &&
         blow.firstReflectionTime > 0.0 && std::isfinite(length) &&
         length > 0.0 && std::isfinite(position) && position > 0.0 &&
         position < length;
}

}  // namespace hammer_detail

// Contact velocity (m/s) with the first fixed-end image from each end.
// t in [0, 2 * firstReflectionTime) is the closed form. t < 0 or
// non-finite is InvalidArgument. t at or after the second-image horizon
// is DomainError: that image is not in the sum. hardHammerVelocity remains
// DomainError from firstReflectionTime onward; this function is the one
// that continues past that cutoff.
[[nodiscard]] inline Result<double, QuantityError> reflectedHammerVelocity(
    const HardHammerBlow& blow, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!hammer_detail::reflectionBlowUsable(blow))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(timeSeconds) || timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!(timeSeconds < 2.0 * blow.firstReflectionTime))
    return R::failure(QuantityError::DomainError);
  const double nut = 2.0 * blow.positionMeters / blow.waveSpeed;
  const double bridge =
      2.0 * (blow.lengthMeters - blow.positionMeters) / blow.waveSpeed;
  const auto fromNut = hammer_detail::arrivedImage(blow, timeSeconds, nut);
  const auto fromBridge =
      hammer_detail::arrivedImage(blow, timeSeconds, bridge);
  const double speed = blow.incomingSpeed * std::exp(-timeSeconds / blow.timeConstant) +
                       fromNut.extraVelocity + fromBridge.extraVelocity;
  if (!std::isfinite(speed)) return R::failure(QuantityError::RangeError);
  return R::success(speed);
}

// Contact force (N), F = 2 Z (v + v_pure(t − t_nut) + v_pure(t − t_bridge)),
// with a delayed pure velocity omitted until its end has reflected.
// Same time domain as reflectedHammerVelocity. The force jumps when an
// image arrives; the velocity does not.
[[nodiscard]] inline Result<double, QuantityError> reflectedHammerForce(
    const HardHammerBlow& blow, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!hammer_detail::reflectionBlowUsable(blow))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(timeSeconds) || timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!(timeSeconds < 2.0 * blow.firstReflectionTime))
    return R::failure(QuantityError::DomainError);
  const double nut = 2.0 * blow.positionMeters / blow.waveSpeed;
  const double bridge =
      2.0 * (blow.lengthMeters - blow.positionMeters) / blow.waveSpeed;
  const auto fromNut = hammer_detail::arrivedImage(blow, timeSeconds, nut);
  const auto fromBridge =
      hammer_detail::arrivedImage(blow, timeSeconds, bridge);
  auto speed = reflectedHammerVelocity(blow, timeSeconds);
  if (!speed.ok()) return R::failure(speed.error());
  const double force =
      2.0 * blow.impedance *
      (speed.value() + fromNut.pureVelocity + fromBridge.pureVelocity);
  if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
  return R::success(force);
}

// Contact displacement (m), the integral of reflectedHammerVelocity from
// the instant of contact. The pure part is v0 τ (1 − e^{−t/τ}). Each
// arrived image adds v0 ((t − t_d) + τ) e^{−(t−t_d)/τ} − v0 τ, which is
// zero at t_d. Same time domain as the velocity.
[[nodiscard]] inline Result<double, QuantityError> reflectedHammerDisplacement(
    const HardHammerBlow& blow, double timeSeconds) {
  using R = Result<double, QuantityError>;
  if (!hammer_detail::reflectionBlowUsable(blow))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(timeSeconds) || timeSeconds < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!(timeSeconds < 2.0 * blow.firstReflectionTime))
    return R::failure(QuantityError::DomainError);
  const double nut = 2.0 * blow.positionMeters / blow.waveSpeed;
  const double bridge =
      2.0 * (blow.lengthMeters - blow.positionMeters) / blow.waveSpeed;
  const auto fromNut = hammer_detail::arrivedImage(blow, timeSeconds, nut);
  const auto fromBridge =
      hammer_detail::arrivedImage(blow, timeSeconds, bridge);
  const double decay = std::exp(-timeSeconds / blow.timeConstant);
  const double metres =
      blow.incomingSpeed * blow.timeConstant * (1.0 - decay) +
      fromNut.extraDisplacement + fromBridge.extraDisplacement;
  if (!std::isfinite(metres)) return R::failure(QuantityError::RangeError);
  return R::success(metres);
}

// F = K ξ^p for ξ > 0, and 0 for ξ ≤ 0. K and p are arguments and must
// be strictly positive and finite. A non-finite compression is
// InvalidArgument. A non-finite power is RangeError. ξ ≤ 0 is loss of
// contact, not the hard-hammer DomainError.
[[nodiscard]] inline Result<double, QuantityError> feltHammerForce(
    double compressionMeters, double stiffness, double exponent) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(compressionMeters) || !std::isfinite(stiffness) ||
      !std::isfinite(exponent))
    return R::failure(QuantityError::InvalidArgument);
  if (!(stiffness > 0.0) || !(exponent > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!(compressionMeters > 0.0)) return R::success(0.0);
  const double force = stiffness * std::pow(compressionMeters, exponent);
  if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
  return R::success(force);
}

// Instantaneous hammer and string state. Displacements are metres,
// velocities are m/s. incomingVelocity is v_il + v_ir at the contact
// point, supplied by the caller.
struct FeltHammerState {
  double hammerDisplacementMeters;
  double hammerVelocityMetersPerSecond;
  double stringDisplacementMeters;
  double incomingVelocityMetersPerSecond;
};

// ξ = y_hammer − y_string. hammerAcceleration is −F/m.
// stringVelocity is F/(2Z) + incomingVelocity. Mass and Z come from the
// blow. No time step is taken, and firstReflectionTime is not consulted:
// the hard-hammer cutoff stays on the hardHammer* functions.
struct FeltHammerDerivative {
  double compressionMeters = 0.0;
  double forceNewtons = 0.0;
  double hammerAcceleration = 0.0;
  double stringVelocity = 0.0;
};

// InvalidArgument: a state component is non-finite, K or p is not
// strictly positive, or the blow's mass or impedance is not strictly
// positive. RangeError if the acceleration or the string velocity
// overflows.
[[nodiscard]] inline Result<FeltHammerDerivative, QuantityError>
feltHammerDerivative(const HardHammerBlow& blow, const FeltHammerState& state,
                     double stiffness, double exponent) {
  using R = Result<FeltHammerDerivative, QuantityError>;
  if (!std::isfinite(state.hammerDisplacementMeters) ||
      !std::isfinite(state.hammerVelocityMetersPerSecond) ||
      !std::isfinite(state.stringDisplacementMeters) ||
      !std::isfinite(state.incomingVelocityMetersPerSecond))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(blow.massKg) || !(blow.massKg > 0.0) ||
      !std::isfinite(blow.impedance) || !(blow.impedance > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double compression =
      state.hammerDisplacementMeters - state.stringDisplacementMeters;
  auto force = feltHammerForce(compression, stiffness, exponent);
  if (!force.ok()) return R::failure(force.error());
  const double acceleration = -force.value() / blow.massKg;
  const double stringVelocity =
      force.value() / (2.0 * blow.impedance) +
      state.incomingVelocityMetersPerSecond;
  if (!std::isfinite(acceleration) || !std::isfinite(stringVelocity))
    return R::failure(QuantityError::RangeError);
  return R::success(FeltHammerDerivative{compression, force.value(),
                                         acceleration, stringVelocity});
}

// Felt contact without a string impedance. Force is feltHammerForce.
// Acceleration is -F/m from the mallet mass alone. A drum head or a
// cymbal does not use F/(2Z): that quotient is the string's driving
// point. ξ ≤ 0 is zero force. Mass must be strictly positive.
struct FeltContactRate {
  double compressionMeters = 0.0;
  double forceNewtons = 0.0;
  double acceleration = 0.0;
};

[[nodiscard]] inline Result<FeltContactRate, QuantityError> feltContactRate(
    double compressionMeters, double stiffness, double exponent, double massKg) {
  using R = Result<FeltContactRate, QuantityError>;
  if (!std::isfinite(massKg) || !(massKg > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto force = feltHammerForce(compressionMeters, stiffness, exponent);
  if (!force.ok()) return R::failure(force.error());
  const double acceleration = -force.value() / massKg;
  if (!std::isfinite(acceleration)) return R::failure(QuantityError::RangeError);
  return R::success(FeltContactRate{compressionMeters, force.value(), acceleration});
}

}  // namespace mmathen
