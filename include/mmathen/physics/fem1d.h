#pragma once

// D17 — finite element method (FEM) rung for the 1-D wave systems: modal
// frequencies from the generalized eigenproblem K v = w^2 M v.
//
// Two element families, uniform mesh of E elements, h = L/E, equations per
// unit mass (u_tt = c^2 u_xx - kappa^2 u_xxxx):
//   Linear (strings, no stiffness): per element
//     K_e = (c^2/h) [1 -1; -1 1],  M_e = (h/6) [2 1; 1 2] (consistent mass).
//     Dirichlet ends remove the end dof; Neumann ends keep it.
//     Closed-form discrete spectrum on a uniform Fixed-Fixed mesh (verified
//     in the tests):  w^2 = (6 c^2/h^2)(1 - cos kh)/(2 + cos kh), k = n pi/L.
//   Hermite cubic (stiff strings, any StiffEnd), dofs (w, w') per node:
//     K_b = (kappa^2/h^3)[12 6h -12 6h; 6h 4h^2 -6h 2h^2; -12 -6h 12 -6h;
//                          6h 2h^2 -6h 4h^2]                      (bending)
//     K_g = (c^2/(30h))[36 3h -36 3h; 3h 4h^2 -3h -h^2; -36 -3h 36 -3h;
//                        3h -h^2 -3h 4h^2]                        (tension)
//     M   = (h/420)[156 22h 54 -13h; 22h 4h^2 13h -3h^2; 54 13h 156 -22h;
//                   -13h -3h^2 -22h 4h^2]                         (mass)
//     Hinged removes w; Clamped removes w and w'; Free removes nothing
//     (moment and shear conditions are natural in the weak form).
//
// Eigenvalues by bisection on the Sylvester inertia count: with M positive
// definite, the number of negative pivots of the symmetric banded K - s M
// (LDL^T without pivoting) equals the number of eigenvalues below s. Each
// eigenvalue is bracketed and bisected to machine resolution; an exactly
// zero pivot perturbs s by one ulp. Rigid-body eigenvalues (w^2 below 1e-9
// of the first elastic scale, e.g. a free-free translation) are skipped.
//
// FEM here is a MODAL rung: frequencies, not time histories. It is an
// independent method for the stiff-end modes of D13 (converging at the
// Hermite rate h^4) and for string modes (h^2, from above).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"
#include "wave1d.h"

namespace mmathen {

namespace detail {
// Symmetric banded matrix, half-bandwidth bw, stored as dense (n x (bw+1))
// upper band: a[i][d] = A(i, i+d).
struct BandMatrix {
  std::size_t n = 0;
  std::size_t bw = 0;
  std::vector<double> a;
  BandMatrix(std::size_t size, std::size_t band)
      : n(size), bw(band), a(size * (band + 1), 0.0) {}
  double& at(std::size_t i, std::size_t d) { return a[i * (bw + 1) + d]; }
  double get(std::size_t i, std::size_t d) const {
    return a[i * (bw + 1) + d];
  }
  void add(std::size_t i, std::size_t j, double v) {
    if (j < i) std::swap(i, j);
    if (j - i <= bw) at(i, j - i) += v;
  }
};

// Number of eigenvalues of (K, M) below s: negative pivots of K - s M.
inline std::size_t inertiaBelow(const BandMatrix& k, const BandMatrix& m,
                                double s) {
  const std::size_t n = k.n;
  const std::size_t bw = k.bw;
  std::vector<double> w(n * (bw + 1));
  for (std::size_t i = 0; i < w.size(); ++i) w[i] = k.a[i] - s * m.a[i];
  auto at = [&](std::size_t i, std::size_t d) -> double& {
    return w[i * (bw + 1) + d];
  };
  std::size_t negatives = 0;
  for (std::size_t i = 0; i < n; ++i) {
    double piv = at(i, 0);
    if (piv == 0.0) piv = 1e-300;  // exact zero pivot: count as positive side
    if (piv < 0.0) ++negatives;
    // Eliminate below the pivot within the band (symmetric update of rows
    // i+1 .. i+bw using row i).
    const std::size_t last = (std::min)(n - 1, i + bw);
    for (std::size_t r = i + 1; r <= last; ++r) {
      const double lri = at(i, r - i) / piv;
      for (std::size_t c = r; c <= last; ++c)
        at(r, c - r) -= lri * at(i, c - i);
    }
  }
  return negatives;
}

inline std::vector<double> bandEigenvalues(const BandMatrix& k,
                                           const BandMatrix& m,
                                           std::size_t count, double upper) {
  std::vector<double> out;
  double lo = 0.0;
  for (std::size_t idx = 0; idx < count; ++idx) {
    // Find s with inertia >= idx+1, then bisect the (idx+1)-th eigenvalue.
    double hi = upper;
    if (inertiaBelow(k, m, hi) < idx + 1) break;
    double a = lo, b = hi;
    for (int it = 0; it < 300; ++it) {
      const double mid = 0.5 * (a + b);
      if (!(mid > a) || !(mid < b)) break;
      if (inertiaBelow(k, m, mid) >= idx + 1)
        b = mid;
      else
        a = mid;
    }
    out.push_back(0.5 * (a + b));
  }
  return out;
}
}  // namespace detail

// Linear-element FEM frequencies of a non-stiff system (any Fixed/Free
// ends), first `count` non-zero modes. elements in [2, 100000], count >= 1
// and < number of free dofs; stiff systems are InvalidArgument (use the
// Hermite family).
[[nodiscard]] inline Result<std::vector<FrequencyHz>, QuantityError>
femStringFrequencies(const WaveSystem1D& system, std::size_t elements,
                     std::size_t count) {
  using R = Result<std::vector<FrequencyHz>, QuantityError>;
  if (system.stiffness != 0.0 || elements < 2 || elements > 100000 ||
      count < 1)
    return R::failure(QuantityError::InvalidArgument);
  const double h = system.length.value() / static_cast<double>(elements);
  const double c2 = system.speed.value() * system.speed.value();
  // Global node numbering 0..E, then drop Dirichlet end dofs.
  const std::size_t first = system.left == Boundary::Fixed ? 1 : 0;
  const std::size_t last =
      system.right == Boundary::Fixed ? elements - 1 : elements;
  const std::size_t n = last - first + 1;
  if (count >= n) return R::failure(QuantityError::InvalidArgument);
  detail::BandMatrix k(n, 1), m(n, 1);
  for (std::size_t e = 0; e < elements; ++e) {
    const std::size_t nodes[2] = {e, e + 1};
    const double ke[2][2] = {{c2 / h, -c2 / h}, {-c2 / h, c2 / h}};
    const double me[2][2] = {{2.0 * h / 6.0, h / 6.0}, {h / 6.0, 2.0 * h / 6.0}};
    for (int a = 0; a < 2; ++a)
      for (int b = 0; b < 2; ++b) {
        const std::size_t ga = nodes[a], gb = nodes[b];
        if (ga < first || ga > last || gb < first || gb > last) continue;
        if (gb < ga) continue;
        k.add(ga - first, gb - first, ke[a][b]);
        m.add(ga - first, gb - first, me[a][b]);
      }
  }
  // Upper bound on w^2: element Rayleigh bound max row sum / min mass scale.
  const double upper = 12.0 * c2 / (h * h) * 1.01;
  const double rigid = system.left == Boundary::Free &&
                       system.right == Boundary::Free;
  auto vals = detail::bandEigenvalues(k, m, count + (rigid ? 1 : 0), upper);
  std::vector<FrequencyHz> out;
  const double scale = c2 * constants::pi * constants::pi /
                       (system.length.value() * system.length.value());
  for (const double v : vals) {
    if (v < 1e-9 * scale) continue;  // rigid translation (Free-Free)
    out.push_back(FrequencyHz{std::sqrt(v) / constants::twoPi});
    if (out.size() == count) break;
  }
  if (out.size() < count) return R::failure(QuantityError::RangeError);
  return R::success(std::move(out));
}

// Hermite-cubic FEM frequencies of a stiff system (any StiffEnd pair),
// first `count` elastic modes. elements in [2, 20000]; requires stiffness.
[[nodiscard]] inline Result<std::vector<FrequencyHz>, QuantityError>
femStiffFrequencies(const WaveSystem1D& system, std::size_t elements,
                    std::size_t count) {
  using R = Result<std::vector<FrequencyHz>, QuantityError>;
  if (!(system.stiffness > 0.0) || elements < 2 || elements > 20000 ||
      count < 1)
    return R::failure(QuantityError::InvalidArgument);
  const double h = system.length.value() / static_cast<double>(elements);
  const double c2 = system.speed.value() * system.speed.value();
  const double k2 = system.stiffness * system.stiffness;
  const std::size_t dofs = 2 * (elements + 1);
  // Constrained dofs.
  std::vector<bool> fixed(dofs, false);
  auto constrain = [&](StiffEnd end, std::size_t node) {
    if (end == StiffEnd::Hinged || end == StiffEnd::Clamped)
      fixed[2 * node] = true;
    if (end == StiffEnd::Clamped) fixed[2 * node + 1] = true;
  };
  constrain(system.stiffLeft, 0);
  constrain(system.stiffRight, elements);
  std::vector<std::size_t> map(dofs, 0);
  std::size_t n = 0;
  for (std::size_t d = 0; d < dofs; ++d)
    if (!fixed[d]) map[d] = n++;
  if (count + 2 >= n) return R::failure(QuantityError::InvalidArgument);
  detail::BandMatrix kk(n, 3), mm(n, 3);
  const double hh = h * h;
  const double kb[4][4] = {{12, 6 * h, -12, 6 * h},
                           {6 * h, 4 * hh, -6 * h, 2 * hh},
                           {-12, -6 * h, 12, -6 * h},
                           {6 * h, 2 * hh, -6 * h, 4 * hh}};
  const double kg[4][4] = {{36, 3 * h, -36, 3 * h},
                           {3 * h, 4 * hh, -3 * h, -hh},
                           {-36, -3 * h, 36, -3 * h},
                           {3 * h, -hh, -3 * h, 4 * hh}};
  const double me[4][4] = {{156, 22 * h, 54, -13 * h},
                           {22 * h, 4 * hh, 13 * h, -3 * hh},
                           {54, 13 * h, 156, -22 * h},
                           {-13 * h, -3 * hh, -22 * h, 4 * hh}};
  for (std::size_t e = 0; e < elements; ++e) {
    const std::size_t g[4] = {2 * e, 2 * e + 1, 2 * e + 2, 2 * e + 3};
    for (int a = 0; a < 4; ++a)
      for (int b = 0; b < 4; ++b) {
        if (fixed[g[a]] || fixed[g[b]]) continue;
        const std::size_t ia = map[g[a]], ib = map[g[b]];
        if (ib < ia) continue;
        kk.add(ia, ib, k2 / (hh * h) * kb[a][b] + c2 / (30.0 * h) * kg[a][b]);
        mm.add(ia, ib, h / 420.0 * me[a][b]);
      }
  }
  // Generous upper bound on w^2 for the requested modes: a few times the
  // hinged continuous value of mode (count + 2).
  const double kmax = static_cast<double>(count + 2) * constants::pi /
                      system.length.value();
  const double upper = 16.0 * (k2 * kmax * kmax * kmax * kmax + c2 * kmax * kmax);
  const double scale = c2 * constants::pi * constants::pi /
                           (system.length.value() * system.length.value()) +
                       k2 * std::pow(constants::pi / system.length.value(), 4);
  auto vals = detail::bandEigenvalues(kk, mm, count + 2, upper);
  std::vector<FrequencyHz> out;
  for (const double v : vals) {
    if (v < 1e-9 * scale) continue;  // rigid-body modes
    out.push_back(FrequencyHz{std::sqrt(v) / constants::twoPi});
    if (out.size() == count) break;
  }
  if (out.size() < count) return R::failure(QuantityError::RangeError);
  return R::success(std::move(out));
}

}  // namespace mmathen
