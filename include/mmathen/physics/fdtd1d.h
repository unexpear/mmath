#pragma once

// D8 — FDTD rung for the 1-D wave system of wave1d.h (explicit leapfrog).
//
//   u_j^{n+1} = 2(1 - lam^2) u_j^n + lam^2 (u_{j+1}^n + u_{j-1}^n) - u_j^{n-1}
//   lam = c*dt/dx (Courant number), dx = L/M, nodes j = 0..M.
// First step from zero initial velocity (second-order Taylor start):
//   u_j^1 = u_j^0 + lam^2/2 (u_{j+1}^0 - 2u_j^0 + u_{j-1}^0)
// Boundaries: Fixed nodes stay exactly 0; Free nodes use the mirror ghost
// u_{-1} = u_1 (second-order Neumann).
//
// Stability (CFL/von Neumann): lam <= 1. lam > 1 grows without bound, so it
// is REJECTED as DomainError, never run. Accuracy: second order in dx at
// fixed lam < 1 (numerical dispersion); at lam = 1 the scheme reproduces
// d'Alembert EXACTLY at grid points for any initial data — the "magic time
// step" (both are sampled F(x-ct) + G(x+ct); verified in the tests).
// Conserved discrete energy for lam <= 1 (tested, Fixed-Fixed):
//   E = 1/2 sum ((u^{n+1}-u^n)/dt)^2 + c^2/(2dx^2) sum D u^{n+1} * D u^n
//
// D9 — lossy systems (LossModel in wave1d.h) use the centered, implicit
// scheme (r = sigma1*dt/dx^2, D = second difference with the same ghosts):
//   (1 + sigma0*dt) u^{n+1} - r D u^{n+1}
//     = 2u^n + lam^2 D u^n - (1 - sigma0*dt) u^{n-1} - r D u^{n-1}
// one tridiagonal (Thomas) solve per step, O(M), diagonally dominant for
// every r >= 0. Second order in dt and dx. Stability, derived here by von
// Neumann with D -> -p, p in [0,4]: the amplification polynomial
//   (1+s+rp) z^2 - (2 - lam^2 p) z + (1-s-rp),  s = sigma0*dt
// (the u^{n-1} terms -(1-s) - r D -> -(1-s) + rp; D9 first wrote +rp in the
// constant term — corrected in D12, when FDTD decay was measured against
// this polynomial and disagreed). Jury: |1-s-rp| <= 1+s+rp always, and
// |b| <= a + c = 2 iff lam^2 p <= 4, i.e. lam <= 1 — the lossless
// condition, unchanged by any sigma0, sigma1 >= 0. (The explicit
// backward-difference variant would instead need lam^2 + 4*sigma1*dt/dx^2
// <= 1 and is first order in time for the sigma1 term; not used.) Zero
// initial velocity gives u_tt(0) = c^2 u_xx, so the Taylor start is the
// lossless one. Discrete energy (below) is non-increasing (tested).
//
// D10 — hinged stiffness (kappa > 0, Fixed-Fixed only; wave1d.h) adds the
// explicit term -mu^2 D^2 u^n, mu = kappa*dt/dx^2, to either scheme above,
// with odd ghosts u_{-1} = -u_1, u_{M+1} = -u_{M-1} (u = 0 and u_xx = 0 at
// the ends, so D u = 0 at the end nodes). The start step adds -mu^2/2 D^2 u^0
// (u_tt(0) = c^2 u_xx - kappa^2 u_xxxx). Stability, derived by von Neumann
// with D -> -p: lossless, z + 1/z = 2 - lam^2 p - mu^2 p^2, bounded iff
// lam^2 p + mu^2 p^2 <= 4 on p in [0,4], i.e.
//   lam^2 + 4 mu^2 <= 1
// (equivalently dx^2 >= (c^2 dt^2 + sqrt(c^4 dt^4 + 16 kappa^2 dt^2))/2, the
// form given by Bilbao). With loss a + c = 2 still, so the Jury condition
// is again exactly lam^2 p + mu^2 p^2 <= 4 (D10 first stated "<= 4 + 2rp",
// from the D9 sign error; corrected in D12). It is
// enforced at creation (DomainError), never run past. Numerical dispersion
// flattens high stiff partials at coarse grids; second-order convergence is
// verified, per-partial tuning error at audio rates is not claimed.
// Discrete energy adds kappa^2/(2 dx^4) sum D u^{n+1} * D u^n.
//
// D13 — clamped and free stiff ends use an extended array with two ghost
// nodes per side, filled each step from centered second-order boundary
// conditions (index -1, -2 left; M+1, M+2 right, mirrored):
//   Hinged : u_0 = 0, u_{-1} = -u_1, u_{-2} = -u_2
//   Clamped: u_0 = 0, u_{-1} = +u_1 (u_x = 0), u_{-2} = +u_2 (unused)
//   Free   : u_xx = 0            -> u_{-1} = 2 u_0 - u_1
//            k^2 u_xxx = c^2 u_x -> u_{-2} = u_2 - 2u_1 + 2u_{-1}
//                                          - q (u_1 - u_{-1}),  q = c^2 dx^2/k^2
//            right end:             u_{M+2} = 2u_{M+1} - 2u_{M-1} + u_{M-2}
//                                          + q (u_{M+1} - u_{M-1})
// Tension and bending use the ghost-filled D and D^2 at every unknown node
// (a free end node is an unknown). With sigma1 loss the implicit D u^{n+1}
// row at a free node is empty (D u = 0 there by the boundary condition), so
// that row is (1+s) u_0. These rows are NOT derived from an energy
// identity; their stability under lam^2 + 4 mu^2 <= 1 is established by
// long runs in the tests, and their accuracy by FFT-measured partials
// converging to the determinant roots (wave1d.h).
//
// Stateful processor under the reserved contract (D5 precedent): all
// buffers allocated at creation, step() allocation-free and O(M), reset()
// restores the initial state bit-identically. Samples of the field are
// frame-indexed (dt is fixed by lam, dx, c), not continuous-time Signals.

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

inline constexpr std::size_t kMaxGridIntervals = std::size_t{1} << 22;

class Fdtd1D {
 public:
  // intervals M in [2, kMaxGridIntervals]; courant finite, in (0, 1].
  [[nodiscard]] static Result<Fdtd1D, QuantityError> create(
      InitialShape shape, std::size_t intervals, double courant) {
    using R = Result<Fdtd1D, QuantityError>;
    if (intervals < 2 || intervals > kMaxGridIntervals)
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(courant) || !(courant > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (courant > 1.0) return R::failure(QuantityError::DomainError);
    const WaveSystem1D& sys = shape.system();
    if (sys.stiffness != 0.0) {
      const double dx = sys.length.value() / static_cast<double>(intervals);
      const double dt = courant * dx / sys.speed.value();
      const double mu = sys.stiffness * dt / (dx * dx);
      if (courant * courant + 4.0 * mu * mu > 1.0)
        return R::failure(QuantityError::DomainError);
    }
    return R::success(Fdtd1D{std::move(shape), intervals, courant});
  }

  void step() {
    const std::size_t m = cur_.size() - 1;
    const double lam2 = courant_ * courant_;
    const WaveSystem1D& sys = shape_.system();
    // neighbors(j) = u_{j+1} + u_{j-1} with the Free-end mirror ghost.
    auto neighbors = [&](const std::vector<double>& u, std::size_t j) {
      if (j == 0) return 2.0 * u[1];
      if (j == m) return 2.0 * u[m - 1];
      return u[j + 1] + u[j - 1];
    };
    const bool stiff = sys.stiffness != 0.0;
    if (stiff && (sys.stiffLeft != StiffEnd::Hinged ||
                  sys.stiffRight != StiffEnd::Hinged)) {
      stepStiffEnds();
      return;
    }
    if (stiff) computeBiharmonic();
    if (!sys.loss.lossless() && steps_ > 0) {
      stepLossy(neighbors);
      return;
    }
    for (std::size_t j = 0; j <= m; ++j) {
      const double nb = neighbors(cur_, j);
      next_[j] = steps_ == 0
                     ? cur_[j] + 0.5 * lam2 * (nb - 2.0 * cur_[j])
                     : 2.0 * (1.0 - lam2) * cur_[j] + lam2 * nb - prev_[j];
    }
    if (stiff) {
      const double mu2 = stiffMu() * stiffMu();
      const double w = steps_ == 0 ? 0.5 * mu2 : mu2;
      for (std::size_t j = 1; j < m; ++j) next_[j] -= w * biharm_[j];
    }
    if (sys.left == Boundary::Fixed) next_[0] = 0.0;
    if (sys.right == Boundary::Fixed) next_[m] = 0.0;
    std::swap(prev_, cur_);
    std::swap(cur_, next_);
    ++steps_;
  }

  void reset() {
    cur_ = initial_;
    std::fill(prev_.begin(), prev_.end(), 0.0);
    std::fill(next_.begin(), next_.end(), 0.0);
    steps_ = 0;
  }

  [[nodiscard]] Result<double, QuantityError> valueAt(std::size_t node) const {
    using R = Result<double, QuantityError>;
    if (node >= cur_.size()) return R::failure(QuantityError::InvalidArgument);
    return R::success(cur_[node]);
  }
  [[nodiscard]] const std::vector<double>& field() const noexcept {
    return cur_;
  }
  // Field one step earlier (valid after the first step; the energy check
  // needs both time levels).
  [[nodiscard]] const std::vector<double>& previousField() const noexcept {
    return prev_;
  }
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_; }
  [[nodiscard]] std::size_t intervals() const noexcept {
    return cur_.size() - 1;
  }
  [[nodiscard]] double courant() const noexcept { return courant_; }
  [[nodiscard]] DistanceMeters gridSpacing() const noexcept {
    return DistanceMeters{dx_};
  }
  [[nodiscard]] DurationSeconds timeStep() const noexcept {
    return DurationSeconds{dt_};
  }
  // t_n = n*dt from the integer step count (never accumulated).
  [[nodiscard]] TimePointSeconds time() const noexcept {
    return TimePointSeconds{static_cast<double>(steps_) * dt_};
  }
  [[nodiscard]] const InitialShape& shape() const noexcept { return shape_; }

 private:
  // Implicit lossy step (see header comment). Unknowns are the non-Fixed
  // nodes j0..j1; Fixed end nodes stay exactly 0.
  template <typename Neighbors>
  void stepLossy(const Neighbors& neighbors) {
    const std::size_t m = cur_.size() - 1;
    const WaveSystem1D& sys = shape_.system();
    const double lam2 = courant_ * courant_;
    const double s = sys.loss.sigma0 * dt_;
    const double r = sys.loss.sigma1 * dt_ / (dx_ * dx_);
    const std::size_t j0 = sys.left == Boundary::Fixed ? 1 : 0;
    const std::size_t j1 = sys.right == Boundary::Fixed ? m - 1 : m;
    for (std::size_t j = j0; j <= j1; ++j) {
      const double lapCur = neighbors(cur_, j) - 2.0 * cur_[j];
      const double lapPrev = neighbors(prev_, j) - 2.0 * prev_[j];
      next_[j] = 2.0 * cur_[j] + lam2 * lapCur - (1.0 - s) * prev_[j] -
                 r * lapPrev;
    }
    if (sys.stiffness != 0.0) {  // biharm_ filled by step()
      const double mu2 = stiffMu() * stiffMu();
      for (std::size_t j = 1; j < m; ++j) next_[j] -= mu2 * biharm_[j];
    }
    // Thomas algorithm on rows j0..j1: sub*u[j-1] + diag*u[j] + sup*u[j+1].
    const double diag = 1.0 + s + 2.0 * r;
    auto sub = [&](std::size_t j) { return j == m ? -2.0 * r : -r; };
    auto sup = [&](std::size_t j) { return j == 0 ? -2.0 * r : -r; };
    workC_[j0] = j0 == j1 ? 0.0 : sup(j0) / diag;
    workD_[j0] = next_[j0] / diag;
    for (std::size_t j = j0 + 1; j <= j1; ++j) {
      const double denom = diag - sub(j) * workC_[j - 1];
      workC_[j] = j == j1 ? 0.0 : sup(j) / denom;
      workD_[j] = (next_[j] - sub(j) * workD_[j - 1]) / denom;
    }
    next_[j1] = workD_[j1];
    for (std::size_t j = j1; j > j0; --j)
      next_[j - 1] = workD_[j - 1] - workC_[j - 1] * next_[j];
    if (sys.left == Boundary::Fixed) next_[0] = 0.0;
    if (sys.right == Boundary::Fixed) next_[m] = 0.0;
    std::swap(prev_, cur_);
    std::swap(cur_, next_);
    ++steps_;
  }

  // Ghost-filled copy of u into ext (ext[j + 2] = u[j], j = -2 .. M+2).
  void fillGhosts(const std::vector<double>& u, std::vector<double>& ext) const {
    const std::size_t m = u.size() - 1;
    const WaveSystem1D& sys = shape_.system();
    const double c = sys.speed.value();
    const double q = c * c * dx_ * dx_ / (sys.stiffness * sys.stiffness);
    for (std::size_t j = 0; j <= m; ++j) ext[j + 2] = u[j];
    switch (sys.stiffLeft) {
      case StiffEnd::Hinged:
        ext[2] = 0.0;
        ext[1] = -u[1];
        ext[0] = -u[2];
        break;
      case StiffEnd::Clamped:
        ext[2] = 0.0;
        ext[1] = u[1];
        ext[0] = u[2];
        break;
      case StiffEnd::Free: {
        const double g1 = 2.0 * u[0] - u[1];
        ext[1] = g1;
        ext[0] = u[2] - 2.0 * u[1] + 2.0 * g1 - q * (u[1] - g1);
        break;
      }
    }
    switch (sys.stiffRight) {
      case StiffEnd::Hinged:
        ext[m + 2] = 0.0;
        ext[m + 3] = -u[m - 1];
        ext[m + 4] = -u[m - 2];
        break;
      case StiffEnd::Clamped:
        ext[m + 2] = 0.0;
        ext[m + 3] = u[m - 1];
        ext[m + 4] = u[m - 2];
        break;
      case StiffEnd::Free: {
        const double g1 = 2.0 * u[m] - u[m - 1];
        ext[m + 3] = g1;
        ext[m + 4] = 2.0 * g1 - 2.0 * u[m - 1] + u[m - 2] + q * (g1 - u[m - 1]);
        break;
      }
    }
  }

  // Lossless or lossy step for clamped/free stiff ends (header comment).
  void stepStiffEnds() {
    const std::size_t m = cur_.size() - 1;
    const WaveSystem1D& sys = shape_.system();
    const double lam2 = courant_ * courant_;
    const double mu2 = stiffMu() * stiffMu();
    const std::size_t j0 = sys.stiffLeft == StiffEnd::Free ? 0 : 1;
    const std::size_t j1 = sys.stiffRight == StiffEnd::Free ? m : m - 1;
    fillGhosts(cur_, extCur_);
    // lap at ext index i (node i - 2), for i = 1 .. M+3.
    auto lapExt = [&](const std::vector<double>& e, std::size_t i) {
      return e[i + 1] - 2.0 * e[i] + e[i - 1];
    };
    for (std::size_t j = j0; j <= j1; ++j) {
      const std::size_t i = j + 2;
      const double lap = lapExt(extCur_, i);
      const double bih =
          lapExt(extCur_, i + 1) - 2.0 * lap + lapExt(extCur_, i - 1);
      lap_[j] = lap;
      biharm_[j] = bih;
    }
    const bool lossy = !sys.loss.lossless() && steps_ > 0;
    if (!lossy) {
      for (std::size_t j = j0; j <= j1; ++j) {
        next_[j] = steps_ == 0
                       ? cur_[j] + 0.5 * (lam2 * lap_[j] - mu2 * biharm_[j])
                       : 2.0 * cur_[j] - prev_[j] + lam2 * lap_[j] -
                             mu2 * biharm_[j];
      }
    } else {
      const double sl = sys.loss.sigma0 * dt_;
      const double r = sys.loss.sigma1 * dt_ / (dx_ * dx_);
      fillGhosts(prev_, extPrev_);
      for (std::size_t j = j0; j <= j1; ++j) {
        const double lapPrev = lapExt(extPrev_, j + 2);
        next_[j] = 2.0 * cur_[j] + lam2 * lap_[j] - mu2 * biharm_[j] -
                   (1.0 - sl) * prev_[j] - r * lapPrev;
      }
      // Rows: interior (1+s+2r) with -r neighbours; a free end node's D u
      // is zero by its boundary condition, so its row is just (1+s).
      auto isFreeNode = [&](std::size_t j) {
        return (j == 0 && sys.stiffLeft == StiffEnd::Free) ||
               (j == m && sys.stiffRight == StiffEnd::Free);
      };
      auto diag = [&](std::size_t j) {
        return isFreeNode(j) ? 1.0 + sl : 1.0 + sl + 2.0 * r;
      };
      auto sub = [&](std::size_t j) { return isFreeNode(j) ? 0.0 : -r; };
      auto sup = [&](std::size_t j) { return isFreeNode(j) ? 0.0 : -r; };
      workC_[j0] = j0 == j1 ? 0.0 : sup(j0) / diag(j0);
      workD_[j0] = next_[j0] / diag(j0);
      for (std::size_t j = j0 + 1; j <= j1; ++j) {
        const double denom = diag(j) - sub(j) * workC_[j - 1];
        workC_[j] = j == j1 ? 0.0 : sup(j) / denom;
        workD_[j] = (next_[j] - sub(j) * workD_[j - 1]) / denom;
      }
      next_[j1] = workD_[j1];
      for (std::size_t j = j1; j > j0; --j)
        next_[j - 1] = workD_[j - 1] - workC_[j - 1] * next_[j];
    }
    if (sys.stiffLeft != StiffEnd::Free) next_[0] = 0.0;
    if (sys.stiffRight != StiffEnd::Free) next_[m] = 0.0;
    std::swap(prev_, cur_);
    std::swap(cur_, next_);
    ++steps_;
  }

  [[nodiscard]] double stiffMu() const {
    return shape_.system().stiffness * dt_ / (dx_ * dx_);
  }

  // D^2 cur_ into biharm_ (Fixed-Fixed hinged: D u = 0 at both end nodes).
  void computeBiharmonic() {
    const std::size_t m = cur_.size() - 1;
    lap_[0] = 0.0;
    lap_[m] = 0.0;
    for (std::size_t j = 1; j < m; ++j)
      lap_[j] = cur_[j + 1] - 2.0 * cur_[j] + cur_[j - 1];
    biharm_[0] = 0.0;
    biharm_[m] = 0.0;
    for (std::size_t j = 1; j < m; ++j)
      biharm_[j] = lap_[j + 1] - 2.0 * lap_[j] + lap_[j - 1];
  }

  Fdtd1D(InitialShape shape, std::size_t intervals, double courant)
      : shape_(std::move(shape)), courant_(courant) {
    const double l = shape_.system().length.value();
    dx_ = l / static_cast<double>(intervals);
    dt_ = courant * dx_ / shape_.system().speed.value();
    initial_.resize(intervals + 1);
    for (std::size_t j = 0; j <= intervals; ++j)
      initial_[j] = shape_.valueAt(j == intervals ? l
                                                  : static_cast<double>(j) * dx_);
    cur_ = initial_;
    prev_.assign(intervals + 1, 0.0);
    next_.assign(intervals + 1, 0.0);
    workC_.assign(intervals + 1, 0.0);
    workD_.assign(intervals + 1, 0.0);
    lap_.assign(intervals + 1, 0.0);
    biharm_.assign(intervals + 1, 0.0);
    extCur_.assign(intervals + 5, 0.0);
    extPrev_.assign(intervals + 5, 0.0);
  }

  InitialShape shape_;
  double courant_;
  double dx_ = 0.0;
  double dt_ = 0.0;
  std::vector<double> initial_;
  std::vector<double> prev_;
  std::vector<double> cur_;
  std::vector<double> next_;
  std::vector<double> workC_;  // Thomas sweep scratch (preallocated)
  std::vector<double> workD_;
  std::vector<double> lap_;     // stiffness scratch (preallocated)
  std::vector<double> biharm_;
  std::vector<double> extCur_;   // ghost-filled copies (D13, preallocated)
  std::vector<double> extPrev_;
  std::uint64_t steps_ = 0;
};

// D12 — exact discrete dispersion of the schemes above, per mode.
//
// On this grid the continuous mode shapes are EXACT eigenvectors of the
// discrete operators: D phi_n = -p phi_n with p = 4 sin^2(k_n dx/2) for
// sin (Fixed left) and cos (Free left, mirror ghost) families alike, and
// D^2 phi_n = p^2 phi_n under hinged ghosts. Substituting u^n = z^n phi_n:
//   lossless:  z + 1/z = 2 - lam^2 p - mu^2 p^2
//              -> cos(w dt) = 1 - (lam^2 p + mu^2 p^2)/2
//   lossy:     (1+s+rp) z^2 - (2 - lam^2 p - mu^2 p^2) z + (1-s-rp) = 0
//              -> |z| = sqrt((1-s-rp)/(1+s+rp)),
//                 cos(w dt) = (2 - lam^2 p - mu^2 p^2) / (2 sqrt((1+s+rp)(1-s-rp)))
// so each mode's numerical frequency w/2pi and decay rate -ln|z|/dt follow
// in closed form. Ideal string at lam = 1: cos(w dt) = cos(k dx), w = c k
// exactly (D8's exactness). Verified in the tests three ways: a single-mode
// run stays proportional to phi_n at every node; FFT-measured frequency
// equals this prediction (not the continuous one); measured decay equals the
// predicted numerical decay. The start step does not change these (it only
// sets the mode's amplitude/phase).
struct DiscreteMode {
  FrequencyHz frequency;
  double decayRate;  // 1/s, amplitude e^{-rate t}
};

// Mode n of an FDTD grid (same validation as Fdtd1D::create). Valid modes:
// n in [1, M-1] for Fixed-Fixed (n = M is the zero grid function), n in
// [1, M] otherwise; else InvalidArgument. A mode whose discrete roots are
// real (non-oscillating on this grid) is DomainError.
[[nodiscard]] inline Result<DiscreteMode, QuantityError> fdtdDiscreteMode(
    const WaveSystem1D& system, std::size_t intervals, double courant, int n) {
  using R = Result<DiscreteMode, QuantityError>;
  if (intervals < 2 || intervals > kMaxGridIntervals)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(courant) || !(courant > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (courant > 1.0) return R::failure(QuantityError::DomainError);
  const bool fixedFixed =
      system.left == Boundary::Fixed && system.right == Boundary::Fixed;
  const auto maxMode = static_cast<long long>(fixedFixed ? intervals - 1
                                                         : intervals);
  if (n < 1 || static_cast<long long>(n) > maxMode)
    return R::failure(QuantityError::InvalidArgument);
  const double c = system.speed.value();
  const double dx = system.length.value() / static_cast<double>(intervals);
  const double dt = courant * dx / c;
  const double mu = system.stiffness * dt / (dx * dx);
  if (system.stiffness != 0.0 && courant * courant + 4.0 * mu * mu > 1.0)
    return R::failure(QuantityError::DomainError);
  // Exact eigenvectors exist only for closed-form (sin/cos) modes; clamped
  // and free stiff ends have no such discrete dispersion relation here.
  if (!detail::closedFormModes(system))
    return R::failure(QuantityError::DomainError);
  const double k = detail::modeWavenumber(system, static_cast<std::size_t>(n));
  const double sh = std::sin(0.5 * k * dx);
  const double p = 4.0 * sh * sh;
  const double s = system.loss.sigma0 * dt;
  const double r = system.loss.sigma1 * dt / (dx * dx);
  const double a = 1.0 + s + r * p;
  const double cc = 1.0 - s - r * p;
  const double b = 2.0 - courant * courant * p - mu * mu * p * p;
  const double denom = 2.0 * std::sqrt(a * cc);
  if (!(cc > 0.0) || !(std::fabs(b) < denom))
    return R::failure(QuantityError::DomainError);
  const double theta = std::acos(b / denom);
  const double rho = std::sqrt(cc / a);
  return R::success(DiscreteMode{FrequencyHz{theta / (constants::twoPi * dt)},
                                 -std::log(rho) / dt});
}

// Pitch error of FDTD mode n against the continuous model's (damped) mode
// frequency, in cents: 1200 log2(f_grid / f_model). Validation as above plus
// modeFrequency's (a non-oscillating continuous mode is DomainError).
[[nodiscard]] inline Result<double, QuantityError> fdtdPitchErrorCents(
    const WaveSystem1D& system, std::size_t intervals, double courant, int n) {
  using R = Result<double, QuantityError>;
  auto grid = fdtdDiscreteMode(system, intervals, courant, n);
  if (!grid.ok()) return R::failure(grid.error());
  auto model = modeFrequency(system, n);
  if (!model.ok()) return R::failure(model.error());
  return R::success(1200.0 * std::log2(grid.value().frequency.value() /
                                       model.value().value()));
}

// D14 — fine pitch tuning of FDTD between grid points ("pre-warping").
//
// An audio-rate grid has dt = 1/fs and dx = L/M with integer M, so the
// grid's partials land where D12's dispersion relation puts them, not on
// the model's pitch. Solving that relation backwards for the Courant number
// makes ONE chosen mode n exact. With mu = kappa dt/dx^2 and the loss terms
// fixed by dt and dx (s = sigma0 dt, r = sigma1 dt/dx^2, a = 1+s+rp,
// c = 1-s-rp), the target w = 2 pi f_n(model) needs
//   lam^2 = (2 - mu^2 p^2 - 2 cos(w dt) sqrt(a c)) / p
// (lossless: a = c = 1). The warped system is the same object with
// c' = lam dx fs, so it runs at exactly dt = 1/fs. Other modes keep D12's
// dispersion error, now relative to an exact mode n. DomainError if the
// required lam is not in (0, 1] or breaks lam^2 + 4 mu^2 <= 1, or if the
// system has no closed-form modes (clamped/free stiff ends).
struct PrewarpedGrid {
  WaveSystem1D system;  // wave speed replaced by c' = lam dx fs
  double courant;
};

[[nodiscard]] inline Result<PrewarpedGrid, QuantityError> fdtdPrewarpForPitch(
    const WaveSystem1D& system, std::size_t intervals, SampleRateHz rate,
    int n) {
  using R = Result<PrewarpedGrid, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || !(fs > 0.0) || intervals < 2 ||
      intervals > kMaxGridIntervals)
    return R::failure(QuantityError::InvalidArgument);
  if (!detail::closedFormModes(system))
    return R::failure(QuantityError::DomainError);
  const bool fixedFixed =
      system.left == Boundary::Fixed && system.right == Boundary::Fixed;
  const auto maxMode = static_cast<long long>(fixedFixed ? intervals - 1
                                                         : intervals);
  if (n < 1 || static_cast<long long>(n) > maxMode)
    return R::failure(QuantityError::InvalidArgument);
  auto target = modeFrequency(system, n);
  if (!target.ok()) return R::failure(target.error());
  const double dt = 1.0 / fs;
  const double dx = system.length.value() / static_cast<double>(intervals);
  const double k = detail::modeWavenumber(system, static_cast<std::size_t>(n));
  const double sh = std::sin(0.5 * k * dx);
  const double p = 4.0 * sh * sh;
  const double mu = system.stiffness * dt / (dx * dx);
  const double sl = system.loss.sigma0 * dt;
  const double r = system.loss.sigma1 * dt / (dx * dx);
  const double a = 1.0 + sl + r * p;
  const double cc = 1.0 - sl - r * p;
  if (!(cc > 0.0)) return R::failure(QuantityError::DomainError);
  const double w = constants::twoPi * target.value().value();
  const double lam2 =
      (2.0 - mu * mu * p * p - 2.0 * std::cos(w * dt) * std::sqrt(a * cc)) / p;
  if (!(lam2 > 0.0) || lam2 > 1.0 || lam2 + 4.0 * mu * mu > 1.0)
    return R::failure(QuantityError::DomainError);
  const double lam = std::sqrt(lam2);
  WaveSystem1D warped = system;
  warped.speed = SpeedMetersPerSecond{lam * dx * fs};
  return R::success(PrewarpedGrid{warped, lam});
}

struct StableGrid {
  std::size_t intervals;
  double courant;
};

// Finest grid whose Courant-1-per-frame time step (dt = 1/fs) is stable for
// the system: largest M with lam^2 + 4 mu^2 <= 1 (for kappa = 0 this is
// M = floor(L*fs/c), lam <= 1). Unlike gridIntervalsForRate this does NOT
// require an exact lam = 1 grid — stiff systems cannot have one — so the
// pitch comes from the model and the scheme's dispersion, not from integer
// delay lengths. M < 2 is DomainError.
[[nodiscard]] inline Result<StableGrid, QuantityError> stableGridForRate(
    const WaveSystem1D& system, SampleRateHz rate) {
  using R = Result<StableGrid, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double c = system.speed.value();
  const double l = system.length.value();
  const double kappa = system.stiffness;
  const double dt = 1.0 / fs;
  const double cdt2 = c * c * dt * dt;
  const double hMin =
      std::sqrt(0.5 * (cdt2 + std::sqrt(cdt2 * cdt2 + 16.0 * kappa * kappa *
                                                          dt * dt)));
  double mFloor = std::floor(l / hMin);
  if (!std::isfinite(mFloor) || mFloor > static_cast<double>(kMaxGridIntervals))
    return R::failure(QuantityError::DomainError);
  // Guard the closed form against roundoff at the limit by repeating
  // Fdtd1D::create's own arithmetic (it recomputes dt from the courant
  // returned here), so an accepted grid is never rejected by create().
  auto courantFor = [&](double m) { return c * dt * m / l; };
  auto stable = [&](double m) {
    const double lam = courantFor(m);
    const double dx = l / m;
    const double dtc = lam * dx / c;
    const double mu = kappa * dtc / (dx * dx);
    return lam <= 1.0 && (kappa == 0.0 || lam * lam + 4.0 * mu * mu <= 1.0);
  };
  while (mFloor >= 2.0 && !stable(mFloor)) mFloor -= 1.0;
  if (mFloor < 2.0) return R::failure(QuantityError::DomainError);
  return R::success(
      StableGrid{static_cast<std::size_t>(mFloor), courantFor(mFloor)});
}

// Grid intervals M = L*fs/c for which a lam = 1 grid steps exactly once per
// audio frame. A non-integer M would silently retune the object, so it is
// DomainError (fractional-delay tuning is deferred), as is M < 2.
[[nodiscard]] inline Result<std::size_t, QuantityError> gridIntervalsForRate(
    const WaveSystem1D& system, SampleRateHz rate) {
  using R = Result<std::size_t, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double m = system.length.value() * fs / system.speed.value();
  const double rounded = std::round(m);
  if (!std::isfinite(m) || std::fabs(m - rounded) > 1e-9 * rounded ||
      rounded < 2.0 || rounded > static_cast<double>(kMaxGridIntervals))
    return R::failure(QuantityError::DomainError);
  return R::success(static_cast<std::size_t>(rounded));
}

}  // namespace mmathen
