#pragma once

// Bow — steady ideal Helmholtz motion on one ideal string (docs/48).
//
// This is a friction model with a closed-form response, not a time-stepped
// stick-slip capture and not a body. The string is the fixed-fixed course
// from instrument.h (lossless, non-stiff). The bridge is rigid: the
// response at the bridge is the transverse force the string exerts there,
// not a velocity obtained by dividing by a body resistance.
//
// β is the bow's distance from the bridge divided by the vibrating length
// (Guettler, "The Helmholtz motion"; Woodhouse, Euphonics §9.3.1). It is
// not the nut-fraction used by a pluck.
//
// Ideal Helmholtz motion, period P = 2L/c = 1/f1 (Woodhouse §9.3.1,
// following the zero-mean velocity condition):
//   stick for (1-β) P at velocity v_b,
//   slip  for β P     at velocity v_s = -v_b (1-β)/β.
// The relative slip speed is v_s - v_b = -v_b/β (Guettler).
// During slip the transverse friction force on the string opposes that
// relative velocity and has magnitude μ_d N.
//
// The force on a rigid bridge is a sawtooth of slope T v_b / (β L), with
// one flyback per period of size -2 Z v_b / β, Z = μ c (Woodhouse §9.3.1,
// Schelleng's geometry). Time origin: t = 0 starts the stick interval.
// The flyback is at the start of slip.
//
// Schelleng's maximum normal force (Woodhouse §9.3.1, eq. 4):
//   f_max = 2 Z |v_b| / (β (μ_s - μ_d)).
// A heavier bow is DomainError: this approximation will not pretend the
// string is still in Helmholtz motion. Schelleng's minimum force is not
// applied. It needs a bridge resistance R (a body), and no R is invented
// here.
//
// Limits: no capture transient, no bow-hair width, no torsion, no thermal
// rosin, no rounded corner, no minimum-force check, no radiated sound.
// Hammer and reed are not this model.
//
// VERIFIED against the identities above. UNVALIDATED against measured bow
// force or bridge force.

#include <cmath>

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kHelmholtzBowSource =
    "Ideal Helmholtz stick/slip: Knut Guettler, \"The Helmholtz motion\", "
    "https://knutsacoustics.com/files/The-Helmholtz-motion.pdf "
    "(relative slip speed -v_b/beta). "
    "Bridge-force sawtooth and Schelleng maximum bow force: Jim Woodhouse, "
    "Euphonics 9.3.1, https://euphonics.org/9-3-1-shellengs-bow-force-limits/ "
    "(slope T v_b/(beta L), flyback -2 Z v_b/beta, "
    "f_max = 2 Z |v_b|/(beta (mu_s-mu_d))). "
    "Schelleng's minimum force is omitted: it needs a bridge resistance.";

// Caller-supplied friction. Not a hidden rosin table: μ_s must exceed μ_d.
struct BowFriction {
  double staticCoefficient;
  double slidingCoefficient;
};

// β from the bridge, bow speed (m/s, sign is direction), normal force (N).
struct BowStroke {
  double bridgeFraction;
  double speedMetersPerSecond;
  double normalForceNewtons;
  BowFriction friction;
};

// Steady regime on one course. Forces are newtons, speeds are m/s.
struct HelmholtzStroke {
  double period = 0.0;
  double beta = 0.0;
  double bowSpeed = 0.0;
  double stickVelocity = 0.0;
  double slipVelocity = 0.0;
  double stickDuration = 0.0;
  double slipDuration = 0.0;
  double impedance = 0.0;
  double maxNormalForce = 0.0;
  double bridgeForceSlope = 0.0;
  double bridgeForceFlyback = 0.0;  // value after the jump minus value before
  double slipFrictionForce = 0.0;   // transverse force on the string
  const char* source = nullptr;
};

namespace helmholtz_detail {

[[nodiscard]] inline double wrapPeriod(double time, double period) {
  double tau = std::fmod(time, period);
  if (tau < 0.0) tau += period;
  if (!(tau < period)) tau = 0.0;
  return tau;
}

[[nodiscard]] inline double signNonzero(double value) {
  if (value > 0.0) return 1.0;
  if (value < 0.0) return -1.0;
  return 0.0;
}

}  // namespace helmholtz_detail

// Admissible steady Helmholtz motion, or a refusal.
// InvalidArgument: β not in (0, 1), zero or non-finite speed, non-positive
// or non-finite normal force, friction coefficients that are non-finite,
// negative, or have μ_s <= μ_d.
// DomainError: normal force above Schelleng's maximum.
[[nodiscard]] inline Result<HelmholtzStroke, QuantityError> makeHelmholtzStroke(
    const StringCourse& course, const BowStroke& stroke) {
  using R = Result<HelmholtzStroke, QuantityError>;
  const double beta = stroke.bridgeFraction;
  const double speed = stroke.speedMetersPerSecond;
  const double normal = stroke.normalForceNewtons;
  const double muS = stroke.friction.staticCoefficient;
  const double muD = stroke.friction.slidingCoefficient;
  if (!std::isfinite(beta) || !(beta > 0.0) || !(beta < 1.0) ||
      !std::isfinite(speed) || speed == 0.0 || !std::isfinite(normal) ||
      !(normal > 0.0) || !std::isfinite(muS) || !std::isfinite(muD) ||
      !(muD >= 0.0) || !(muS > muD))
    return R::failure(QuantityError::InvalidArgument);
  const double length = course.geometry.vibratingLength.value();
  const double tension = course.tension.value();
  const double density = course.material.linearDensity.value();
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(tension) ||
      !(tension > 0.0) || !std::isfinite(density) || !(density > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double waveSpeed = std::sqrt(tension / density);
  const double impedance = density * waveSpeed;
  const double period = 2.0 * length / waveSpeed;
  const double gap = muS - muD;
  const double maxNormal =
      2.0 * impedance * std::fabs(speed) / (beta * gap);
  if (!std::isfinite(maxNormal) || !std::isfinite(period))
    return R::failure(QuantityError::RangeError);
  if (normal > maxNormal) return R::failure(QuantityError::DomainError);
  const double stickVelocity = speed;
  const double slipVelocity = -speed * (1.0 - beta) / beta;
  const double relativeSlip = slipVelocity - speed;
  const double flyback = -2.0 * impedance * speed / beta;
  const double slope = -flyback / period;
  HelmholtzStroke motion;
  motion.period = period;
  motion.beta = beta;
  motion.bowSpeed = speed;
  motion.stickVelocity = stickVelocity;
  motion.slipVelocity = slipVelocity;
  motion.stickDuration = (1.0 - beta) * period;
  motion.slipDuration = beta * period;
  motion.impedance = impedance;
  motion.maxNormalForce = maxNormal;
  motion.bridgeForceSlope = slope;
  motion.bridgeForceFlyback = flyback;
  motion.slipFrictionForce =
      -muD * normal * helmholtz_detail::signNonzero(relativeSlip);
  motion.source = kHelmholtzBowSource;
  return R::success(motion);
}

// Bow-point transverse velocity. Stick on [0, stickDuration), slip after,
// inside each period. t = 0 starts the stick.
[[nodiscard]] inline double helmholtzBowVelocity(const HelmholtzStroke& motion,
                                                 double timeSeconds) {
  const double tau = helmholtz_detail::wrapPeriod(timeSeconds, motion.period);
  return tau < motion.stickDuration ? motion.stickVelocity
                                    : motion.slipVelocity;
}

// Zero-mean bow-point displacement, the integral of helmholtzBowVelocity.
[[nodiscard]] inline double helmholtzBowDisplacement(
    const HelmholtzStroke& motion, double timeSeconds) {
  const double tau = helmholtz_detail::wrapPeriod(timeSeconds, motion.period);
  const double stick = motion.stickDuration;
  const double rising = motion.stickVelocity * stick;
  double raw = 0.0;
  if (tau < stick) {
    raw = motion.stickVelocity * tau;
  } else {
    raw = rising + motion.slipVelocity * (tau - stick);
  }
  return raw - 0.5 * rising;
}

// Zero-mean force on a rigid bridge. The flyback (after minus before) sits
// at the start of slip. Units: newtons.
[[nodiscard]] inline double helmholtzBridgeForce(const HelmholtzStroke& motion,
                                                 double timeSeconds) {
  const double tau = helmholtz_detail::wrapPeriod(timeSeconds, motion.period);
  double sinceSlip = tau - motion.stickDuration;
  if (sinceSlip < 0.0) sinceSlip += motion.period;
  return 0.5 * motion.bridgeForceFlyback +
         motion.bridgeForceSlope * sinceSlip;
}

}  // namespace mmathen
