#pragma once

// Bow — steady ideal Helmholtz motion, and a separate time-stepped
// stick-slip capture, on one ideal string (docs/48).
//
// The steady stroke is a closed form. The capture steps the same friction
// pair on rigid ends. Neither is a body. The string is the fixed-fixed
// course from instrument.h (lossless, non-stiff). The bridge is rigid:
// the steady response at the bridge is the transverse force the string
// exerts there, not a velocity obtained by dividing by a body resistance.
// The capture's end reflection is −1. A dashpot, and the bridge resistance
// Schelleng's minimum takes as an argument, are not applied here.
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
// string is still in Helmholtz motion. Schelleng's minimum needs a bridge
// resistance R. schellengMinimumForce takes that R as an argument.
// Woodhouse eq. (9):
//   f_min = Z² |v_b| / (2 R β² (μ_s - μ_d)),
// and eq. (10): f_max / f_min = 4 β R / Z. No numerical R is stored.
// The string is not re-solved with a moving bridge.
//
// The time step is Woodhouse, Euphonics 9.2.1: outgoing waves are the
// other side's incoming wave plus f/(2Z), and v_h is the sum of the two
// incoming waves. On this ideal string each reflection function is a
// delay and a factor −1 (Euphonics 9.5.1, rigid end, no corner rounding).
// Friction is the same pair as the steady stroke: stick while the required
// force lies inside μ_s N, and, once slipping, |f| = μ_d N until the
// required force falls to μ_d N. That hysteresis is the Coulomb case of
// the three-intersection rule (Woodhouse and Galluzzo, Acta Acustica 90,
// 2004). No falling rosin curve is stored. Friedlander and Keller showed
// that periodic motion on a lossless rigid string is unstable, so a long
// run is not claimed to lock to Helmholtz. The string is not re-solved
// with a moving bridge.
//
// Limits of the steady stroke: no bow-hair width, no torsion, no thermal
// rosin, no rounded corner. The minimum force is a separate check once R
// is supplied. Hammer and reed are not this model.
//
// VERIFIED against the identities above. UNVALIDATED against measured bow
// force or bridge force.

#include <cmath>
#include <limits>
#include <vector>

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
    "Schelleng minimum, same section eq. (9): "
    "f_min = Z^2 |v_b| / (2 R beta^2 (mu_s-mu_d)), "
    "with f_max/f_min = 4 beta R/Z. R is a caller argument.";

// Caller-supplied friction. Not a hidden rosin table: μ_s must exceed μ_d.
struct BowFriction {
  double staticCoefficient;
  double slidingCoefficient;
  // Optional smooth rosin curve. 0 keeps the Coulomb law above. When
  // positive, sliding friction is the hyperbolic fit
  //   mu(dv) = mu_d + (mu_s - mu_d) v0 / (v0 + |dv|)
  // of Smith and Woodhouse (J. Mech. Phys. Solids 48, 2000) as used in
  // Woodhouse, Acta Acustica 89 (2003); v0 in m/s (their fitted value is
  // of order 0.1 m/s: VERIFY). The slip root is unique for this curve
  // with stick held while |f| <= mu_s N, so there is no recapture
  // hysteresis.
  double hyperbolicV0 = 0.0;
  // When true, sliding friction is the steady-sliding curve fit of Smith
  // and Woodhouse, "The tribology of rosin", J. Mech. Phys. Solids 48
  // (2000) 1633, Eq. (2):
  //   mu(V) = 0.4 exp(-V/0.01) + 0.45 exp(-V/0.1) + 0.35   (V in m/s)
  // so mu(0) = 1.2 is the sticking limit and the coefficients above are
  // ignored. The slip root is found by bisection on [0, |v_b - v_h|].
  bool smithWoodhouse2000 = false;
};

[[nodiscard]] inline double smithWoodhouse2000Mu(double v) noexcept {
  return 0.4 * std::exp(-v / 0.01) + 0.45 * std::exp(-v / 0.1) + 0.35;
}

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

// Woodhouse eq. (9), written as f_max * Z / (4 β R) so it matches eq. (10).
// R is the caller's bridge resistance in N·s/m. The model assumes R is
// large beside Z; a small positive R is still evaluated, and no cutoff
// ratio is invented. InvalidArgument if R is not strictly positive and
// finite, or the stroke does not carry a usable Z, β, and maximum.
// RangeError if the minimum overflows.
[[nodiscard]] inline Result<double, QuantityError> schellengMinimumForce(
    const HelmholtzStroke& motion, double bridgeResistance) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(bridgeResistance) || !(bridgeResistance > 0.0) ||
      !std::isfinite(motion.impedance) || !(motion.impedance > 0.0) ||
      !std::isfinite(motion.beta) || !(motion.beta > 0.0) ||
      !(motion.beta < 1.0) || !std::isfinite(motion.maxNormalForce) ||
      !(motion.maxNormalForce > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double minimum = motion.maxNormalForce * motion.impedance /
                         (4.0 * motion.beta * bridgeResistance);
  if (!std::isfinite(minimum) || !(minimum > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(minimum);
}

// The rigid-bridge stroke, additionally refused when the normal force is
// below Schelleng's minimum for the caller's bridge resistance.
// DomainError if the force is above the maximum or below the minimum.
[[nodiscard]] inline Result<HelmholtzStroke, QuantityError> makeHelmholtzStroke(
    const StringCourse& course, const BowStroke& stroke,
    double bridgeResistance) {
  using R = Result<HelmholtzStroke, QuantityError>;
  auto motion = makeHelmholtzStroke(course, stroke);
  if (!motion.ok()) return R::failure(motion.error());
  auto minimum = schellengMinimumForce(motion.value(), bridgeResistance);
  if (!minimum.ok()) return R::failure(minimum.error());
  if (stroke.normalForceNewtons < minimum.value())
    return R::failure(QuantityError::DomainError);
  return R::success(std::move(motion).value());
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

inline constexpr const char* kStickSlipSource =
    "Time-stepped stick-slip: Jim Woodhouse, Euphonics 9.2.1, "
    "https://euphonics.org/9-2-1-time-domain-simulation-of-a-bowed-string/ "
    "(v = v_h + f/(2Z), outgoing = the other incoming + f/(2Z)). "
    "Rigid ends, reflection -1, no corner rounding: Euphonics 9.5.1. "
    "Constant mu_d while slipping; stick while |f| <= mu_s N, and "
    "recapture only at mu_d N. No bridge resistance and no rosin curve.";

// One sample at the bow. historyVelocity is v_h. Incoming waves are the
// two travelling-wave contributions that sum to it.
struct StickSlipSample {
  double stringVelocity = 0.0;
  double frictionForce = 0.0;
  double historyVelocity = 0.0;
  double incomingBridge = 0.0;
  double incomingNut = 0.0;
  bool stuck = false;
};

struct StickSlipCapture {
  double timeStep = 0.0;
  double impedance = 0.0;
  int bridgeRoundTripSamples = 0;
  int nutRoundTripSamples = 0;
  std::vector<StickSlipSample> samples;
  const char* source = nullptr;
};

// One Friedlander-Keller intersection. The string starts a capture at rest,
// so the first call uses wasStuck = true (static friction).
// InvalidArgument: impedance, normal force, or a friction coefficient not
// strictly usable, zero or non-finite bow speed, or a non-finite v_h.
// The division by 2Z sits after the impedance check in this function.
[[nodiscard]] inline Result<StickSlipSample, QuantityError> stickSlipJunction(
    double historyVelocity, double incomingBridge, double incomingNut,
    double bowSpeed, double impedance, double normalForce, BowFriction friction,
    bool wasStuck) {
  using R = Result<StickSlipSample, QuantityError>;
  const double muS = friction.staticCoefficient;
  const double muD = friction.slidingCoefficient;
  if (!std::isfinite(historyVelocity) || !std::isfinite(incomingBridge) ||
      !std::isfinite(incomingNut) || !std::isfinite(bowSpeed) || bowSpeed == 0.0 ||
      !std::isfinite(impedance) || !(impedance > 0.0) ||
      !std::isfinite(normalForce) || !(normalForce > 0.0) ||
      !std::isfinite(muS) || !std::isfinite(muD) || !(muD >= 0.0) ||
      !(muS > muD))
    return R::failure(QuantityError::InvalidArgument);
  const double twoZ = 2.0 * impedance;
  const double required = twoZ * (bowSpeed - historyVelocity);
  const double staticLimit = muS * normalForce;
  const double dynamicLimit = muD * normalForce;
  const double absRequired = std::fabs(required);
  StickSlipSample sample;
  sample.historyVelocity = historyVelocity;
  sample.incomingBridge = incomingBridge;
  sample.incomingNut = incomingNut;
  if (friction.smithWoodhouse2000) {
    const double limit = smithWoodhouse2000Mu(0.0) * normalForce;
    if (absRequired <= limit) {
      sample.frictionForce = required;
      sample.stuck = true;
      sample.stringVelocity = bowSpeed;
    } else {
      const double a = std::fabs(bowSpeed - historyVelocity);
      const double k = normalForce / twoZ;
      double lo = 0.0, hi = a;
      for (int it = 0; it < 60; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (mid + k * smithWoodhouse2000Mu(mid) - a < 0.0) lo = mid; else hi = mid;
      }
      const double slip = 0.5 * (lo + hi);
      sample.frictionForce =
          smithWoodhouse2000Mu(slip) * normalForce * helmholtz_detail::signNonzero(required);
      sample.stuck = false;
      sample.stringVelocity = historyVelocity + sample.frictionForce / twoZ;
    }
  } else if (friction.hyperbolicV0 > 0.0) {
    if (absRequired <= staticLimit) {
      sample.frictionForce = required;
      sample.stuck = true;
      sample.stringVelocity = bowSpeed;
    } else {
      // s = |v - v_b| solves s = A - mu(s) N / 2Z, A = |v_b - v_h|.
      const double v0 = friction.hyperbolicV0;
      const double a = std::fabs(bowSpeed - historyVelocity);
      const double k = normalForce / twoZ;
      const double b = v0 - a + k * muD;
      const double c = v0 * (k * muS - a);
      const double slip = 0.5 * (-b + std::sqrt(std::max(0.0, b * b - 4.0 * c)));
      const double mu = muD + (muS - muD) * v0 / (v0 + slip);
      sample.frictionForce = mu * normalForce * helmholtz_detail::signNonzero(required);
      sample.stuck = false;
      sample.stringVelocity = historyVelocity + sample.frictionForce / twoZ;
    }
  } else if (wasStuck ? absRequired <= staticLimit : absRequired <= dynamicLimit) {
    sample.frictionForce = required;
    sample.stuck = true;
    sample.stringVelocity = bowSpeed;
  } else {
    sample.frictionForce =
        dynamicLimit * helmholtz_detail::signNonzero(required);
    sample.stuck = false;
    sample.stringVelocity = historyVelocity + sample.frictionForce / twoZ;
  }
  if (!std::isfinite(sample.stringVelocity) ||
      !std::isfinite(sample.frictionForce))
    return R::failure(QuantityError::RangeError);
  return R::success(sample);
}

// Step the rigid-end string from rest. Bridge and nut arguments are the
// round-trip delays in samples, so β is bridge / (bridge + nut) and must
// match the stroke. The string reflection is −1. Schelleng's force window
// is not applied: this is the friction law, not the steady regime.
// InvalidArgument: delays that are not positive, a delay ratio that is not
// the stroke's β, a non-positive period count, or a stroke the junction
// refuses. The product of periods and samples must fit in an int.
[[nodiscard]] inline Result<StickSlipCapture, QuantityError> captureStickSlip(
    const StringCourse& course, const BowStroke& stroke,
    int bridgeRoundTripSamples, int nutRoundTripSamples, int periodCount) {
  using R = Result<StickSlipCapture, QuantityError>;
  if (bridgeRoundTripSamples < 1 || nutRoundTripSamples < 1 || periodCount < 1)
    return R::failure(QuantityError::InvalidArgument);
  if (bridgeRoundTripSamples >
      std::numeric_limits<int>::max() - nutRoundTripSamples)
    return R::failure(QuantityError::InvalidArgument);
  const int steps = bridgeRoundTripSamples + nutRoundTripSamples;
  if (periodCount > std::numeric_limits<int>::max() / steps)
    return R::failure(QuantityError::InvalidArgument);
  const double beta = stroke.bridgeFraction;
  const double delayBeta =
      static_cast<double>(bridgeRoundTripSamples) / static_cast<double>(steps);
  if (!std::isfinite(beta) || !(std::fabs(delayBeta - beta) <= 1e-9))
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
  if (!std::isfinite(waveSpeed) || !(waveSpeed > 0.0) || !std::isfinite(period) ||
      !(period > 0.0) || !std::isfinite(impedance) || !(impedance > 0.0))
    return R::failure(QuantityError::RangeError);
  const int total = periodCount * steps;
  std::vector<double> outBridge(static_cast<std::size_t>(total), 0.0);
  std::vector<double> outNut(static_cast<std::size_t>(total), 0.0);
  StickSlipCapture capture;
  capture.timeStep = period / static_cast<double>(steps);
  capture.impedance = impedance;
  capture.bridgeRoundTripSamples = bridgeRoundTripSamples;
  capture.nutRoundTripSamples = nutRoundTripSamples;
  capture.samples.reserve(static_cast<std::size_t>(total));
  capture.source = kStickSlipSource;
  bool wasStuck = true;
  for (int i = 0; i < total; ++i) {
    double incomingBridge = 0.0;
    double incomingNut = 0.0;
    if (i >= bridgeRoundTripSamples)
      incomingBridge = -outBridge[static_cast<std::size_t>(i - bridgeRoundTripSamples)];
    if (i >= nutRoundTripSamples)
      incomingNut = -outNut[static_cast<std::size_t>(i - nutRoundTripSamples)];
    const double history = incomingBridge + incomingNut;
    auto sample = stickSlipJunction(history, incomingBridge, incomingNut,
                                    stroke.speedMetersPerSecond, impedance,
                                    stroke.normalForceNewtons, stroke.friction,
                                    wasStuck);
    if (!sample.ok()) return R::failure(sample.error());
    const double twoZ = 2.0 * impedance;
    outBridge[static_cast<std::size_t>(i)] =
        incomingNut + sample.value().frictionForce / twoZ;
    outNut[static_cast<std::size_t>(i)] =
        incomingBridge + sample.value().frictionForce / twoZ;
    wasStuck = sample.value().stuck;
    capture.samples.push_back(sample.value());
  }
  return R::success(std::move(capture));
}

}  // namespace mmathen
