#pragma once

// Body — one mode of driving-point admittance, driven by the Helmholtz
// bridge force (docs/48).
//
// Woodhouse's modal sum for the body admittance at a drive point is one
// term per mode,
//   Y(ω) = jω / (m (ω0² - ω² + j ω ω0 / Q)),
// with Q = 1/(2ζ) and the mode-shape factor absorbed into the effective
// mass m ("On the 'bridge hill' of the violin", eq. (4)). This header
// keeps a single term. A measured bridge admittance is a sum of many
// terms; that sum is not stored here.
//
// Resonance frequency, Q, and m are arguments. Body length does not
// determine them, and this library has no measured mode table for the
// violin, viola, cello, or double bass. Nothing in those factories is a
// hidden A0 or B1.
//
// The drive is the rigid-bridge Helmholtz force from bow.h, open-loop.
// The string is not re-solved with a moving end, so there is no
// back-reaction and no wolf note. The bow itself is unchanged.
//
// The same mode has a resonant mechanical resistance r = m ω0 / Q, which
// is the real driving-point impedance at ω0. Schelleng's minimum bow
// force is not evaluated. It needs the bridge resistance the string
// actually sees, and one mode's r is not that measurement. No numerical
// R is invented for any instrument.
//
// Not evaluated, because each one needs data this layer does not have:
//   far-field pressure (radiating area or volume velocity, and a distance),
//   the cavity Helmholtz frequency (f-hole area, cavity volume, neck
//   length; body length is not those),
//   A0–B1 coupling (a mutual term).
// Hammer and reed are not this model.
//
// VERIFIED against the modal admittance and against the oscillator ODE
// driven by the sawtooth's Fourier series. UNVALIDATED against a measured
// bridge admittance or a radiated tone.

#include <cmath>
#include <complex>

#include "../math/constants.h"
#include "bow.h"

namespace mmathen {

inline constexpr const char* kBodyResonatorSource =
    "One driving-point term of Jim Woodhouse, \"On the 'bridge hill' of "
    "the violin\", eq. (4): "
    "Y(omega) = j omega / (m (omega0^2 - omega^2 + j omega omega0 / Q)), "
    "Q = 1/(2 zeta), mode-shape factor absorbed into the effective mass m. "
    "https://archive.violinbridges.co.uk/wp-content/uploads/2015/06/Bridgehill.pdf . "
    "Resonance, Q, and m are caller arguments; no instrument mode table is "
    "stored. The Helmholtz force drives the mode open-loop. Schelleng's "
    "minimum bow force is not applied. Far-field pressure, the cavity "
    "Helmholtz frequency, and A0-B1 coupling are not evaluated.";

// Caller-supplied mode. Frequency in hertz, Q dimensionless, mass in kg.
struct BodyModeSpec {
  double resonanceHz;
  double qualityFactor;
  double modalMassKg;
};

// One built mode. damping is r = m ω0 / Q (N·s/m), the resonant resistance.
// resonantMobility is 1/r (m/s per N).
struct BodyResonator {
  double resonanceHz = 0.0;
  double omega0 = 0.0;
  double qualityFactor = 0.0;
  double modalMassKg = 0.0;
  double stiffness = 0.0;
  double damping = 0.0;
  double resonantMobility = 0.0;
  const char* source = nullptr;
};

// InvalidArgument if frequency, Q, or mass is non-finite or not strictly
// positive. RangeError if stiffness or damping overflows.
[[nodiscard]] inline Result<BodyResonator, QuantityError> makeBodyResonator(
    const BodyModeSpec& spec) {
  using R = Result<BodyResonator, QuantityError>;
  const double frequency = spec.resonanceHz;
  const double quality = spec.qualityFactor;
  const double mass = spec.modalMassKg;
  if (!std::isfinite(frequency) || !(frequency > 0.0) ||
      !std::isfinite(quality) || !(quality > 0.0) || !std::isfinite(mass) ||
      !(mass > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double omega0 = constants::twoPi * frequency;
  const double stiffness = mass * omega0 * omega0;
  const double damping = mass * omega0 / quality;
  if (!std::isfinite(omega0) || !std::isfinite(stiffness) ||
      !std::isfinite(damping) || !(damping > 0.0))
    return R::failure(QuantityError::RangeError);
  BodyResonator mode;
  mode.resonanceHz = frequency;
  mode.omega0 = omega0;
  mode.qualityFactor = quality;
  mode.modalMassKg = mass;
  mode.stiffness = stiffness;
  mode.damping = damping;
  mode.resonantMobility = 1.0 / damping;
  mode.source = kBodyResonatorSource;
  return R::success(mode);
}

// Driving-point mobility Y(f) = v/F, metres per second per newton.
// f = 0 returns 0. A negative or non-finite frequency is InvalidArgument.
[[nodiscard]] inline Result<std::complex<double>, QuantityError> bodyMobility(
    const BodyResonator& mode, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (frequencyHz == 0.0) return R::success(std::complex<double>{0.0, 0.0});
  const double omega = constants::twoPi * frequencyHz;
  const std::complex<double> denominator(
      mode.omega0 * mode.omega0 - omega * omega,
      omega * mode.omega0 / mode.qualityFactor);
  const std::complex<double> mobility =
      std::complex<double>(0.0, omega) / (mode.modalMassKg * denominator);
  if (!std::isfinite(mobility.real()) || !std::isfinite(mobility.imag()))
    return R::failure(QuantityError::RangeError);
  return R::success(mobility);
}

namespace body_detail {

[[nodiscard]] inline double sinceSlip(const HelmholtzStroke& motion,
                                      double timeSeconds) {
  double tau = std::fmod(timeSeconds, motion.period);
  if (tau < 0.0) tau += motion.period;
  if (!(tau < motion.period)) tau = 0.0;
  double slip = tau - motion.stickDuration;
  if (slip < 0.0) slip += motion.period;
  return slip;
}

// Complex Fourier coefficient of the zero-mean bridge-force sawtooth:
// c_n = -j * flyback / (2 π n), so F(s) = sum_{n≠0} c_n exp(j 2π n s / P)
// with s measured from the flyback (the bow's sinceSlip).
[[nodiscard]] inline std::complex<double> forceCoefficient(
    const HelmholtzStroke& motion, int harmonic) {
  return std::complex<double>(
      0.0, -motion.bridgeForceFlyback /
               (constants::twoPi * static_cast<double>(harmonic)));
}

[[nodiscard]] inline bool usableStroke(const HelmholtzStroke& motion) {
  return std::isfinite(motion.period) && motion.period > 0.0 &&
         std::isfinite(motion.bridgeForceFlyback) &&
         std::isfinite(motion.stickDuration);
}

}  // namespace body_detail

// Coefficient v_n of the steady body velocity in the sinceSlip coordinate:
// v(s) includes 2 Re(v_n exp(j 2π n s / P)). harmonic is 1-based.
// InvalidArgument if the stroke or the index is unusable, or the index
// exceeds 4096.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
bodyVelocityCoefficient(const BodyResonator& mode, const HelmholtzStroke& stroke,
                        int harmonic) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!body_detail::usableStroke(stroke) || harmonic < 1 || harmonic > 4096)
    return R::failure(QuantityError::InvalidArgument);
  const double frequency =
      static_cast<double>(harmonic) / stroke.period;
  auto mobility = bodyMobility(mode, frequency);
  if (!mobility.ok()) return R::failure(mobility.error());
  return R::success(mobility.value() *
                    body_detail::forceCoefficient(stroke, harmonic));
}

// Steady bridge-foot velocity (m/s) under the Helmholtz force, summed
// through `harmonics` terms of the sawtooth. The time origin matches
// helmholtzBridgeForce: t = 0 starts the stick, and the sawtooth coordinate
// starts at the flyback.
[[nodiscard]] inline Result<double, QuantityError> bodyVelocity(
    const BodyResonator& mode, const HelmholtzStroke& stroke,
    double timeSeconds, int harmonics) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(timeSeconds) || harmonics < 1 || harmonics > 4096 ||
      !body_detail::usableStroke(stroke))
    return R::failure(QuantityError::InvalidArgument);
  const double slip = body_detail::sinceSlip(stroke, timeSeconds);
  std::complex<double> sum{0.0, 0.0};
  for (int n = 1; n <= harmonics; ++n) {
    auto coefficient = bodyVelocityCoefficient(mode, stroke, n);
    if (!coefficient.ok()) return R::failure(coefficient.error());
    const double angle =
        constants::twoPi * static_cast<double>(n) * slip / stroke.period;
    sum += coefficient.value() * std::exp(std::complex<double>(0.0, angle));
  }
  const double velocity = 2.0 * sum.real();
  if (!std::isfinite(velocity)) return R::failure(QuantityError::RangeError);
  return R::success(velocity);
}

// Steady bridge-foot displacement (m), the integral of bodyVelocity with
// the same truncation. Zero mean, because the force has no DC term.
[[nodiscard]] inline Result<double, QuantityError> bodyDisplacement(
    const BodyResonator& mode, const HelmholtzStroke& stroke,
    double timeSeconds, int harmonics) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(timeSeconds) || harmonics < 1 || harmonics > 4096 ||
      !body_detail::usableStroke(stroke))
    return R::failure(QuantityError::InvalidArgument);
  const double slip = body_detail::sinceSlip(stroke, timeSeconds);
  std::complex<double> sum{0.0, 0.0};
  for (int n = 1; n <= harmonics; ++n) {
    auto velocity = bodyVelocityCoefficient(mode, stroke, n);
    if (!velocity.ok()) return R::failure(velocity.error());
    const double omega =
        constants::twoPi * static_cast<double>(n) / stroke.period;
    const std::complex<double> displacement =
        velocity.value() / std::complex<double>(0.0, omega);
    const double angle = omega * slip;
    sum += displacement * std::exp(std::complex<double>(0.0, angle));
  }
  const double metres = 2.0 * sum.real();
  if (!std::isfinite(metres)) return R::failure(QuantityError::RangeError);
  return R::success(metres);
}

}  // namespace mmathen
