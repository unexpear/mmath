#pragma once

// Body — driving-point admittance as a sum of modes, driven by the
// Helmholtz bridge force (docs/48).
//
// Woodhouse's modal sum for the body admittance at a drive point is
//   Y(ω) = Σ_k jω / (m_k (ω_k² - ω² + j ω ω_k / Q_k)),
// with Q = 1/(2ζ) and the mode-shape factor absorbed into the effective
// mass m ("On the 'bridge hill' of the violin", eq. (4)). Each term is
// that quotient. The sum is the caller’s table of terms, added. This
// header does not store a measured bridge admittance.
//
// Resonance frequency, Q, and m of every mode are arguments. Body length
// does not determine them, and this library has no measured mode table
// for the violin, viola, cello, or double bass. Nothing in those
// factories is a hidden A0 or B1. A0–B1 coupling would be a mutual term
// between two modes; the sum here is diagonal driving-point terms only.
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
// The cavity air resonance is makeCavityHelmholtz: Rayleigh's resonator
// from a caller-supplied f-hole area, cavity volume, and effective neck
// length, with c and ρ from the dry-air model. It is not a row of the
// modal sum. Inserting it there would need a modal mass and a Q, and
// neither is invented. Body length is not the neck, the volume, or the
// area.
//
// Not evaluated, because each one needs data this layer does not have:
//   far-field pressure (radiating area or volume velocity, and a distance),
//   A0–B1 coupling (a mutual term).
// Hammer and reed are not this model.
//
// VERIFIED against the modal admittance and against the oscillator ODE
// driven by the sawtooth's Fourier series. UNVALIDATED against a measured
// bridge admittance or a radiated tone.

#include <cmath>
#include <complex>
#include <cstddef>
#include <span>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "bow.h"

namespace mmathen {

inline constexpr const char* kBodyResonatorSource =
    "Driving-point terms of Jim Woodhouse, \"On the 'bridge hill' of "
    "the violin\", eq. (4): "
    "Y(omega) = sum_k j omega / (m_k (omega_k^2 - omega^2 + "
    "j omega omega_k / Q_k)), "
    "Q = 1/(2 zeta), mode-shape factor absorbed into the effective mass m. "
    "https://archive.violinbridges.co.uk/wp-content/uploads/2015/06/Bridgehill.pdf . "
    "Each resonance, Q, and m is a caller argument. No violin, viola, cello, "
    "or bass admittance table is stored, and no A0 or B1 frequency is "
    "stored. The Helmholtz force drives the sum open-loop. Schelleng's "
    "minimum bow force is not applied. The cavity Helmholtz frequency is "
    "makeCavityHelmholtz and is not a term of this sum. Far-field pressure "
    "and A0-B1 coupling (a mutual term) are not evaluated.";

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

// Build every row of a caller-supplied table. `specs` and `modes` must be
// the same non-zero length. On failure `modes` is left unchanged.
// InvalidArgument if the table is empty, the lengths differ, or any row
// fails the single-mode checks. RangeError if any row overflows.
[[nodiscard]] inline Result<std::size_t, QuantityError> makeBodyResonators(
    std::span<const BodyModeSpec> specs, std::span<BodyResonator> modes) {
  using R = Result<std::size_t, QuantityError>;
  if (specs.empty() || specs.size() != modes.size())
    return R::failure(QuantityError::InvalidArgument);
  for (const BodyModeSpec& spec : specs) {
    auto built = makeBodyResonator(spec);
    if (!built.ok()) return R::failure(built.error());
  }
  for (std::size_t i = 0; i < specs.size(); ++i) {
    auto built = makeBodyResonator(specs[i]);
    if (!built.ok()) return R::failure(built.error());
    modes[i] = built.value();
  }
  return R::success(specs.size());
}

// Driving-point mobility Y(f) = v/F, metres per second per newton.
// f = 0 returns 0. A negative or non-finite frequency is InvalidArgument.
//
// Y = jω / (m (d_r + j d_i)) is expanded as
//   (ω d_i + j ω d_r) / (m (d_r² + d_i²)),
// which is the same quotient. The only divisor is m|D|². std::complex
// division is not used: MSVC's _Div (VC\Tools\MSVC\14.44 include\complex
// line 1007) divides by the real part on the |Im| < |Re| branch. At
// resonance d_r is 0, and a Release compile that constant-folds that
// zero — the body header test drives the mode at its own resonance —
// treats the library divide as C4723. |D|² is zero only when both parts
// are, which is RangeError rather than a silent 0/0.
[[nodiscard]] inline Result<std::complex<double>, QuantityError> bodyMobility(
    const BodyResonator& mode, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (!std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (frequencyHz == 0.0) return R::success(std::complex<double>{0.0, 0.0});
  const double omega = constants::twoPi * frequencyHz;
  const double realPart = mode.omega0 * mode.omega0 - omega * omega;
  const double imagPart = omega * mode.omega0 / mode.qualityFactor;
  const double modulus2 = realPart * realPart + imagPart * imagPart;
  const double divisor = mode.modalMassKg * modulus2;
  if (!(divisor > 0.0) || !std::isfinite(divisor))
    return R::failure(QuantityError::RangeError);
  const double scale = omega / divisor;
  const std::complex<double> mobility{scale * imagPart, scale * realPart};
  if (!std::isfinite(mobility.real()) || !std::isfinite(mobility.imag()))
    return R::failure(QuantityError::RangeError);
  return R::success(mobility);
}

// Sum of the driving-point terms. An empty table is InvalidArgument.
// Each term uses the algebraic quotient above; the sum adds those
// complexes and does not divide them. A negative or non-finite frequency
// is InvalidArgument. f = 0 returns 0.
[[nodiscard]] inline Result<std::complex<double>, QuantityError> bodyMobility(
    std::span<const BodyResonator> modes, double frequencyHz) {
  using R = Result<std::complex<double>, QuantityError>;
  if (modes.empty() || !std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  std::complex<double> sum{0.0, 0.0};
  for (const BodyResonator& mode : modes) {
    auto term = bodyMobility(mode, frequencyHz);
    if (!term.ok()) return R::failure(term.error());
    const std::complex<double>& value = term.value();
    sum += value;
  }
  if (!std::isfinite(sum.real()) || !std::isfinite(sum.imag()))
    return R::failure(QuantityError::RangeError);
  return R::success(sum);
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
    // X = V / (j ω) = v_im/ω - j v_re/ω. n ≥ 1 and P > 0, so ω > 0.
    // Written out for the same reason as bodyMobility: the divisor would
    // otherwise be complex(0, ω), and MSVC's complex divide splits on
    // that zero real part.
    if (!(omega > 0.0) || !std::isfinite(omega))
      return R::failure(QuantityError::RangeError);
    const std::complex<double>& harmonicVelocity = velocity.value();
    const std::complex<double> displacement{harmonicVelocity.imag() / omega,
                                            -harmonicVelocity.real() / omega};
    const double angle = omega * slip;
    sum += displacement * std::exp(std::complex<double>(0.0, angle));
  }
  const double metres = 2.0 * sum.real();
  if (!std::isfinite(metres)) return R::failure(QuantityError::RangeError);
  return R::success(metres);
}

// Coefficient of the steady velocity when the bridge force drives every
// mode in the table. Same sawtooth coefficient as one mode, times the
// summed mobility. An empty table is InvalidArgument.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
bodyVelocityCoefficient(std::span<const BodyResonator> modes,
                        const HelmholtzStroke& stroke, int harmonic) {
  using R = Result<std::complex<double>, QuantityError>;
  if (modes.empty() || !body_detail::usableStroke(stroke) || harmonic < 1 ||
      harmonic > 4096)
    return R::failure(QuantityError::InvalidArgument);
  const double frequency = static_cast<double>(harmonic) / stroke.period;
  auto mobility = bodyMobility(modes, frequency);
  if (!mobility.ok()) return R::failure(mobility.error());
  return R::success(mobility.value() *
                    body_detail::forceCoefficient(stroke, harmonic));
}

// Steady bridge-foot velocity under the Helmholtz force through the summed
// mobility. Linear in the modes: it equals the sum of the one-mode
// velocities.
[[nodiscard]] inline Result<double, QuantityError> bodyVelocity(
    std::span<const BodyResonator> modes, const HelmholtzStroke& stroke,
    double timeSeconds, int harmonics) {
  using R = Result<double, QuantityError>;
  if (modes.empty() || !std::isfinite(timeSeconds) || harmonics < 1 ||
      harmonics > 4096 || !body_detail::usableStroke(stroke))
    return R::failure(QuantityError::InvalidArgument);
  const double slip = body_detail::sinceSlip(stroke, timeSeconds);
  std::complex<double> sum{0.0, 0.0};
  for (int n = 1; n <= harmonics; ++n) {
    auto coefficient = bodyVelocityCoefficient(modes, stroke, n);
    if (!coefficient.ok()) return R::failure(coefficient.error());
    const double angle =
        constants::twoPi * static_cast<double>(n) * slip / stroke.period;
    sum += coefficient.value() * std::exp(std::complex<double>(0.0, angle));
  }
  const double velocity = 2.0 * sum.real();
  if (!std::isfinite(velocity)) return R::failure(QuantityError::RangeError);
  return R::success(velocity);
}

// Steady bridge-foot displacement, the integral of the summed velocity.
// The division by ω is real, as in the one-mode displacement.
[[nodiscard]] inline Result<double, QuantityError> bodyDisplacement(
    std::span<const BodyResonator> modes, const HelmholtzStroke& stroke,
    double timeSeconds, int harmonics) {
  using R = Result<double, QuantityError>;
  if (modes.empty() || !std::isfinite(timeSeconds) || harmonics < 1 ||
      harmonics > 4096 || !body_detail::usableStroke(stroke))
    return R::failure(QuantityError::InvalidArgument);
  const double slip = body_detail::sinceSlip(stroke, timeSeconds);
  std::complex<double> sum{0.0, 0.0};
  for (int n = 1; n <= harmonics; ++n) {
    auto velocity = bodyVelocityCoefficient(modes, stroke, n);
    if (!velocity.ok()) return R::failure(velocity.error());
    const double omega =
        constants::twoPi * static_cast<double>(n) / stroke.period;
    if (!(omega > 0.0) || !std::isfinite(omega))
      return R::failure(QuantityError::RangeError);
    const std::complex<double>& harmonicVelocity = velocity.value();
    const std::complex<double> displacement{harmonicVelocity.imag() / omega,
                                            -harmonicVelocity.real() / omega};
    const double angle = omega * slip;
    sum += displacement * std::exp(std::complex<double>(0.0, angle));
  }
  const double metres = 2.0 * sum.real();
  if (!std::isfinite(metres)) return R::failure(QuantityError::RangeError);
  return R::success(metres);
}

// Rayleigh's Helmholtz resonator for a cavity with one effective neck.
// A is the caller's total opening area (both f-holes, if that is what was
// measured). L is the effective neck length: an end correction is the
// caller's responsibility and is not added here. V is the cavity volume.
// Body length is none of these three.
//
//   Ma = ρ L / A,   Ca = V / (ρ c²),   ω = 1/√(Ma Ca) = c √(A / (V L)).
//
// c = √(γ R T) and ρ = p / (R T) from the dry-air model. No violin, viola,
// cello, or bass area, volume, or neck is stored. The frequency is not
// written into a BodyResonator.
inline constexpr const char* kCavityHelmholtzSource =
    "Rayleigh Helmholtz resonator: omega = c * sqrt(A / (V * L)), "
    "Ma = rho * L / A, Ca = V / (rho * c * c). "
    "c = sqrt(gamma * R * T) and rho = p / (R * T) from physics/medium.h "
    "and physics/impedance.h. A, V, and L are caller arguments. L already "
    "includes any end correction; none is added here. No violin, viola, "
    "cello, or bass f-hole, volume, or neck is stored. The frequency is "
    "not inserted into the body-mode sum.";

// Opening area (m²), cavity volume (m³), effective neck (m), and the
// static air state. Pressure and temperature are the dry-air arguments,
// as on a reed bore.
struct CavityHelmholtzSpec {
  double fHoleAreaM2;
  double volumeM3;
  double effectiveNeckMeters;
  double airPressurePa;
  double temperatureKelvin;
};

// acousticMass is Ma (kg/m⁴). acousticCompliance is Ca (m³/Pa).
struct CavityHelmholtz {
  double fHoleAreaM2 = 0.0;
  double volumeM3 = 0.0;
  double effectiveNeckMeters = 0.0;
  double soundSpeedMetersPerSecond = 0.0;
  double airDensityKgPerM3 = 0.0;
  double acousticMass = 0.0;
  double acousticCompliance = 0.0;
  double frequencyHz = 0.0;
  const char* source = nullptr;
};

// InvalidArgument if area, volume, or neck is non-finite or not strictly
// positive. Air-density and sound-speed errors propagate (non-positive
// pressure or temperature is DomainError). RangeError if Ma, Ca, or the
// frequency overflows.
[[nodiscard]] inline Result<CavityHelmholtz, QuantityError> makeCavityHelmholtz(
    const CavityHelmholtzSpec& spec) {
  using R = Result<CavityHelmholtz, QuantityError>;
  const double area = spec.fHoleAreaM2;
  const double volume = spec.volumeM3;
  const double neck = spec.effectiveNeckMeters;
  if (!std::isfinite(area) || !(area > 0.0) || !std::isfinite(volume) ||
      !(volume > 0.0) || !std::isfinite(neck) || !(neck > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto density = airDensity(PressurePascals{spec.airPressurePa},
                            TemperatureKelvin{spec.temperatureKelvin});
  if (!density.ok()) return R::failure(density.error());
  auto air = makeAir(TemperatureKelvin{spec.temperatureKelvin});
  if (!air.ok()) return R::failure(air.error());
  auto speed = speedOfSound(air.value());
  if (!speed.ok()) return R::failure(speed.error());
  const double rho = density.value().value();
  const double c = speed.value().value();
  const double acousticMass = rho * neck / area;
  const double acousticCompliance = volume / (rho * c * c);
  const double product = acousticMass * acousticCompliance;
  if (!(product > 0.0) || !std::isfinite(product))
    return R::failure(QuantityError::RangeError);
  const double omega = 1.0 / std::sqrt(product);
  const double frequency = omega / constants::twoPi;
  if (!std::isfinite(acousticMass) || !std::isfinite(acousticCompliance) ||
      !std::isfinite(omega) || !std::isfinite(frequency) || !(frequency > 0.0))
    return R::failure(QuantityError::RangeError);
  CavityHelmholtz cavity;
  cavity.fHoleAreaM2 = area;
  cavity.volumeM3 = volume;
  cavity.effectiveNeckMeters = neck;
  cavity.soundSpeedMetersPerSecond = c;
  cavity.airDensityKgPerM3 = rho;
  cavity.acousticMass = acousticMass;
  cavity.acousticCompliance = acousticCompliance;
  cavity.frequencyHz = frequency;
  cavity.source = kCavityHelmholtzSource;
  return R::success(cavity);
}

}  // namespace mmathen
