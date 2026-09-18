#pragma once

// D17 — coupling objects: an ideal string attached to a bridge oscillator.
//
// String: u_tt = c^2 u_xx on [0, L], tension T, linear density mu = T/c^2,
// fixed at x = 0. At x = L it is attached to a mass-spring-damper "bridge"
// (mass m, stiffness k, damping r) whose displacement IS the string end:
//   m y'' = -T u_x(L) - k y - r y',   y = u(L).
// Energy flows from the string into the bridge and (with r > 0) out of the
// system — the smallest honest instance of "objects coupled together".
//
// Continuous modes (lossless, r = 0): X = sin(beta x), w = c beta, and the
// bridge condition gives the transcendental equation
//   T beta cos(beta L) + (k - m w^2) sin(beta L) = 0,
// solved by sign-change scan and bisection. Limits: k -> infinity recovers
// the fixed-fixed series n c/2L; m = k = 0 recovers fixed-free
// (2n-1) c/4L (D8's mixed-end series).
//
// FDTD: interior nodes as D8 (Courant lam). The end node M is the bridge
// with half a string cell of mass attached (second-order boundary):
//   (m + mu h/2)(y^{n+1} - 2y^n + y^{n-1})/dt^2
//     = -T (y^n - u_{M-1}^n)/h - k y^n - r (y^{n+1} - y^{n-1})/(2 dt).
// Stability, derived (sufficient): leapfrog on u'' = -A u is stable when
// dt^2 rho(A) < 4 (strict); Gershgorin bounds rho(A) by the largest row
// sum — interior rows 4c^2/h^2 (lam <= 1), bridge row (2T/h + k)/m_eff. So
// creation requires lam <= 1 and dt^2 (2T/h + k)/m_eff < 4 (DomainError
// otherwise); damping r >= 0 only adds dissipation. For m = k = 0 the bridge
// row equals an interior row (4c^2/h^2), consistently.
//
// Verified, unvalidated: a lumped rigid bridge, one polarization, no
// soundboard, no radiation.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"
#include "wave1d.h"

namespace mmathen {

struct BridgeOscillator {
  double mass = 0.0;       // kg, >= 0
  double stiffness = 0.0;  // N/m, >= 0
  double damping = 0.0;    // N s/m, >= 0
};

struct CoupledString {
  WaveSystem1D string;  // Fixed at x = 0; its right boundary is replaced
  TensionNewtons tension;
  BridgeOscillator bridge;
};

// String (non-stiff, lossless, left end Fixed) + bridge; InvalidArgument for
// negative/non-finite parameters, a stiff or lossy string, or a Free left end.
[[nodiscard]] inline Result<CoupledString, QuantityError> makeCoupledString(
    const WaveSystem1D& string, TensionNewtons tension,
    BridgeOscillator bridge) {
  using R = Result<CoupledString, QuantityError>;
  const double t = tension.value();
  if (!std::isfinite(t) || !(t > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  for (const double v : {bridge.mass, bridge.stiffness, bridge.damping})
    if (!std::isfinite(v) || v < 0.0)
      return R::failure(QuantityError::InvalidArgument);
  if (string.stiffness != 0.0 || !string.loss.lossless() ||
      string.left != Boundary::Fixed)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(CoupledString{string, tension, bridge});
}

// First `count` coupled mode frequencies (lossless: bridge damping ignored),
// from the transcendental equation; scan step pi/(64L) in beta.
[[nodiscard]] inline Result<std::vector<FrequencyHz>, QuantityError>
coupledModeFrequencies(const CoupledString& s, int count) {
  using R = Result<std::vector<FrequencyHz>, QuantityError>;
  if (count < 1 || count > 4096)
    return R::failure(QuantityError::InvalidArgument);
  const double l = s.string.length.value();
  const double c = s.string.speed.value();
  const double t = s.tension.value();
  const double m = s.bridge.mass;
  const double k = s.bridge.stiffness;
  auto f = [&](double beta) {
    const double w = c * beta;
    // Normalised by (T beta + |k - m w^2|) so large k or m keep the sign
    // change well conditioned.
    const double a = t * beta;
    const double b = k - m * w * w;
    return (a * std::cos(beta * l) + b * std::sin(beta * l)) /
           (a + std::fabs(b));
  };
  const double step = constants::pi / (64.0 * l);
  std::vector<FrequencyHz> out;
  double b0 = 1e-6 * step;
  double f0 = f(b0);
  const double limit = (static_cast<double>(count) + 2.0) * constants::pi / l;
  while (static_cast<int>(out.size()) < count && b0 < limit) {
    const double b1 = b0 + step;
    const double f1 = f(b1);
    if ((f0 < 0.0) != (f1 < 0.0)) {
      double lo = b0, hi = b1, flo = f0;
      for (int it = 0; it < 200; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (!(mid > lo) || !(mid < hi)) break;
        const double fm = f(mid);
        if ((fm < 0.0) == (flo < 0.0)) {
          lo = mid;
          flo = fm;
        } else {
          hi = mid;
        }
      }
      out.push_back(FrequencyHz{c * 0.5 * (lo + hi) / constants::twoPi});
    }
    b0 = b1;
    f0 = f1;
  }
  if (static_cast<int>(out.size()) < count)
    return R::failure(QuantityError::RangeError);
  return R::success(std::move(out));
}

class CoupledStringFdtd {
 public:
  [[nodiscard]] static Result<CoupledStringFdtd, QuantityError> create(
      const CoupledString& system, const InitialShape& shape,
      std::size_t intervals, double courant) {
    using R = Result<CoupledStringFdtd, QuantityError>;
    if (intervals < 2 || intervals > (std::size_t{1} << 22) ||
        !std::isfinite(courant) || !(courant > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (courant > 1.0) return R::failure(QuantityError::DomainError);
    const double l = system.string.length.value();
    const double c = system.string.speed.value();
    const double t = system.tension.value();
    const double h = l / static_cast<double>(intervals);
    const double dt = courant * h / c;
    const double mu = t / (c * c);
    const double meff = system.bridge.mass + 0.5 * mu * h;
    if (!(dt * dt * (2.0 * t / h + system.bridge.stiffness) / meff < 4.0))
      return R::failure(QuantityError::DomainError);
    return R::success(CoupledStringFdtd{system, shape, intervals, courant});
  }

  void step() {
    const std::size_t m = u_.size() - 1;
    const double lam2 = courant_ * courant_;
    for (std::size_t j = 1; j < m; ++j) {
      const double lap = u_[j + 1] - 2.0 * u_[j] + u_[j - 1];
      next_[j] = steps_ == 0 ? u_[j] + 0.5 * lam2 * lap
                             : 2.0 * u_[j] - prev_[j] + lam2 * lap;
    }
    next_[0] = 0.0;
    // Bridge node.
    const double force = -tension_ * (u_[m] - u_[m - 1]) / h_ -
                         bridge_.stiffness * u_[m];
    if (steps_ == 0) {
      // Zero initial velocity: y^1 = y^0 + dt^2/2 * a^0 (damping term is
      // zero at rest velocity).
      next_[m] = u_[m] + 0.5 * dt_ * dt_ * force / meff_;
    } else {
      const double g = bridge_.damping * dt_ / (2.0 * meff_);
      next_[m] = (2.0 * u_[m] - (1.0 - g) * prev_[m] +
                  dt_ * dt_ * force / meff_) /
                 (1.0 + g);
    }
    std::swap(prev_, u_);
    std::swap(u_, next_);
    ++steps_;
  }

  [[nodiscard]] const std::vector<double>& field() const noexcept { return u_; }
  [[nodiscard]] const std::vector<double>& previousField() const noexcept {
    return prev_;
  }
  [[nodiscard]] double bridgeDisplacement() const noexcept { return u_.back(); }
  [[nodiscard]] DurationSeconds timeStep() const noexcept {
    return DurationSeconds{dt_};
  }
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_; }
  // Discrete energy (lossless conservation check; with damping it must not
  // increase): string kinetic on interior nodes (mu h per node), bridge
  // kinetic with m_eff, string potential T/(2h) D+u^{n+1} D+u^n, spring
  // k/2 y^{n+1} y^n. Evaluated between time levels n and n+1.
  [[nodiscard]] double energy() const {
    const std::size_t m = u_.size() - 1;
    const double mu = tension_ / (c_ * c_);
    double e = 0.0;
    for (std::size_t j = 1; j < m; ++j) {
      const double v = (u_[j] - prev_[j]) / dt_;
      e += 0.5 * mu * h_ * v * v;
    }
    const double vb = (u_[m] - prev_[m]) / dt_;
    e += 0.5 * meff_ * vb * vb;
    for (std::size_t j = 0; j < m; ++j)
      e += 0.5 * tension_ / h_ * (u_[j + 1] - u_[j]) * (prev_[j + 1] - prev_[j]);
    e += 0.5 * bridge_.stiffness * u_[m] * prev_[m];
    return e;
  }

 private:
  CoupledStringFdtd(const CoupledString& system, const InitialShape& shape,
                    std::size_t intervals, double courant)
      : courant_(courant), bridge_(system.bridge) {
    const double l = system.string.length.value();
    c_ = system.string.speed.value();
    tension_ = system.tension.value();
    h_ = l / static_cast<double>(intervals);
    dt_ = courant * h_ / c_;
    meff_ = bridge_.mass + 0.5 * tension_ / (c_ * c_) * h_;
    u_.assign(intervals + 1, 0.0);
    for (std::size_t j = 1; j <= intervals; ++j)
      u_[j] = shape.valueAt(j == intervals ? l : static_cast<double>(j) * h_);
    prev_.assign(intervals + 1, 0.0);
    next_.assign(intervals + 1, 0.0);
  }

  double courant_;
  BridgeOscillator bridge_;
  double c_ = 0.0, tension_ = 0.0, h_ = 0.0, dt_ = 0.0, meff_ = 0.0;
  std::vector<double> prev_, u_, next_;
  std::uint64_t steps_ = 0;
};

}  // namespace mmathen
