#pragma once

// D8 — the 1-D lossless wave equation u_tt = c^2 u_xx on [0, L] as a model
// object, with the analytical and modal rungs of the solver ladder.
// (FDTD: fdtd1d.h; waveguide: waveguide1d.h. Same object, solver swaps.)
//
// MODEL CARD (VALIDATION.md). Assumptions: linear, uniform c, ideal
// boundaries. Boundaries: Fixed (Dirichlet, u = 0) or Free (Neumann,
// u_x = 0). Field u is transverse displacement (m) for strings, acoustic
// pressure (Pa, relative) for pipes — open pipe end = Fixed (p = 0), closed
// end = Free (rigid wall, p_x = 0). Supported: any L, c > 0; initial shapes
// limited to closed-form kinds (pluck, single mode) with zero initial
// velocity. Optional extensions, each verified only on the rungs that accept
// it: two-parameter loss (D9, LossModel) and hinged bending stiffness (D10,
// Fixed-Fixed only) — modal and FDTD accept both; the analytical and
// waveguide rungs are ideal-only and reject them explicitly. Status: VERIFIED
// against the model (closed forms, cross-solver agreement), UNVALIDATED
// against physical measurement — no end corrections, radiation, clamped
// stiff ends or measured material data; real instruments deviate.
//
// Modes (n >= 1): same-type ends k_n = n*pi/L — Fixed-Fixed sin(k x),
// Free-Free cos(k x) plus the constant n = 0 mode; mixed ends
// k_n = (2n-1)*pi/(2L) — Fixed-Free sin, Free-Fixed cos. f_n = c*k_n/(2pi).
//
// Analytical rung: d'Alembert u(x,t) = [F(x-ct) + F(x+ct)]/2, F the
// extension of the initial shape — odd about a Fixed end, even about a Free
// end; period 2L for same-type ends, 4L for mixed. Exact for every shape.
//
// Modal rung: u = sum a_n phi_n(x) cos(w_n t), a_n = (2/L) int f*phi_n.
// For a pluck (triangle, apex h at x0), integrating by parts twice with
// phi'' = -k^2 phi gives the closed form used here:
//   int f*phi_n = (1/k^2)[hL/(x0(L-x0))*phi(x0) - h/x0*phi(0)
//                         - h/(L-x0)*phi(L)]
// hence |a_n| <= 4|h|/(k_n^2 x0 (L-x0)) and a stated truncation bound
// 4|h|/(x0(L-x0)) * sum_{n>N} 1/k_n^2, with the tail sums bounded by
// L^2/(pi^2 N) (same-type) and 2L^2/(pi^2 (2N-1)) (mixed).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"
#include "modes.h"

namespace mmathen {

enum class Boundary { Fixed, Free };

// D9 — optional loss: u_tt + 2*sigma0*u_t = c^2 u_xx + 2*sigma1*u_txx, the
// frequency-dependent loss form common in physical-modeling synthesis
// (Bilbao, Numerical Sound Synthesis, 2009, lossy string). Constants here are
// derived, not transcribed: each mode phi_n (k_n) obeys
//   q'' + 2*sigma_n*q' + w_n^2 q = 0,  sigma_n = sigma0 + sigma1*k_n^2,
// so amplitude decays as e^{-sigma_n t} and T60_n = 3*ln(10)/sigma_n
// (-60 dB = 10^-3 in amplitude). sigma0 [1/s] and sigma1 [m^2/s] must be
// finite and >= 0. No default values stand for real materials: parameters
// come from caller data (lossFromT60), per VALIDATION.md.
struct LossModel {
  double sigma0 = 0.0;
  double sigma1 = 0.0;
  [[nodiscard]] bool lossless() const noexcept {
    return sigma0 == 0.0 && sigma1 == 0.0;
  }
};

struct WaveSystem1D {
  DistanceMeters length;
  SpeedMetersPerSecond speed;
  Boundary left;
  Boundary right;
  LossModel loss;
  // D10 — bending stiffness kappa [m^2/s], kappa^2 = E*I/(rho*A): adds
  // -kappa^2 u_xxxx. Fixed-Fixed systems only, read as HINGED (simply
  // supported) ends: u = 0 and u_xx = 0, so mode shapes stay sin(n pi x/L)
  // and w_n^2 = c^2 k_n^2 + kappa^2 k_n^4, i.e. exactly D1's
  // f_n = n f0 sqrt(1 + B n^2) with f0 = c/2L and B = kappa^2 pi^2/(c^2 L^2).
  // Clamped ends (Fletcher's extra (1 + 2 sqrt(B)/pi + ...) factor) and
  // stiff free ends are deferred and rejected.
  double stiffness = 0.0;
};

inline constexpr std::size_t kMaxModalTerms = std::size_t{1} << 20;

[[nodiscard]] inline Result<WaveSystem1D, QuantityError> makeWaveSystem1D(
    DistanceMeters length, SpeedMetersPerSecond speed, Boundary left,
    Boundary right) {
  using R = Result<WaveSystem1D, QuantityError>;
  const double l = length.value();
  const double c = speed.value();
  if (!std::isfinite(l) || !(l > 0.0) || !std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(WaveSystem1D{length, speed, left, right, LossModel{}, 0.0});
}

// The same object with bending stiffness kappa attached (replacing any
// previous value). kappa finite and >= 0 (InvalidArgument otherwise);
// kappa > 0 on anything but Fixed-Fixed ends is DomainError (see above).
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> withStiffness(
    WaveSystem1D system, double kappa) {
  using R = Result<WaveSystem1D, QuantityError>;
  if (!std::isfinite(kappa) || kappa < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (kappa > 0.0 && (system.left != Boundary::Fixed ||
                      system.right != Boundary::Fixed))
    return R::failure(QuantityError::DomainError);
  system.stiffness = kappa;
  return R::success(system);
}

// kappa for a target inharmonicity B (D1's coefficient): kappa =
// sqrt(B)*c*L/pi. B finite and >= 0, else InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> stiffnessFromB(
    const WaveSystem1D& system, double b) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(b) || b < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double kappa = std::sqrt(b) * system.speed.value() *
                       system.length.value() / constants::pi;
  if (!std::isfinite(kappa)) return R::failure(QuantityError::RangeError);
  return R::success(kappa);
}

// B = kappa^2 pi^2 / (c^2 L^2) of a (hinged) stiff system; 0 when not stiff.
[[nodiscard]] inline double inharmonicityB(const WaveSystem1D& system) {
  const double k = system.stiffness * constants::pi /
                   (system.speed.value() * system.length.value());
  return k * k;
}

// The same object with a loss model attached (replacing any previous one).
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> withLoss(
    WaveSystem1D system, LossModel loss) {
  using R = Result<WaveSystem1D, QuantityError>;
  if (!std::isfinite(loss.sigma0) || loss.sigma0 < 0.0 ||
      !std::isfinite(loss.sigma1) || loss.sigma1 < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  system.loss = loss;
  return R::success(system);
}

inline constexpr double kT60DecayConstant = 3.0 * constants::ln10;

// Fit sigma0, sigma1 so that waves at f1 and f2 decay with T60 t1 and t2:
//   sigma(k) = sigma0 + sigma1*k^2 = 3 ln10 / T60,  k = 2*pi*f/c
// (k from the lossless dispersion relation: the f given are undamped mode
// frequencies — the damping shift sqrt(w^2 - sigma^2) is not inverted).
// Two equations, two unknowns, solved exactly. f1 == f2, non-positive or
// non-finite inputs are InvalidArgument; data the model cannot represent
// (decay rate falling with frequency, giving sigma1 < 0, or sigma0 < 0) is
// DomainError — reported, never clamped.
[[nodiscard]] inline Result<LossModel, QuantityError> lossFromT60(
    SpeedMetersPerSecond speed, FrequencyHz f1, DurationSeconds t1,
    FrequencyHz f2, DurationSeconds t2) {
  using R = Result<LossModel, QuantityError>;
  const double c = speed.value();
  const double fa = f1.value();
  const double fb = f2.value();
  const double ta = t1.value();
  const double tb = t2.value();
  // Plain per-value checks (not a loop over an initializer_list): MSVC
  // Release constant-propagates inlined calls and must see each guard
  // dominate its division, or /WX turns C4723 into an error.
  auto positive = [](double v) { return std::isfinite(v) && v > 0.0; };
  if (!positive(c) || !positive(fa) || !positive(fb))
    return R::failure(QuantityError::InvalidArgument);
  if (!positive(ta) || !positive(tb))
    return R::failure(QuantityError::InvalidArgument);
  if (fa == fb) return R::failure(QuantityError::InvalidArgument);
  const double ka = constants::twoPi * fa / c;
  const double kb = constants::twoPi * fb / c;
  const double sa = kT60DecayConstant / ta;
  const double sb = kT60DecayConstant / tb;
  const double sigma1 = (sb - sa) / (kb * kb - ka * ka);
  const double sigma0 = sa - sigma1 * ka * ka;
  if (!std::isfinite(sigma0) || !std::isfinite(sigma1))
    return R::failure(QuantityError::RangeError);
  if (sigma1 < 0.0 || sigma0 < 0.0)
    return R::failure(QuantityError::DomainError);
  return R::success(LossModel{sigma0, sigma1});
}

// Ideal string, fixed at both ends; c = sqrt(T/mu) from modes.h.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> idealString(
    DistanceMeters length, TensionNewtons tension,
    LinearDensityKgPerM density) {
  using R = Result<WaveSystem1D, QuantityError>;
  auto c = stringWaveSpeed(tension, density);
  if (!c.ok()) return R::failure(c.error());
  return makeWaveSystem1D(length, c.value(), Boundary::Fixed, Boundary::Fixed);
}

// Open-open pipe, pressure field: p = 0 at both open ends.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> openPipe(
    DistanceMeters length, SpeedMetersPerSecond speed) {
  return makeWaveSystem1D(length, speed, Boundary::Fixed, Boundary::Fixed);
}

// Stopped pipe, pressure field: closed (rigid) end at x = 0, open at x = L.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> stoppedPipe(
    DistanceMeters length, SpeedMetersPerSecond speed) {
  return makeWaveSystem1D(length, speed, Boundary::Free, Boundary::Fixed);
}

namespace detail {
inline bool mixedEnds(const WaveSystem1D& s) { return s.left != s.right; }

// Wavenumber of non-constant mode n >= 1.
inline double modeWavenumber(const WaveSystem1D& s, std::size_t n) {
  const double l = s.length.value();
  const double nn = static_cast<double>(n);
  return mixedEnds(s) ? (2.0 * nn - 1.0) * constants::pi / (2.0 * l)
                      : nn * constants::pi / l;
}

// Mode shape: sin where the left end is Fixed, cos where it is Free.
inline double modeShapeValue(const WaveSystem1D& s, std::size_t n, double x) {
  const double kx = modeWavenumber(s, n) * x;
  return s.left == Boundary::Fixed ? std::sin(kx) : std::cos(kx);
}
}  // namespace detail

namespace detail {
// Angular frequency of the undamped mode: c*k, or sqrt(c^2 k^2 + kappa^2 k^4)
// when stiff (the non-stiff form is kept literally so D8/D9 values are
// bit-identical).
inline double modeOmega(const WaveSystem1D& s, std::size_t n) {
  const double k = modeWavenumber(s, n);
  const double c = s.speed.value();
  if (s.stiffness == 0.0) return c * k;
  const double ck = c * k;
  const double kk = s.stiffness * k * k;
  return std::sqrt(ck * ck + kk * kk);
}

inline double modeDecay(const WaveSystem1D& s, std::size_t n) {
  const double k = modeWavenumber(s, n);
  return s.loss.sigma0 + s.loss.sigma1 * k * k;
}

// Solution of q'' + 2*sigma*q' + w^2 q = 0 with q(0) = 1, q'(0) = 0, in all
// three regimes. |q| <= 1 always (the oscillator energy is non-increasing),
// so modal truncation bounds carry over from the lossless case unchanged.
inline double dampedModeFactor(double omega, double sigma, double t) {
  if (sigma == 0.0) return std::cos(omega * t);
  if (sigma < omega) {
    const double wd = std::sqrt(omega * omega - sigma * sigma);
    return std::exp(-sigma * t) *
           (std::cos(wd * t) + sigma / wd * std::sin(wd * t));
  }
  if (sigma == omega) return std::exp(-sigma * t) * (1.0 + sigma * t);
  // Overdamped: written with decaying exponentials only (no cosh overflow).
  const double mu = std::sqrt(sigma * sigma - omega * omega);
  return 0.5 * ((1.0 + sigma / mu) * std::exp((mu - sigma) * t) +
                (1.0 - sigma / mu) * std::exp(-(mu + sigma) * t));
}
}  // namespace detail

// Frequency of mode n >= 1 (the Free-Free constant mode does not oscillate
// and is not a resonance: n = 0 is InvalidArgument for every system).
// Lossless: c*k_n/2pi. Lossy: the damped frequency sqrt(w_n^2 - sigma_n^2)/2pi;
// a critically or overdamped mode does not oscillate: DomainError.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> modeFrequency(
    const WaveSystem1D& system, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  const auto nn = static_cast<std::size_t>(n);
  const double omega = detail::modeOmega(system, nn);
  if (!std::isfinite(omega)) return R::failure(QuantityError::RangeError);
  if (system.loss.lossless()) return R::success(FrequencyHz{omega / constants::twoPi});
  const double sigma = detail::modeDecay(system, nn);
  if (!(sigma < omega)) return R::failure(QuantityError::DomainError);
  return R::success(FrequencyHz{
      std::sqrt(omega * omega - sigma * sigma) / constants::twoPi});
}

// Amplitude decay rate sigma_n of mode n >= 1 (0 for a lossless system).
[[nodiscard]] inline Result<double, QuantityError> modeDecayRate(
    const WaveSystem1D& system, int n) {
  using R = Result<double, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  const double s = detail::modeDecay(system, static_cast<std::size_t>(n));
  if (!std::isfinite(s)) return R::failure(QuantityError::RangeError);
  return R::success(s);
}

// T60 of mode n >= 1: 3 ln10 / sigma_n. A lossless mode never decays:
// DomainError (infinity is not reported as a duration).
[[nodiscard]] inline Result<DurationSeconds, QuantityError> modeT60(
    const WaveSystem1D& system, int n) {
  using R = Result<DurationSeconds, QuantityError>;
  auto s = modeDecayRate(system, n);
  if (!s.ok()) return R::failure(s.error());
  if (!(s.value() > 0.0)) return R::failure(QuantityError::DomainError);
  return R::success(DurationSeconds{kT60DecayConstant / s.value()});
}

// Initial displacement/pressure shape with zero initial velocity. Closed
// set of kinds so every solver has an exact reference; the system travels
// with the shape so solvers cannot pair a shape with the wrong object.
class InitialShape {
 public:
  enum class Kind { Pluck, Mode };

  // Triangle through (0,0), (x0,h), (L,0); 0 < x0 < L, h finite.
  [[nodiscard]] static Result<InitialShape, QuantityError> pluck(
      const WaveSystem1D& system, DistanceMeters position, double height) {
    using R = Result<InitialShape, QuantityError>;
    const double x0 = position.value();
    const double l = system.length.value();
    if (!std::isfinite(x0) || !(x0 > 0.0) || !(x0 < l) ||
        !std::isfinite(height))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(InitialShape{system, Kind::Pluck, x0, height, 0});
  }

  // amplitude * phi_n(x), n >= 1.
  [[nodiscard]] static Result<InitialShape, QuantityError> mode(
      const WaveSystem1D& system, int n, double amplitude) {
    using R = Result<InitialShape, QuantityError>;
    if (n < 1 || !std::isfinite(amplitude))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(InitialShape{system, Kind::Mode, 0.0, amplitude,
                                   static_cast<std::size_t>(n)});
  }

  [[nodiscard]] const WaveSystem1D& system() const noexcept { return sys_; }
  [[nodiscard]] Kind kind() const noexcept { return kind_; }

  // Shape value on [0, L] (callers keep x in range). A Fixed boundary is
  // exactly 0, not sin(n*pi) roundoff.
  [[nodiscard]] double valueAt(double x) const {
    const double l = sys_.length.value();
    if ((x == 0.0 && sys_.left == Boundary::Fixed) ||
        (x == l && sys_.right == Boundary::Fixed))
      return 0.0;
    if (kind_ == Kind::Mode)
      return amp_ * detail::modeShapeValue(sys_, n_, x);
    return x <= x0_ ? amp_ * x / x0_ : amp_ * (l - x) / (l - x0_);
  }

  // Modal coefficient a_n for n >= 1; constantTerm() for the Free-Free
  // n = 0 mode (zero for every other system).
  [[nodiscard]] double modalCoefficient(std::size_t n) const {
    if (kind_ == Kind::Mode) return n == n_ ? amp_ : 0.0;
    const double l = sys_.length.value();
    const double k = detail::modeWavenumber(sys_, n);
    const double h = amp_;
    const double integral =
        (h * l / (x0_ * (l - x0_)) * detail::modeShapeValue(sys_, n, x0_) -
         h / x0_ * detail::modeShapeValue(sys_, n, 0.0) -
         h / (l - x0_) * detail::modeShapeValue(sys_, n, l)) /
        (k * k);
    return 2.0 / l * integral;
  }
  [[nodiscard]] double constantTerm() const {
    if (sys_.left != Boundary::Free || sys_.right != Boundary::Free) return 0.0;
    return kind_ == Kind::Pluck ? amp_ / 2.0 : 0.0;  // mean of the triangle
  }

  // Sup-norm bound on the modal tail beyond N terms.
  [[nodiscard]] double truncationBound(std::size_t terms) const {
    if (kind_ == Kind::Mode) return terms >= n_ ? 0.0 : std::fabs(amp_);
    const double l = sys_.length.value();
    const double nn = static_cast<double>(terms);
    const double tail =
        detail::mixedEnds(sys_)
            ? 2.0 * l * l / (constants::pi * constants::pi * (2.0 * nn - 1.0))
            : l * l / (constants::pi * constants::pi * nn);
    return 4.0 * std::fabs(amp_) / (x0_ * (l - x0_)) * tail;
  }

  [[nodiscard]] std::string formula() const {
    return kind_ == Kind::Pluck ? "pluck" : "mode" + std::to_string(n_);
  }

 private:
  InitialShape(WaveSystem1D sys, Kind kind, double x0, double amp,
               std::size_t n)
      : sys_(sys), kind_(kind), x0_(x0), amp_(amp), n_(n) {}
  WaveSystem1D sys_;
  Kind kind_;
  double x0_;
  double amp_;
  std::size_t n_;
};

namespace detail {
// Extension of the initial shape to the real line (odd about Fixed ends,
// even about Free ends).
inline double extendedShape(const InitialShape& shape, double s) {
  const WaveSystem1D& sys = shape.system();
  const double l = sys.length.value();
  const double period = mixedEnds(sys) ? 4.0 * l : 2.0 * l;
  s = std::fmod(s, period);
  if (s < 0.0) s += period;
  double sign = 1.0;
  if (s > period / 2.0) {  // f(s) = f(s - P) = leftSign * f(P - s)
    s = period - s;
    sign = sys.left == Boundary::Fixed ? -1.0 : 1.0;
  }
  if (s > l) {  // reflect about L
    s = 2.0 * l - s;
    sign *= sys.right == Boundary::Fixed ? -1.0 : 1.0;
  }
  if (s < 0.0) s = 0.0;  // roundoff guard at the fold points
  if (s > l) s = l;
  return sign * shape.valueAt(s);
}
}  // namespace detail

// Analytical rung: d'Alembert with boundary reflections. x in [0, L],
// t finite; otherwise InvalidArgument. Lossy systems are not traveling-wave
// solutions (loss makes the equation dispersive): DomainError — explicit
// solver/object incompatibility, not an approximation.
[[nodiscard]] inline Result<double, QuantityError> analyticField(
    const InitialShape& shape, DistanceMeters x, TimePointSeconds t) {
  using R = Result<double, QuantityError>;
  if (!shape.system().loss.lossless() || shape.system().stiffness != 0.0)
    return R::failure(QuantityError::DomainError);
  const double xx = x.value();
  const double tt = t.value();
  const double l = shape.system().length.value();
  if (!std::isfinite(xx) || xx < 0.0 || xx > l || !std::isfinite(tt))
    return R::failure(QuantityError::InvalidArgument);
  const double ct = shape.system().speed.value() * tt;
  return R::success(0.5 * (detail::extendedShape(shape, xx - ct) +
                           detail::extendedShape(shape, xx + ct)));
}

// Modal rung: N non-constant modes (plus the Free-Free constant term).
class ModalSolution {
 public:
  [[nodiscard]] static Result<ModalSolution, QuantityError> create(
      const InitialShape& shape, std::size_t terms) {
    using R = Result<ModalSolution, QuantityError>;
    if (terms < 1 || terms > kMaxModalTerms)
      return R::failure(QuantityError::InvalidArgument);
    std::vector<Term> ts;
    ts.reserve(terms);
    for (std::size_t n = 1; n <= terms; ++n) {
      const double k = detail::modeWavenumber(shape.system(), n);
      ts.push_back(Term{n, k, detail::modeOmega(shape.system(), n),
                        detail::modeDecay(shape.system(), n),
                        shape.modalCoefficient(n)});
    }
    return R::success(ModalSolution{shape, std::move(ts)});
  }

  [[nodiscard]] Result<double, QuantityError> at(DistanceMeters x,
                                                 TimePointSeconds t) const {
    using R = Result<double, QuantityError>;
    const double xx = x.value();
    const double tt = t.value();
    if (!std::isfinite(xx) || xx < 0.0 || xx > shape_.system().length.value() ||
        !std::isfinite(tt))
      return R::failure(QuantityError::InvalidArgument);
    const bool sinShape = shape_.system().left == Boundary::Fixed;
    // The Free-Free constant mode (k = 0) sees only sigma0:
    // q'' + 2*sigma0*q' = 0 with q'(0) = 0 keeps it constant.
    double sum = shape_.constantTerm();
    for (const auto& term : terms_) {
      const double kx = term.k * xx;
      sum += term.a * (sinShape ? std::sin(kx) : std::cos(kx)) *
             detail::dampedModeFactor(term.omega, term.sigma, tt);
    }
    return R::success(sum);
  }

  [[nodiscard]] double truncationBound() const {
    return shape_.truncationBound(terms_.size());
  }
  [[nodiscard]] std::size_t terms() const noexcept { return terms_.size(); }

 private:
  struct Term {
    std::size_t n;
    double k;
    double omega;
    double sigma;
    double a;
  };
  ModalSolution(InitialShape shape, std::vector<Term> terms)
      : shape_(std::move(shape)), terms_(std::move(terms)) {}
  InitialShape shape_;
  std::vector<Term> terms_;
};

}  // namespace mmathen
