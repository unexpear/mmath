#pragma once

// D17 — 2-D and 3-D wave equation on a rectangular box: FDTD rung.
//
//   u_tt = c^2 (u_xx + u_yy [+ u_zz])      on [0,Lx] x [0,Ly] [x [0,Lz]]
//
// Two objects share it: a rectangular MEMBRANE (all edges Dirichlet, u = 0;
// transverse displacement, c = sqrt(surface tension / areal density)) and a
// rectangular ROOM (all walls rigid, Neumann; acoustic pressure, c = speed
// of sound). Grid: equal spacing h on every axis, n_i intervals, so
// L_i = n_i h. Explicit leapfrog with the 2D+1 point Laplacian; Dirichlet
// boundary nodes stay 0, Neumann walls use mirror ghosts (as D8).
//
// Modes: Dirichlet sin(m_i pi x_i/L_i), m_i >= 1; Neumann cos(...), m_i >= 0
// (not all zero). Continuous frequency f = (c/2) sqrt(sum (m_i/L_i)^2)
// (Rayleigh; the room's axial series equals D2's pipe series n c/2L).
// Exact discrete dispersion (the same eigenvector argument as D12, per
// axis): cos(w dt) = 1 - lam^2/2 sum_i p_i,  p_i = 4 sin^2(k_i h/2).
// Stability (von Neumann, p_i <= 4): lam = c dt/h <= 1/sqrt(D); larger is
// DomainError. Discrete energy
//   E = 1/2 sum ((u^{n+1}-u^n)/dt)^2 + c^2/(2h^2) sum_axes sum D_i u^{n+1} D_i u^n
// is conserved (Dirichlet; tested). Lossless, homogeneous, ideal walls:
// verified, unvalidated (no wall absorption, no air loss, no membrane
// stiffness).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"

namespace mmathen {

enum class BoxWalls { Dirichlet, Neumann };

struct BoxGrid {
  int dimensions;                     // 2 or 3
  std::array<std::size_t, 3> intervals;  // n_x, n_y, n_z (n_z unused in 2-D)
  DistanceMeters spacing;             // h
  SpeedMetersPerSecond speed;
  BoxWalls walls;
};

inline constexpr std::size_t kMaxBoxNodes = std::size_t{1} << 24;

[[nodiscard]] inline Result<BoxGrid, QuantityError> makeBoxGrid(
    int dimensions, std::array<std::size_t, 3> intervals,
    DistanceMeters spacing, SpeedMetersPerSecond speed, BoxWalls walls) {
  using R = Result<BoxGrid, QuantityError>;
  if (dimensions != 2 && dimensions != 3)
    return R::failure(QuantityError::InvalidArgument);
  const double h = spacing.value();
  const double c = speed.value();
  if (!std::isfinite(h) || !(h > 0.0) || !std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  std::size_t nodes = 1;
  for (int d = 0; d < dimensions; ++d) {
    const std::size_t n = intervals[static_cast<std::size_t>(d)];
    if (n < 2 || n > 4096) return R::failure(QuantityError::InvalidArgument);
    nodes *= n + 1;
  }
  if (nodes > kMaxBoxNodes) return R::failure(QuantityError::InvalidArgument);
  if (dimensions == 2) intervals[2] = 0;
  return R::success(BoxGrid{dimensions, intervals, spacing, speed, walls});
}

namespace detail {
inline bool validBoxMode(const BoxGrid& g, std::array<int, 3> m) {
  bool anyNonZero = false;
  for (int d = 0; d < g.dimensions; ++d) {
    const int mi = m[static_cast<std::size_t>(d)];
    const auto n = static_cast<int>(g.intervals[static_cast<std::size_t>(d)]);
    if (g.walls == BoxWalls::Dirichlet && (mi < 1 || mi > n - 1)) return false;
    if (g.walls == BoxWalls::Neumann && (mi < 0 || mi > n)) return false;
    anyNonZero = anyNonZero || mi != 0;
  }
  return anyNonZero;
}
}  // namespace detail

// Continuous (Rayleigh) frequency of box mode m on the grid's box.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> boxModeFrequency(
    const BoxGrid& g, std::array<int, 3> m) {
  using R = Result<FrequencyHz, QuantityError>;
  if (!detail::validBoxMode(g, m)) return R::failure(QuantityError::InvalidArgument);
  double sum = 0.0;
  for (int d = 0; d < g.dimensions; ++d) {
    const auto i = static_cast<std::size_t>(d);
    const double l = static_cast<double>(g.intervals[i]) * g.spacing.value();
    const double q = static_cast<double>(m[i]) / l;
    sum += q * q;
  }
  return R::success(FrequencyHz{0.5 * g.speed.value() * std::sqrt(sum)});
}

// Exact numerical frequency of mode m on this grid at Courant lam.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> boxDiscreteModeFrequency(
    const BoxGrid& g, double courant, std::array<int, 3> m) {
  using R = Result<FrequencyHz, QuantityError>;
  if (!detail::validBoxMode(g, m) || !std::isfinite(courant) ||
      !(courant > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (courant > 1.0 / std::sqrt(static_cast<double>(g.dimensions)))
    return R::failure(QuantityError::DomainError);
  const double h = g.spacing.value();
  double psum = 0.0;
  for (int d = 0; d < g.dimensions; ++d) {
    const auto i = static_cast<std::size_t>(d);
    const double l = static_cast<double>(g.intervals[i]) * h;
    const double k = static_cast<double>(m[i]) * constants::pi / l;
    const double s = std::sin(0.5 * k * h);
    psum += 4.0 * s * s;
  }
  const double cosv = 1.0 - 0.5 * courant * courant * psum;
  const double dt = courant * h / g.speed.value();
  return R::success(FrequencyHz{std::acos(cosv) / (constants::twoPi * dt)});
}

// Initial field with zero initial velocity: a single box mode (amplitude
// times the product of sin/cos factors) or a raised-cosine bump.
struct BoxInitial {
  enum class Kind { Mode, Bump } kind;
  std::array<int, 3> mode;
  std::array<double, 3> center;  // meters
  double radius;                 // meters
  double amplitude;
};

[[nodiscard]] inline Result<BoxInitial, QuantityError> boxModeInitial(
    const BoxGrid& g, std::array<int, 3> m, double amplitude) {
  using R = Result<BoxInitial, QuantityError>;
  if (!detail::validBoxMode(g, m) || !std::isfinite(amplitude))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(BoxInitial{BoxInitial::Kind::Mode, m, {}, 0.0, amplitude});
}

// Raised-cosine bump a*(1+cos(pi r/R))/2 for r < R, zero outside.
[[nodiscard]] inline Result<BoxInitial, QuantityError> boxBumpInitial(
    const BoxGrid& g, std::array<double, 3> center, DistanceMeters radius,
    double amplitude) {
  using R = Result<BoxInitial, QuantityError>;
  const double rr = radius.value();
  if (!std::isfinite(rr) || !(rr > 0.0) || !std::isfinite(amplitude))
    return R::failure(QuantityError::InvalidArgument);
  for (int d = 0; d < g.dimensions; ++d) {
    const auto i = static_cast<std::size_t>(d);
    const double l = static_cast<double>(g.intervals[i]) * g.spacing.value();
    if (!std::isfinite(center[i]) || center[i] < 0.0 || center[i] > l)
      return R::failure(QuantityError::InvalidArgument);
  }
  return R::success(
      BoxInitial{BoxInitial::Kind::Bump, {}, center, rr, amplitude});
}

class FdtdBox {
 public:
  [[nodiscard]] static Result<FdtdBox, QuantityError> create(
      const BoxGrid& grid, const BoxInitial& initial, double courant) {
    using R = Result<FdtdBox, QuantityError>;
    if (!std::isfinite(courant) || !(courant > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (courant > 1.0 / std::sqrt(static_cast<double>(grid.dimensions)))
      return R::failure(QuantityError::DomainError);
    return R::success(FdtdBox{grid, initial, courant});
  }

  void step() {
    const double lam2 = courant_ * courant_;
    const std::size_t nx = g_.intervals[0], ny = g_.intervals[1];
    const std::size_t nz = g_.dimensions == 3 ? g_.intervals[2] : 0;
    const bool dir = g_.walls == BoxWalls::Dirichlet;
    auto at = [&](const std::vector<double>& u, std::size_t i, std::size_t j,
                  std::size_t k) { return u[index(i, j, k)]; };
    // Neighbour sum along one axis with mirror ghosts for Neumann walls.
    auto axisSum = [&](const std::vector<double>& u, std::size_t i,
                       std::size_t j, std::size_t k, int axis) {
      std::size_t n = axis == 0 ? nx : axis == 1 ? ny : nz;
      std::size_t c = axis == 0 ? i : axis == 1 ? j : k;
      auto val = [&](std::size_t cc) {
        return axis == 0 ? at(u, cc, j, k) : axis == 1 ? at(u, i, cc, k)
                                                       : at(u, i, j, cc);
      };
      if (c == 0) return 2.0 * val(1);
      if (c == n) return 2.0 * val(n - 1);
      return val(c + 1) + val(c - 1);
    };
    for (std::size_t k = 0; k <= nz; ++k)
      for (std::size_t j = 0; j <= ny; ++j)
        for (std::size_t i = 0; i <= nx; ++i) {
          const std::size_t id = index(i, j, k);
          if (dir && onBoundary(i, j, k)) {
            next_[id] = 0.0;
            continue;
          }
          double lap = 0.0;
          for (int a = 0; a < g_.dimensions; ++a)
            lap += axisSum(cur_, i, j, k, a) - 2.0 * cur_[id];
          next_[id] = steps_ == 0 ? cur_[id] + 0.5 * lam2 * lap
                                  : 2.0 * cur_[id] - prev_[id] + lam2 * lap;
        }
    std::swap(prev_, cur_);
    std::swap(cur_, next_);
    ++steps_;
  }

  [[nodiscard]] Result<double, QuantityError> valueAt(std::size_t i,
                                                      std::size_t j,
                                                      std::size_t k = 0) const {
    using R = Result<double, QuantityError>;
    if (i > g_.intervals[0] || j > g_.intervals[1] ||
        k > (g_.dimensions == 3 ? g_.intervals[2] : 0))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(cur_[index(i, j, k)]);
  }
  [[nodiscard]] const std::vector<double>& field() const noexcept { return cur_; }
  [[nodiscard]] const std::vector<double>& previousField() const noexcept {
    return prev_;
  }
  [[nodiscard]] DurationSeconds timeStep() const noexcept {
    return DurationSeconds{dt_};
  }
  [[nodiscard]] double courant() const noexcept { return courant_; }
  [[nodiscard]] const BoxGrid& grid() const noexcept { return g_; }
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_; }
  [[nodiscard]] std::size_t index(std::size_t i, std::size_t j,
                                  std::size_t k) const noexcept {
    return i + (g_.intervals[0] + 1) * (j + (g_.intervals[1] + 1) * k);
  }

 private:
  FdtdBox(const BoxGrid& g, const BoxInitial& init, double courant)
      : g_(g), courant_(courant) {
    const double h = g.spacing.value();
    dt_ = courant * h / g.speed.value();
    const std::size_t nx = g.intervals[0], ny = g.intervals[1];
    const std::size_t nz = g.dimensions == 3 ? g.intervals[2] : 0;
    const std::size_t total = (nx + 1) * (ny + 1) * (nz + 1);
    cur_.assign(total, 0.0);
    prev_.assign(total, 0.0);
    next_.assign(total, 0.0);
    const bool dir = g.walls == BoxWalls::Dirichlet;
    for (std::size_t k = 0; k <= nz; ++k)
      for (std::size_t j = 0; j <= ny; ++j)
        for (std::size_t i = 0; i <= nx; ++i) {
          if (dir && onBoundary(i, j, k)) continue;
          const std::array<double, 3> x = {static_cast<double>(i) * h,
                                           static_cast<double>(j) * h,
                                           static_cast<double>(k) * h};
          double v = init.amplitude;
          if (init.kind == BoxInitial::Kind::Mode) {
            for (int d = 0; d < g.dimensions; ++d) {
              const auto a = static_cast<std::size_t>(d);
              const double l = static_cast<double>(g.intervals[a]) * h;
              const double kx =
                  static_cast<double>(init.mode[a]) * constants::pi * x[a] / l;
              v *= dir ? std::sin(kx) : std::cos(kx);
            }
          } else {
            double r2 = 0.0;
            for (int d = 0; d < g.dimensions; ++d) {
              const auto a = static_cast<std::size_t>(d);
              r2 += (x[a] - init.center[a]) * (x[a] - init.center[a]);
            }
            const double r = std::sqrt(r2);
            v = r < init.radius
                    ? init.amplitude * 0.5 *
                          (1.0 + std::cos(constants::pi * r / init.radius))
                    : 0.0;
          }
          cur_[index(i, j, k)] = v;
        }
  }

  [[nodiscard]] bool onBoundary(std::size_t i, std::size_t j,
                                std::size_t k) const noexcept {
    if (i == 0 || i == g_.intervals[0] || j == 0 || j == g_.intervals[1])
      return true;
    return g_.dimensions == 3 && (k == 0 || k == g_.intervals[2]);
  }

  BoxGrid g_;
  double courant_;
  double dt_ = 0.0;
  std::vector<double> prev_, cur_, next_;
  std::uint64_t steps_ = 0;
};

}  // namespace mmathen
