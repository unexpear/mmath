#pragma once

// Struck free bar (glockenspiel, celesta, tubular bells): a uniform bar
// or tube on two soft supports, struck by a hard or felt mallet, heard
// through its own radiation. Modal; no samples.
//
// Flexure. Free-free Euler-Bernoulli shapes
//   phi_n(x) = cosh b x + cos b x - s_n (sinh b x + sin b x),
//   s_n = (cosh bL - cos bL) / (sinh bL - sin bL),
// b_n L = 4.7300, 7.8532, 10.9956, 14.1372, (2n+1) pi / 2 after that
// (Rayleigh, Theory of Sound, vol. 1, sec. 171-174), normalised to
// integral phi^2 dx = L. Frequencies with the first-order rotary-inertia
// and shear correction of Timoshenko beam theory,
//   w_n = w_EB,n / sqrt(1 + (b_n r)^2 (1 + E / (kappa G))),
// r the radius of gyration, kappa the shear coefficient (Cowper, J. Appl.
// Mech. 33, 335-340, 1966: 10(1+nu)/(12+11nu) rectangle, 6(1+nu)(1+m^2)^2
// / ((7+6nu)(1+m^2)^2 + (20+12nu)m^2) tube, m = ri/ro). The first-order
// form is the low-(b r) expansion of the Timoshenko frequency equation
// (Han, Benaroya and Wei, JSV 225(5), 935-988, 1999); VERIFY above
// b r = 0.3 (the top glockenspiel bars reach about 0.25).
// Torsion (rectangular bars): Saint-Venant, free-free shapes cos(n pi x/L),
// w = (n pi / L) sqrt(G K_t / (rho I_p)), K_t = w h^3 (1/3 - 0.21 (h/w)
// (1 - h^4 / (12 w^4))) (Roark's Formulas for Stress and Strain, 7th ed.,
// table 10.7, case 4).
//
// Strike. Hertz contact of a sphere of radius R on the bar face,
// F = K d^(3/2) (1 + lambda d'), K = (4/3) E* sqrt(R),
// 1/E* = (1 - nu1^2)/E1 + (1 - nu2^2)/E2 (Johnson, Contact Mechanics,
// 1985, sec. 4.2), with Hunt-Crossley damping lambda (Hunt and Crossley,
// J. Appl. Mech. 42, 440-445, 1975). A felt mallet passes its own power-law
// stiffness instead (FeltHammer values). The mallet is a point mass; the
// strike point (xs, ys) drives flexure by phi_n(xs) and torsion by
// ys psi_n(xs).
//
// Radiation. Each element of a narrow bar moving normal to its face pushes
// the air's added mass m_a = rho0 pi (w/2)^2 per length (a strip of width w;
// a cylinder of radius a moving sideways: rho0 pi a^2; Lamb, Hydrodynamics,
// 6th ed., sec. 71-72) and radiates as a point dipole of that force,
// p = cos(alpha) / (4 pi r c) dF/dt (Morse and Ingard, Theoretical
// Acoustics, sec. 7.1). Summed along the bar with the phase
// exp(i k x sin(theta) cos(phi)) this is the modal far field; its sphere
// integral gives each mode's radiation damping. Compact-section only: VERIFY
// above k w / 2 = 1. Torsion radiates from two half-strips moving in
// opposition (lateral phase 2 sin(k w sin(theta) sin(phi) / 4)).
//
// Losses. sigma_n = w_n / (2 Q_int) + sigma_rad,n + sigma_mount,n. Q_int is
// the material plus everything that also damps the fundamental; the
// supports sit at the fundamental's nodes (0.224 L) and add
// sigma_mount,n = c_s (phi_n(x_s1)^2 + phi_n(x_s2)^2) / (2 rho A L) for a
// support dashpot c_s (N s / m). Both are the caller's (TUNED per
// instrument).

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../physics/oscillator.h"
#include "../render/voice.h"
#include "../units/physical_units.h"

namespace mmathen {

struct BarMaterial {
  double youngsPa = 0.0;
  double densityKgPerM3 = 0.0;
  double poisson = 0.3;
};

// Cross-section: rectangle (width x thickness, struck on the wide face) or
// round tube (outer and inner radius; torsion not modelled).
struct BarSection {
  bool tube = false;
  double widthM = 0.0, thicknessM = 0.0;            // rectangle
  double outerRadiusM = 0.0, innerRadiusM = 0.0;    // tube
};

struct BarLosses {
  double internalQ = 0.0;           // Q of the material and its fixings
  double supportDashpotNsPerM = 0.0;  // each of the two supports
  double supportFraction = 0.2242;    // fundamental node (Rayleigh: 0.2242 L)
};

struct BarMallet {
  double massKg = 0.0;
  double stiffness = 0.0;   // N / m^exponent
  double exponent = 1.5;
  double dampingPerMeterPerSecond = 0.0;  // Hunt-Crossley lambda (s/m)
};

// Hertz stiffness K = (4/3) E* sqrt(R) of a sphere (radius R) on a flat
// bar, E* from both materials (Johnson 1985, eq. 4.22).
[[nodiscard]] inline double hertzSphereOnFlatStiffness(double radiusM, double e1, double nu1,
                                                       double e2, double nu2) {
  const double inv = (1.0 - nu1 * nu1) / e1 + (1.0 - nu2 * nu2) / e2;
  return 4.0 / 3.0 * std::sqrt(radiusM) / inv;
}

namespace bar_detail {
inline constexpr std::array<double, 4> kFreeFreeBL = {4.730040745, 7.853204624, 10.99560784, 14.13716549};
inline double betaL(int n) {  // n = 1, 2, ...
  return n <= 4 ? kFreeFreeBL[static_cast<std::size_t>(n - 1)] : (2.0 * n + 1.0) * constants::pi / 2.0;
}
// Unnormalised free-free shape at x / L = u; evaluated in a form that
// stays finite for large b L (cosh - s sinh cancels).
inline double shapeRaw(int n, double u) {
  const double bl = betaL(n), bx = bl * u;
  const double s = (std::cosh(bl) - std::cos(bl)) / (std::sinh(bl) - std::sin(bl));
  // cosh bx - s sinh bx = ((1 - s) e^{bx} + (1 + s) e^{-bx}) / 2
  return 0.5 * ((1.0 - s) * std::exp(bx) + (1.0 + s) * std::exp(-bx)) + std::cos(bx) - s * std::sin(bx);
}
}  // namespace bar_detail

struct BarSpec {
  BarMaterial material;
  BarSection section;
  double lengthM = 0.0;
};

struct BarMode {
  bool torsion = false;
  int index = 0;
  double hz = 0.0;
  double sigma = 0.0;          // total decay rate, 1/s
  double radiationSigma = 0.0;
  std::vector<double> shape;   // normalised shape sampled at kShapePoints
};

inline constexpr int kBarShapePoints = 241;

[[nodiscard]] inline double barArea(const BarSection& s) {
  return s.tube ? constants::pi * (s.outerRadiusM * s.outerRadiusM - s.innerRadiusM * s.innerRadiusM)
                : s.widthM * s.thicknessM;
}
[[nodiscard]] inline double barGyrationRadius(const BarSection& s) {
  if (s.tube) return 0.5 * std::sqrt(s.outerRadiusM * s.outerRadiusM + s.innerRadiusM * s.innerRadiusM);
  return s.thicknessM / std::sqrt(12.0);
}
[[nodiscard]] inline double barShearCoefficient(const BarSection& s, double nu) {
  if (!s.tube) return 10.0 * (1.0 + nu) / (12.0 + 11.0 * nu);
  const double m = s.innerRadiusM / s.outerRadiusM, m2 = (1.0 + m * m) * (1.0 + m * m);
  return 6.0 * (1.0 + nu) * m2 / ((7.0 + 6.0 * nu) * m2 + (20.0 + 12.0 * nu) * m * m);
}
// Added air mass per length for motion normal to the struck face.
[[nodiscard]] inline double barAddedMassPerLength(const BarSection& s, double rho0) {
  return s.tube ? rho0 * constants::pi * s.outerRadiusM * s.outerRadiusM
                : rho0 * constants::pi * 0.25 * s.widthM * s.widthM;
}

// Flexural frequency of mode n (1-based), Timoshenko first order.
[[nodiscard]] inline double barFlexuralHz(const BarSpec& b, int n) {
  const double E = b.material.youngsPa, rho = b.material.densityKgPerM3, nu = b.material.poisson;
  const double G = E / (2.0 * (1.0 + nu)), kap = barShearCoefficient(b.section, nu);
  const double r = barGyrationRadius(b.section), beta = bar_detail::betaL(n) / b.lengthM;
  const double eb = beta * beta * r * std::sqrt(E / rho) / constants::twoPi;
  return eb / std::sqrt(1.0 + (beta * r) * (beta * r) * (1.0 + E / (kap * G)));
}
[[nodiscard]] inline double barTorsionHz(const BarSpec& b, int n) {
  if (b.section.tube) return 0.0;
  const double w = b.section.widthM, h = b.section.thicknessM;
  const double a = std::max(w, h), t = std::min(w, h);
  const double kt = a * t * t * t * (1.0 / 3.0 - 0.21 * (t / a) * (1.0 - std::pow(t / a, 4) / 12.0));
  const double ip = w * h * (w * w + h * h) / 12.0;
  const double G = b.material.youngsPa / (2.0 * (1.0 + b.material.poisson));
  return n / (2.0 * b.lengthM) * std::sqrt(G * kt / (b.material.densityKgPerM3 * ip));
}
// Length whose first flexural mode is hz (bisection), or 0.
[[nodiscard]] inline double barLengthFor(const BarSpec& like, double hz) {
  BarSpec b = like;
  double lo = 1e-3, hi = 5.0;
  for (int i = 0; i < 200; ++i) {
    b.lengthM = 0.5 * (lo + hi);
    if (barFlexuralHz(b, 1) > hz) lo = b.lengthM; else hi = b.lengthM;
  }
  return 0.5 * (lo + hi);
}

// Observer: distance r, polar angle theta from the struck-face normal and
// azimuth phi from the bar axis.
struct BarListener {
  double distanceM = 1.0;
  double polarRad = 0.0;
  double azimuthRad = 0.0;
  double airDensity = 1.204;  // 20 C (Kinsler et al., Fundamentals of Acoustics, table)
  double soundSpeed = 343.2;
};

namespace bar_detail {
// Dipole line integral along the bar, J = integral shape(x) e^{i k (x - L/2) s} dx.
inline std::complex<double> lineIntegral(const std::vector<double>& shape, double L, double k, double s) {
  std::complex<double> sum{};
  const int N = static_cast<int>(shape.size());
  const double dx = L / (N - 1);
  for (int i = 0; i < N; ++i) {
    const double x = i * dx, wgt = (i == 0 || i == N - 1) ? 0.5 : 1.0;
    sum += wgt * shape[static_cast<std::size_t>(i)] * std::polar(1.0, k * (x - 0.5 * L) * s);
  }
  return sum * dx;
}
// Far-field factor per unit modal acceleration rate for a direction:
// flexure m_a J cos(theta); torsion m_half (w/4) J cos(theta) 2 sin(k w s_y / 4).
inline double directivity(const BarMode& m, const BarSpec& b, double rho0, double k, double th, double ph) {
  const double sx = std::sin(th) * std::cos(ph), sy = std::sin(th) * std::sin(ph), cz = std::cos(th);
  const auto J = lineIntegral(m.shape, b.lengthM, k, sx);
  if (!m.torsion) return barAddedMassPerLength(b.section, rho0) * std::abs(J) * cz;
  const double w = b.section.widthM, half = rho0 * constants::pi * (w / 4.0) * (w / 4.0);
  return half * (w / 4.0) * std::abs(J) * cz * 2.0 * std::sin(k * w * sy / 4.0);
}
}  // namespace bar_detail

// Modes below maxHz with their losses. Radiation damping from the dipole
// field: sigma_rad = P / (2 M w^2) per unit modal displacement (P the
// sphere-integrated radiated power for unit modal acceleration amplitude
// times w^2, M the modal mass or inertia).
[[nodiscard]] inline Result<std::vector<BarMode>, QuantityError> barModes(
    const BarSpec& b, const BarLosses& loss, double maxHz, const BarListener& air = BarListener{}) {
  using R = Result<std::vector<BarMode>, QuantityError>;
  const BarMaterial& mat = b.material;
  if (!(mat.youngsPa > 0.0) || !(mat.densityKgPerM3 > 0.0) || !(b.lengthM > 0.0) ||
      !(loss.internalQ > 0.0) || loss.supportDashpotNsPerM < 0.0 || !(maxHz > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (b.section.tube ? !(b.section.outerRadiusM > b.section.innerRadiusM && b.section.innerRadiusM >= 0.0)
                     : !(b.section.widthM > 0.0 && b.section.thicknessM > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double A = barArea(b.section), L = b.lengthM, rho = mat.densityKgPerM3;
  std::vector<BarMode> out;
  auto finish = [&](BarMode m, double inertia) {
    const double w = constants::twoPi * m.hz, k = w / air.soundSpeed;
    // Sphere integral of the dipole intensity (midpoint rule, 48 x 48).
    const int NT = 48, NP = 48;
    double P = 0.0;
    for (int i = 0; i < NT; ++i) {
      const double th = (i + 0.5) * constants::pi / NT;
      for (int j = 0; j < NP; ++j) {
        const double ph = (j + 0.5) * constants::twoPi / NP;
        const double d = bar_detail::directivity(m, b, air.airDensity, k, th, ph);
        P += d * d * std::sin(th);
      }
    }
    P *= (constants::pi / NT) * (constants::twoPi / NP);
    // |p| r = k d |a| / (4 pi), mean intensity |p|^2 / (2 rho0 c), |a| = w^2 |q|.
    const double power = k * k * w * w * w * w * P / (16.0 * constants::pi * constants::pi * 2.0 * air.airDensity * air.soundSpeed);
    const double energy = 0.5 * inertia * w * w;
    m.radiationSigma = power / (2.0 * energy);
    const int N = kBarShapePoints;
    auto at = [&](double u) {
      const double pos = u * (N - 1);
      const int i = std::min(N - 2, static_cast<int>(pos));
      const double f = pos - i;
      return (1.0 - f) * m.shape[static_cast<std::size_t>(i)] + f * m.shape[static_cast<std::size_t>(i + 1)];
    };
    const double s1 = at(loss.supportFraction), s2 = at(1.0 - loss.supportFraction);
    const double scale = m.torsion ? 0.0 : 1.0;  // supports sit on the centreline: no torsional drag
    m.sigma = w / (2.0 * loss.internalQ) + m.radiationSigma +
              scale * loss.supportDashpotNsPerM * (s1 * s1 + s2 * s2) / (2.0 * inertia);
    if (!(m.sigma < w)) return false;
    out.push_back(std::move(m));
    return true;
  };
  for (int n = 1; n <= 40; ++n) {
    const double hz = barFlexuralHz(b, n);
    if (hz > maxHz) break;
    BarMode m;
    m.index = n;
    m.hz = hz;
    m.shape.resize(kBarShapePoints);
    double norm = 0.0;
    for (int i = 0; i < kBarShapePoints; ++i) {
      const double v = bar_detail::shapeRaw(n, static_cast<double>(i) / (kBarShapePoints - 1));
      m.shape[static_cast<std::size_t>(i)] = v;
      norm += v * v * ((i == 0 || i == kBarShapePoints - 1) ? 0.5 : 1.0);
    }
    norm = std::sqrt(norm / (kBarShapePoints - 1));
    for (double& v : m.shape) v /= norm;
    if (!finish(std::move(m), rho * A * L)) return R::failure(QuantityError::DomainError);
  }
  if (!b.section.tube) {
    const double ip = b.section.widthM * b.section.thicknessM *
                      (b.section.widthM * b.section.widthM + b.section.thicknessM * b.section.thicknessM) / 12.0;
    for (int n = 1; n <= 40; ++n) {
      const double hz = barTorsionHz(b, n);
      if (hz > maxHz) break;
      BarMode m;
      m.torsion = true;
      m.index = n;
      m.hz = hz;
      m.shape.resize(kBarShapePoints);
      for (int i = 0; i < kBarShapePoints; ++i)
        m.shape[static_cast<std::size_t>(i)] =
            std::sqrt(2.0) * std::cos(n * constants::pi * i / (kBarShapePoints - 1.0));
      if (!finish(std::move(m), rho * ip * L)) return R::failure(QuantityError::DomainError);
    }
  }
  return R::success(std::move(out));
}

// Pressure amplitude at the listener per unit modal acceleration amplitude,
// summing the point dipoles along the bar with their exact distance and
// the dipole's near-field term, |p| = |sum f dx cos(a) (1 + i k R) e^{-ikR}
// / (4 pi R^2)| (Morse and Ingard sec. 7.1); a close listener hears the
// near part of the bar without the far field's cancellation along it.
// The modal phase is dropped (each mode's waveform keeps its own phase).
[[nodiscard]] inline double barListenerGain(const BarMode& m, const BarSpec& b, const BarListener& li) {
  const double k = constants::twoPi * m.hz / li.soundSpeed, L = b.lengthM;
  const double ox = li.distanceM * std::sin(li.polarRad) * std::cos(li.azimuthRad);
  const double oy = li.distanceM * std::sin(li.polarRad) * std::sin(li.azimuthRad);
  const double oz = li.distanceM * std::cos(li.polarRad);
  const int N = static_cast<int>(m.shape.size());
  const double dx = L / (N - 1);
  auto field = [&](double y0, double weight) {
    std::complex<double> sum{};
    for (int i = 0; i < N; ++i) {
      const double x = i * dx - 0.5 * L, rx = ox - x, ry = oy - y0;
      const double R = std::sqrt(rx * rx + ry * ry + oz * oz), ca = oz / R;
      const double wgt = (i == 0 || i == N - 1) ? 0.5 : 1.0;
      sum += wgt * m.shape[static_cast<std::size_t>(i)] * ca * std::complex<double>(1.0, k * R) *
             std::polar(1.0, -k * R) / (4.0 * constants::pi * R * R);
    }
    return weight * sum * dx;
  };
  if (!m.torsion) return barAddedMassPerLength(b.section, li.airDensity) * std::abs(field(0.0, 1.0));
  const double w = b.section.widthM, half = li.airDensity * constants::pi * (w / 4.0) * (w / 4.0);
  return half * (w / 4.0) * std::abs(field(w / 4.0, 1.0) - field(-w / 4.0, 1.0));
}

// Struck bar voice: modal oscillators (exact step), explicit contact, run at
// `oversample` times the output rate with boxcar decimation; output is the
// far-field pressure (Pa) at the listener.
class StruckBarVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<StruckBarVoice, QuantityError> create(
      const BarSpec& bar, const BarLosses& loss, const BarMallet& mallet, double speedMps,
      double strikeFraction, double strikeOffsetFraction, SampleRateHz rate,
      const BarListener& listener = BarListener{}, int oversample = 4) {
    using R = Result<StruckBarVoice, QuantityError>;
    if (!(mallet.massKg > 0.0) || !(mallet.stiffness > 0.0) || !(mallet.exponent >= 1.0) ||
        mallet.dampingPerMeterPerSecond < 0.0 || !(speedMps > 0.0) || !(strikeFraction >= 0.0) ||
        !(strikeFraction <= 1.0) || std::fabs(strikeOffsetFraction) > 0.5 || oversample < 1 ||
        !(rate.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double fs = rate.value() * oversample;
    auto modes = barModes(bar, loss, std::min(20000.0, 0.45 * rate.value()), listener);
    if (!modes.ok()) return R::failure(modes.error());
    StruckBarVoice v;
    v.rate_ = rate;
    v.factor_ = oversample;
    v.dt_ = 1.0 / fs;
    v.mallet_ = mallet;
    v.speed_ = speedMps;
    const double A = barArea(bar.section), rho = bar.material.densityKgPerM3, L = bar.lengthM;
    const double ys = strikeOffsetFraction * (bar.section.tube ? 2.0 * bar.section.outerRadiusM : bar.section.widthM);
    for (const BarMode& m : modes.value()) {
      Mode o;
      o.omega = constants::twoPi * m.hz;
      o.sigma = m.sigma;
      const double pos = strikeFraction * (kBarShapePoints - 1);
      const int i = std::min(kBarShapePoints - 2, static_cast<int>(pos));
      const double f = pos - i;
      const double at = (1.0 - f) * m.shape[static_cast<std::size_t>(i)] + f * m.shape[static_cast<std::size_t>(i + 1)];
      if (m.torsion) {
        const double ip = bar.section.widthM * bar.section.thicknessM *
                          (bar.section.widthM * bar.section.widthM + bar.section.thicknessM * bar.section.thicknessM) / 12.0;
        o.drive = ys * at / (rho * ip * L);  // angular accel per newton
        o.contact = ys * at;                 // displacement at the strike per unit angle
      } else {
        o.drive = at / (rho * A * L);
        o.contact = at;
      }
      o.radiate = barListenerGain(m, bar, listener);
      v.modes_.push_back(o);
    }
    v.reset();
    return R::success(std::move(v));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }
  [[nodiscard]] std::size_t modeCount() const noexcept { return modes_.size(); }
  [[nodiscard]] double modeHz(std::size_t i) const noexcept { return i < modes_.size() ? modes_[i].omega / constants::twoPi : 0.0; }
  [[nodiscard]] double modeSigma(std::size_t i) const noexcept { return i < modes_.size() ? modes_[i].sigma : 0.0; }
  [[nodiscard]] double contactSeconds() const noexcept { return contactTime_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const auto count = static_cast<std::size_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    for (std::size_t frame = 0; frame < count; ++frame) {
      double sum = 0.0;
      for (int sub = 0; sub < factor_; ++sub) {
        double u = 0.0, ur = 0.0;
        for (const Mode& m : modes_) { u += m.contact * m.s.displacement; ur += m.contact * m.s.velocity; }
        const double pen = y_ - u;
        double F = 0.0;
        if (pen > 0.0 && inContactPossible_) {
          F = mallet_.stiffness * std::pow(pen, mallet_.exponent) *
              std::max(0.0, 1.0 + mallet_.dampingPerMeterPerSecond * (vy_ - ur));
          contactTime_ += dt_;
        } else if (contactTime_ > 0.0 && pen <= 0.0 && vy_ < 0.0) {
          inContactPossible_ = false;  // the player lifts the mallet after the bounce
        }
        double p = 0.0;
        for (Mode& m : modes_) {
          const double force = F * m.drive;
          const double acc = force - 2.0 * m.sigma * m.s.velocity - m.omega * m.omega * m.s.displacement;
          auto st = stepConstantForceOscillator(m.s, m.omega, m.sigma, force, dt_);
          if (!st.ok()) return R::failure(st.error());
          m.s = st.value();
          p += m.radiate * acc;
        }
        vy_ += -F / mallet_.massKg * dt_;
        y_ += vy_ * dt_;
        if (!std::isfinite(p) || !std::isfinite(y_)) return R::failure(QuantityError::RangeError);
        sum += p;
      }
      out[frame] = sum / factor_;
    }
    return R::success(n);
  }

  void reset() override {
    for (Mode& m : modes_) m.s = {};
    y_ = 0.0;
    vy_ = speed_;
    contactTime_ = 0.0;
    inContactPossible_ = true;
  }

 private:
  struct Mode {
    double omega = 0.0, sigma = 0.0, drive = 0.0, contact = 0.0, radiate = 0.0;
    OscillatorState s;
  };
  std::vector<Mode> modes_;
  SampleRateHz rate_{48000.0};
  int factor_ = 1;
  double dt_ = 0.0, speed_ = 0.0, y_ = 0.0, vy_ = 0.0, contactTime_ = 0.0;
  bool inContactPossible_ = true;
  BarMallet mallet_;
};

}  // namespace mmathen
