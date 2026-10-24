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
// bodyVelocity is the rigid-bridge Helmholtz force from bow.h, open-loop.
// movingBridgeModes re-solves the string: the nut stays fixed and the
// bridge end is the caller’s mobility. A resistance R uses Y = 1/R. A
// mode table uses bodyMobility. Neither stores a measured admittance.
// The steady sawtooth is not rewritten, and a double-slip wolf cycle is
// not stepped.
//
// bridgeResistance is Re(1/Y) at a caller frequency. At one mode’s own
// resonance it equals that mode’s r = m ω0 / Q, and that value may be
// passed to schellengMinimumForce. No numerical R is stored for any
// instrument.
//
// The cavity air resonance is makeCavityHelmholtz: Rayleigh's resonator
// from a caller-supplied f-hole area, cavity volume, and effective neck
// length, with c and ρ from the dry-air model. It is not a row of the
// modal sum. Inserting it there would need a modal mass and a Q, and
// neither is invented. Body length is not the neck, the volume, or the
// area.
//
// coupleBodyModes adds one mutual stiffness between two caller-supplied
// modes. The diagonals stay those modes' own stiffnesses. This is the
// off-diagonal term an A0–B1 pair needs. It does not store an A0 or a B1
// frequency, and it does not turn the cavity's acoustic mass into a
// mechanical modal mass.
//
// compactMonopolePressure is the spherical wave of a compact source,
//   P = j ω ρ U exp(-j k r) / (4 π r),
// with k = ω/c. U is the caller's volume velocity, or the caller's
// radiating area times a normal velocity. Distance is an argument. No
// violin, viola, cello, or bass radiating area is stored. Body length is
// not the distance.
// Hammer and reed are not this model.
//
// VERIFIED against the modal admittance and against the oscillator ODE
// driven by the sawtooth's Fourier series. UNVALIDATED against a measured
// bridge admittance or a radiated tone.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <span>
#include <vector>

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
    "makeCavityHelmholtz and is not a term of this sum. A mutual stiffness "
    "between two modes is coupleBodyModes. Far-field pressure is "
    "compactMonopolePressure; no radiating area is stored.";

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

// Two oscillators with the caller's uncoupled stiffnesses on the diagonal
// and one mutual stiffness K (N/m) off the diagonal:
//   (k1 - ω² m1)(k2 - ω² m2) - K² = 0.
// K = 0 leaves the resonances uncoupled. The sign of K does not move the
// frequencies. The sum of the squared coupled angular frequencies equals
// the sum of the uncoupled ones. A root that is not a positive frequency
// (K² ≥ k1 k2) is DomainError. This is not a stored A0–B1 pair.
inline constexpr const char* kCoupledModesSource =
    "Two oscillators with uncoupled stiffnesses k = m omega0^2 and one "
    "mutual stiffness K: (k1 - omega^2 m1)(k2 - omega^2 m2) - K^2 = 0. "
    "K is a caller argument. No A0 or B1 frequency is stored. The cavity "
    "acoustic mass is not used as a mechanical modal mass.";

struct CoupledModePair {
  double lowerHz = 0.0;
  double upperHz = 0.0;
  double mutualStiffness = 0.0;
  const char* source = nullptr;
};

// InvalidArgument if K is non-finite or either mode has a non-positive
// mass or stiffness. DomainError if K² is at least k1 k2, so the lower
// root is not a positive frequency.
[[nodiscard]] inline Result<CoupledModePair, QuantityError> coupleBodyModes(
    const BodyResonator& first, const BodyResonator& second,
    double mutualStiffness) {
  using R = Result<CoupledModePair, QuantityError>;
  const double m1 = first.modalMassKg;
  const double m2 = second.modalMassKg;
  const double k1 = first.stiffness;
  const double k2 = second.stiffness;
  if (!std::isfinite(mutualStiffness) || !std::isfinite(m1) ||
      !std::isfinite(m2) || !std::isfinite(k1) || !std::isfinite(k2) ||
      !(m1 > 0.0) || !(m2 > 0.0) || !(k1 > 0.0) || !(k2 > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double stiffnessProduct = k1 * k2;
  const double coupling = mutualStiffness * mutualStiffness;
  if (!std::isfinite(stiffnessProduct) || !std::isfinite(coupling))
    return R::failure(QuantityError::RangeError);
  if (!(stiffnessProduct > coupling))
    return R::failure(QuantityError::DomainError);
  const double trace = m1 * k2 + m2 * k1;
  const double split = m1 * k2 - m2 * k1;
  // (m1 k2 - m2 k1)² + 4 m1 m2 K². A sum of squares, so it does not
  // cancel through zero the way trace² - 4 m1 m2 k1 k2 can.
  const double discriminant = split * split + 4.0 * m1 * m2 * coupling;
  const double massProduct = 2.0 * m1 * m2;
  if (!(discriminant >= 0.0) || !std::isfinite(discriminant) ||
      !(massProduct > 0.0) || !std::isfinite(massProduct) ||
      !std::isfinite(trace))
    return R::failure(QuantityError::RangeError);
  const double root = std::sqrt(discriminant);
  const double omega2Upper = (trace + root) / massProduct;
  const double omega2Lower = (trace - root) / massProduct;
  if (!(omega2Lower > 0.0) || !(omega2Upper >= omega2Lower) ||
      !std::isfinite(omega2Lower) || !std::isfinite(omega2Upper))
    return R::failure(QuantityError::DomainError);
  const double lowerHz = std::sqrt(omega2Lower) / constants::twoPi;
  const double upperHz = std::sqrt(omega2Upper) / constants::twoPi;
  if (!std::isfinite(lowerHz) || !std::isfinite(upperHz) || !(lowerHz > 0.0))
    return R::failure(QuantityError::RangeError);
  CoupledModePair pair;
  pair.lowerHz = lowerHz;
  pair.upperHz = upperHz;
  pair.mutualStiffness = mutualStiffness;
  pair.source = kCoupledModesSource;
  return R::success(pair);
}

// Compact monopole. P = j ω ρ U exp(-j k r) / (4 π r), k = ω/c.
// U is the caller's complex volume velocity (m³/s). The factor j exp(-jkr)
// is written with real sine and cosine, so the pressure is not a complex
// division. ρ and c are the dry-air model. No instrument area is stored.
inline constexpr const char* kMonopoleSource =
    "Compact monopole: P = j omega rho U exp(-j k r) / (4 pi r), "
    "k = omega/c. U is a caller volume velocity, or a caller radiating "
    "area times a normal velocity. Distance is a caller argument. "
    "rho and c are the dry-air model. No violin, viola, cello, or bass "
    "radiating area is stored.";

struct MonopoleField {
  std::complex<double> pressure{};
  std::complex<double> volumeVelocity{};
  double distanceMeters = 0.0;
  double frequencyHz = 0.0;
  double waveNumber = 0.0;
  double soundSpeedMetersPerSecond = 0.0;
  double airDensityKgPerM3 = 0.0;
  const char* source = nullptr;
};

// InvalidArgument if distance is not strictly positive, frequency is
// negative, or U is non-finite. Frequency 0 returns zero pressure.
// Air-density and sound-speed errors propagate. RangeError if the
// pressure overflows.
[[nodiscard]] inline Result<MonopoleField, QuantityError> compactMonopolePressure(
    std::complex<double> volumeVelocity, double distanceMeters,
    double frequencyHz, double airPressurePa, double temperatureKelvin) {
  using R = Result<MonopoleField, QuantityError>;
  if (!std::isfinite(distanceMeters) || !(distanceMeters > 0.0) ||
      !std::isfinite(frequencyHz) || frequencyHz < 0.0 ||
      !std::isfinite(volumeVelocity.real()) ||
      !std::isfinite(volumeVelocity.imag()))
    return R::failure(QuantityError::InvalidArgument);
  auto density = airDensity(PressurePascals{airPressurePa},
                            TemperatureKelvin{temperatureKelvin});
  if (!density.ok()) return R::failure(density.error());
  auto air = makeAir(TemperatureKelvin{temperatureKelvin});
  if (!air.ok()) return R::failure(air.error());
  auto speed = speedOfSound(air.value());
  if (!speed.ok()) return R::failure(speed.error());
  const double rho = density.value().value();
  const double c = speed.value().value();
  const double omega = constants::twoPi * frequencyHz;
  const double waveNumber = omega / c;
  std::complex<double> pressure{0.0, 0.0};
  if (frequencyHz != 0.0) {
    const double kr = waveNumber * distanceMeters;
    const double scale =
        omega * rho / (4.0 * constants::pi * distanceMeters);
    const double sine = std::sin(kr);
    const double cosine = std::cos(kr);
    const double realU = volumeVelocity.real();
    const double imagU = volumeVelocity.imag();
    pressure = std::complex<double>{scale * (sine * realU - cosine * imagU),
                                    scale * (cosine * realU + sine * imagU)};
  }
  if (!std::isfinite(waveNumber) || !std::isfinite(pressure.real()) ||
      !std::isfinite(pressure.imag()))
    return R::failure(QuantityError::RangeError);
  MonopoleField field;
  field.pressure = pressure;
  field.volumeVelocity = volumeVelocity;
  field.distanceMeters = distanceMeters;
  field.frequencyHz = frequencyHz;
  field.waveNumber = waveNumber;
  field.soundSpeedMetersPerSecond = c;
  field.airDensityKgPerM3 = rho;
  field.source = kMonopoleSource;
  return R::success(field);
}

// U = S v. Area is the caller's radiating area (m²). Normal velocity is
// the caller's surface velocity (m/s), not a bridge velocity and not a
// body length.
[[nodiscard]] inline Result<MonopoleField, QuantityError> compactMonopolePressure(
    double radiatingAreaM2, std::complex<double> normalVelocity,
    double distanceMeters, double frequencyHz, double airPressurePa,
    double temperatureKelvin) {
  using R = Result<MonopoleField, QuantityError>;
  if (!std::isfinite(radiatingAreaM2) || !(radiatingAreaM2 > 0.0) ||
      !std::isfinite(normalVelocity.real()) ||
      !std::isfinite(normalVelocity.imag()))
    return R::failure(QuantityError::InvalidArgument);
  const std::complex<double> volumeVelocity{
      radiatingAreaM2 * normalVelocity.real(),
      radiatingAreaM2 * normalVelocity.imag()};
  return compactMonopolePressure(volumeVelocity, distanceMeters, frequencyHz,
                                 airPressurePa, temperatureKelvin);
}

inline constexpr const char* kMovingBridgeSource =
    "Ideal string fixed at the nut and re-solved on a moving bridge: "
    "j sin(omega L/c) + Y Z cos(omega L/c) = 0, Z = mu c. "
    "A caller resistance uses Y = 1/R. A caller mode table uses "
    "bodyMobility. For a real R the oscillation is n c/(2L) when R > Z, "
    "with decay (c/L) artanh(Z/R), and (2n-1) c/(4L) when R < Z, with "
    "decay (c/L) artanh(R/Z). Velocity reflection "
    "lambda = (Z - R)/(Z + R) "
    "(Woodhouse, Euphonics 9.5.1: rigid is -1, and "
    "lambda is about -1 + 2Z/R when R is large beside Z). "
    "No measured bridge admittance is stored.";

// Free motion of the string on a moving bridge. decayPerSecond is σ in
// e^{j ω t} with ω = 2π f + j σ, so the amplitude decays as e^{-σ t}.
// reflection is (Z − R)/(Z + R) for a resistive bridge, and 0 when the
// termination is the modal sum.
struct MovingBridgeMode {
  double frequencyHz = 0.0;
  double decayPerSecond = 0.0;
  double reflection = 0.0;
  const char* source = nullptr;
};

namespace body_detail {

struct CourseWave {
  double length = 0.0;
  double speed = 0.0;
  double impedance = 0.0;
};

[[nodiscard]] inline Result<CourseWave, QuantityError> courseWave(
    const StringCourse& course) {
  using R = Result<CourseWave, QuantityError>;
  const double length = course.geometry.vibratingLength.value();
  const double tension = course.tension.value();
  const double density = course.material.linearDensity.value();
  if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(tension) ||
      !(tension > 0.0) || !std::isfinite(density) || !(density > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double speed = std::sqrt(tension / density);
  const double impedance = density * speed;
  if (!std::isfinite(speed) || !(speed > 0.0) || !std::isfinite(impedance) ||
      !(impedance > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(CourseWave{length, speed, impedance});
}

// Y(ω) for complex ω, the same algebraic quotient as bodyMobility on the
// real axis. The only divisor is m|D|².
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
mobilityAt(const BodyResonator& mode, std::complex<double> omega) {
  using R = Result<std::complex<double>, QuantityError>;
  const double wr = omega.real();
  const double wi = omega.imag();
  if (!std::isfinite(wr) || !std::isfinite(wi))
    return R::failure(QuantityError::InvalidArgument);
  const double omega0 = mode.omega0;
  const double damp = omega0 / mode.qualityFactor;
  const double w2r = wr * wr - wi * wi;
  const double w2i = 2.0 * wr * wi;
  const double realPart = omega0 * omega0 - w2r - wi * damp;
  const double imagPart = -w2i + wr * damp;
  const double modulus2 = realPart * realPart + imagPart * imagPart;
  const double divisor = mode.modalMassKg * modulus2;
  if (!(divisor > 0.0) || !std::isfinite(divisor))
    return R::failure(QuantityError::RangeError);
  const double realY = (wr * imagPart - wi * realPart) / divisor;
  const double imagY = (wr * realPart + wi * imagPart) / divisor;
  if (!std::isfinite(realY) || !std::isfinite(imagY))
    return R::failure(QuantityError::RangeError);
  return R::success(std::complex<double>{realY, imagY});
}

[[nodiscard]] inline Result<std::complex<double>, QuantityError> mobilityAt(
    std::span<const BodyResonator> modes, std::complex<double> omega) {
  using R = Result<std::complex<double>, QuantityError>;
  if (modes.empty()) return R::failure(QuantityError::InvalidArgument);
  std::complex<double> sum{0.0, 0.0};
  for (const BodyResonator& mode : modes) {
    auto term = mobilityAt(mode, omega);
    if (!term.ok()) return R::failure(term.error());
    sum += term.value();
  }
  if (!std::isfinite(sum.real()) || !std::isfinite(sum.imag()))
    return R::failure(QuantityError::RangeError);
  return R::success(sum);
}

[[nodiscard]] inline std::complex<double> bridgeCharacteristic(
    const CourseWave& wave, std::complex<double> omega,
    std::complex<double> mobility) {
  const std::complex<double> theta{omega.real() * wave.length / wave.speed,
                                   omega.imag() * wave.length / wave.speed};
  const std::complex<double> sine = std::sin(theta);
  const std::complex<double> cosine = std::cos(theta);
  const double zr = wave.impedance;
  const double yzr = mobility.real() * cosine.real() - mobility.imag() * cosine.imag();
  const double yzi = mobility.real() * cosine.imag() + mobility.imag() * cosine.real();
  return std::complex<double>{-sine.imag() + zr * yzr, sine.real() + zr * yzi};
}

[[nodiscard]] inline bool sameRoot(std::complex<double> a, std::complex<double> b) {
  const double scale =
      std::max(1.0, std::max(std::fabs(a.real()), std::fabs(b.real())));
  return std::fabs(a.real() - b.real()) <= 1e-5 * scale &&
         std::fabs(a.imag() - b.imag()) <= 1e-4 * scale;
}

// Newton on j sin(θ) + Y Z cos(θ) = 0. A failed mobility sample or a
// vanishing derivative abandons this guess.
[[nodiscard]] inline bool newtonBridgeRoot(
    const CourseWave& wave, std::span<const BodyResonator> modes,
    std::complex<double> guess, std::complex<double>& root) {
  std::complex<double> omega = guess;
  for (int iteration = 0; iteration < 40; ++iteration) {
    auto mobility = mobilityAt(modes, omega);
    if (!mobility.ok()) return false;
    const std::complex<double> value =
        bridgeCharacteristic(wave, omega, mobility.value());
    const double scale =
        std::max(1.0, std::hypot(omega.real(), omega.imag()));
    const double step = 1e-8 * scale;
    auto shifted = mobilityAt(modes, std::complex<double>{omega.real() + step,
                                                          omega.imag()});
    if (!shifted.ok()) return false;
    const std::complex<double> next =
        bridgeCharacteristic(wave, std::complex<double>{omega.real() + step,
                                                        omega.imag()},
                            shifted.value());
    const double derivativeReal = (next.real() - value.real()) / step;
    const double derivativeImag = (next.imag() - value.imag()) / step;
    const double denominator =
        derivativeReal * derivativeReal + derivativeImag * derivativeImag;
    if (!(denominator > 0.0) || !std::isfinite(denominator)) return false;
    const double moveReal =
        (value.real() * derivativeReal + value.imag() * derivativeImag) /
        denominator;
    const double moveImag =
        (value.imag() * derivativeReal - value.real() * derivativeImag) /
        denominator;
    if (!std::isfinite(moveReal) || !std::isfinite(moveImag)) return false;
    omega = std::complex<double>{omega.real() - moveReal,
                                 omega.imag() - moveImag};
    if (!(omega.real() > 0.0) || !std::isfinite(omega.real()) ||
        !std::isfinite(omega.imag()))
      return false;
    if (std::hypot(moveReal, moveImag) <= 1e-12 * scale &&
        std::hypot(value.real(), value.imag()) <= 1e-8)
      break;
  }
  auto mobility = mobilityAt(modes, omega);
  if (!mobility.ok()) return false;
  const std::complex<double> value =
      bridgeCharacteristic(wave, omega, mobility.value());
  if (!(std::hypot(value.real(), value.imag()) <= 1e-6)) return false;
  if (!(omega.real() > 0.0)) return false;
  if (omega.imag() < 0.0) {
    if (omega.imag() < -1e-8 * std::max(1.0, omega.real())) return false;
    omega = std::complex<double>{omega.real(), 0.0};
  }
  root = omega;
  return true;
}

}  // namespace body_detail

// Resistive part of the bridge impedance, Re(1/Y), at a real frequency.
// Y is bodyMobility. The quotient is G / |Y|²; there is no complex
// division. InvalidArgument for an empty table or a bad frequency.
// DomainError when that real part is not positive. At a single mode's
// own resonance this is the mode's r.
[[nodiscard]] inline Result<double, QuantityError> bridgeResistance(
    std::span<const BodyResonator> modes, double frequencyHz) {
  using R = Result<double, QuantityError>;
  auto mobility = bodyMobility(modes, frequencyHz);
  if (!mobility.ok()) return R::failure(mobility.error());
  const double conductance = mobility.value().real();
  const double susceptance = mobility.value().imag();
  const double modulus2 =
      conductance * conductance + susceptance * susceptance;
  if (!(modulus2 > 0.0) || !std::isfinite(modulus2))
    return R::failure(QuantityError::RangeError);
  const double resistance = conductance / modulus2;
  if (!std::isfinite(resistance) || !(resistance > 0.0))
    return R::failure(QuantityError::DomainError);
  return R::success(resistance);
}

[[nodiscard]] inline Result<double, QuantityError> bridgeResistance(
    const BodyResonator& mode, double frequencyHz) {
  const BodyResonator modes[] = {mode};
  return bridgeResistance(std::span<const BodyResonator>{modes, 1}, frequencyHz);
}

// Exact roots for a frequency-independent bridge resistance R (N·s/m).
// R > Z keeps the fixed-fixed pitches and adds decay. R < Z keeps the
// fixed-free pitches. R = Z is the matched termination: DomainError,
// there is no finite standing mode. modeIndex is 1-based.
// InvalidArgument: R not strictly positive and finite, or modeIndex < 1.
[[nodiscard]] inline Result<MovingBridgeMode, QuantityError>
resistiveBridgeMode(const StringCourse& course, double resistance,
                    int modeIndex) {
  using R = Result<MovingBridgeMode, QuantityError>;
  if (!std::isfinite(resistance) || !(resistance > 0.0) || modeIndex < 1 ||
      modeIndex > 4096)
    return R::failure(QuantityError::InvalidArgument);
  auto wave = body_detail::courseWave(course);
  if (!wave.ok()) return R::failure(wave.error());
  const double impedance = wave.value().impedance;
  const double speed = wave.value().speed;
  const double length = wave.value().length;
  if (resistance == impedance) return R::failure(QuantityError::DomainError);
  const double ratio = resistance > impedance ? impedance / resistance
                                             : resistance / impedance;
  const double decay = (speed / length) * std::atanh(ratio);
  const double harmonic = static_cast<double>(modeIndex);
  const double frequency =
      resistance > impedance
          ? harmonic * speed / (2.0 * length)
          : (2.0 * harmonic - 1.0) * speed / (4.0 * length);
  const double reflection =
      (impedance - resistance) / (impedance + resistance);
  if (!std::isfinite(decay) || !(decay > 0.0) || !std::isfinite(frequency) ||
      !(frequency > 0.0) || !std::isfinite(reflection))
    return R::failure(QuantityError::RangeError);
  return R::success(
      MovingBridgeMode{frequency, decay, reflection, kMovingBridgeSource});
}

// Lowest `count` free modes of the string terminated by the modal sum.
// Guesses march in wavenumber and Newton finishes each root. An empty
// table, a non-positive count, or too few converged roots is refused.
// bodyMobility's real-frequency quotient is not modified.
[[nodiscard]] inline Result<std::vector<MovingBridgeMode>, QuantityError>
movingBridgeModes(const StringCourse& course,
                  std::span<const BodyResonator> modes, int count) {
  using R = Result<std::vector<MovingBridgeMode>, QuantityError>;
  if (modes.empty() || count < 1 || count > 64)
    return R::failure(QuantityError::InvalidArgument);
  auto wave = body_detail::courseWave(course);
  if (!wave.ok()) return R::failure(wave.error());
  const double length = wave.value().length;
  const double speed = wave.value().speed;
  const double step = constants::pi / (16.0 * length);
  const double limit = (static_cast<double>(count) + 4.0) * constants::pi / length;
  std::vector<std::complex<double>> roots;
  for (double beta = step; beta < limit; beta += step) {
    std::complex<double> root;
    if (!body_detail::newtonBridgeRoot(
            wave.value(), modes, std::complex<double>{speed * beta, 0.0},
            root))
      continue;
    bool known = false;
    for (const std::complex<double>& found : roots) {
      if (body_detail::sameRoot(found, root)) {
        known = true;
        break;
      }
    }
    if (!known) roots.push_back(root);
  }
  if (static_cast<int>(roots.size()) < count)
    return R::failure(QuantityError::RangeError);
  for (std::size_t i = 1; i < roots.size(); ++i) {
    std::complex<double> value = roots[i];
    std::size_t j = i;
    while (j > 0 && roots[j - 1].real() > value.real()) {
      roots[j] = roots[j - 1];
      --j;
    }
    roots[j] = value;
  }
  std::vector<MovingBridgeMode> modesOut;
  modesOut.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const std::complex<double>& root = roots[static_cast<std::size_t>(i)];
    const double frequency = root.real() / constants::twoPi;
    const double decay = root.imag();
    if (!std::isfinite(frequency) || !(frequency > 0.0) || !std::isfinite(decay) ||
        decay < 0.0)
      return R::failure(QuantityError::RangeError);
    modesOut.push_back(MovingBridgeMode{frequency, decay, 0.0, kMovingBridgeSource});
  }
  return R::success(std::move(modesOut));
}

[[nodiscard]] inline Result<std::vector<MovingBridgeMode>, QuantityError>
movingBridgeModes(const StringCourse& course, const BodyResonator& mode,
                  int count) {
  const BodyResonator modes[] = {mode};
  return movingBridgeModes(course, std::span<const BodyResonator>{modes, 1},
                           count);
}

}  // namespace mmathen
