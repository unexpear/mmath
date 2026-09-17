// D13 — clamped and free stiff ends (Catch2).
//
// Independent anchors:
// 1. Beam limit (tension -> 0): the determinant's roots equal the classical
//    Euler-Bernoulli roots, computed HERE by bisection on the textbook
//    characteristic equations cos x cosh x = +-1 and tan x = tanh x.
// 2. Hinged-hinged through the determinant equals the closed form n pi/L.
// 3. Clamped-clamped approaches Fletcher (1964)'s approximation
//    f_n = n f0 sqrt(1+Bn^2)(1 + 2 sqrt(B)/pi + 4B/pi^2), with a residual
//    that scales as B^1.5 (the order of the terms Fletcher drops).
// 4. FDTD with the new ghost rows: FFT-measured partials converge to the
//    determinant frequencies at second order; long runs at the stability
//    limit stay bounded.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/fdtd1d.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

constexpr double kPi = std::numbers::pi;

WaveSystem1D stiffSystem(double length, double c, double kappa, StiffEnd left,
                         StiffEnd right) {
  const Boundary bl = left == StiffEnd::Free ? Boundary::Free : Boundary::Fixed;
  const Boundary br =
      right == StiffEnd::Free ? Boundary::Free : Boundary::Fixed;
  auto base = unwrap(makeWaveSystem1D(DistanceMeters{length},
                                      SpeedMetersPerSecond{c}, bl, br));
  return unwrap(withStiffness(base, kappa, left, right));
}

double bisectRoot(const std::function<double(double)>& f, double a, double b) {
  double fa = f(a);
  for (int i = 0; i < 200; ++i) {
    const double m = 0.5 * (a + b);
    const double fm = f(m);
    if ((fm < 0.0) == (fa < 0.0)) {
      a = m;
      fa = fm;
    } else {
      b = m;
    }
  }
  return 0.5 * (a + b);
}
}  // namespace

TEST_CASE("beam limit reproduces classical Euler-Bernoulli roots",
          "[d13ends]") {
  // Tension 1e-9 of the bending scale: pure beam to machine precision.
  const auto cc1 = [](double x) { return std::cos(x) * std::cosh(x) - 1.0; };
  const auto ccm1 = [](double x) { return std::cos(x) * std::cosh(x) + 1.0; };
  const auto ttan = [](double x) { return std::tan(x) - std::tanh(x); };
  struct Case {
    StiffEnd left, right;
    double r1, r2;
  };
  const std::vector<Case> cases = {
      {StiffEnd::Clamped, StiffEnd::Free, bisectRoot(ccm1, 1.5, 2.5),
       bisectRoot(ccm1, 4.5, 5.0)},
      {StiffEnd::Clamped, StiffEnd::Clamped, bisectRoot(cc1, 4.5, 5.0),
       bisectRoot(cc1, 7.5, 8.0)},
      {StiffEnd::Free, StiffEnd::Free, bisectRoot(cc1, 4.5, 5.0),
       bisectRoot(cc1, 7.5, 8.0)},
      {StiffEnd::Hinged, StiffEnd::Free, bisectRoot(ttan, 3.5, 4.5),
       bisectRoot(ttan, 6.9, 7.2)},
      {StiffEnd::Clamped, StiffEnd::Hinged, bisectRoot(ttan, 3.5, 4.5),
       bisectRoot(ttan, 6.9, 7.2)},
  };
  for (const auto& k : cases) {
    const auto sys = stiffSystem(1.0, 1e-9, 1.0, k.left, k.right);
    const auto roots = unwrap(stiffModeWavenumbers(sys, 2));
    CHECK(nearlyEqual(roots[0], k.r1, 1e-12, 0.0));
    CHECK(nearlyEqual(roots[1], k.r2, 1e-12, 0.0));
  }
  // The textbook values themselves (e.g. Blevins, Formulas for Natural
  // Frequency and Mode Shape): cantilever 1.87510407, clamped-clamped
  // 4.73004074, pinned-free 3.92660231.
  CHECK(nearlyEqual(cases[0].r1, 1.87510407, 1e-8, 0.0));
  CHECK(nearlyEqual(cases[1].r1, 4.73004074, 1e-8, 0.0));
  CHECK(nearlyEqual(cases[3].r1, 3.92660231, 1e-8, 0.0));
}

TEST_CASE("hinged ends through the determinant equal the closed form",
          "[d13ends]") {
  const auto sys = stiffSystem(1.3, 2.0, 0.05, StiffEnd::Hinged,
                               StiffEnd::Hinged);
  const auto roots = unwrap(stiffModeWavenumbers(sys, 12));
  for (std::size_t i = 0; i < roots.size(); ++i)
    CHECK(nearlyEqual(roots[i], static_cast<double>(i + 1) * kPi / 1.3, 0.0,
                      1e-12));
}

TEST_CASE("clamped-clamped approaches Fletcher's formula as B^1.5",
          "[d13ends]") {
  const double c = 100.0;
  const double f0 = c / 2.0;
  std::vector<double> residual;
  for (const double b : {1e-5, 1e-4, 1e-3}) {
    const double kappa = std::sqrt(b) * c / kPi;
    const auto sys = stiffSystem(1.0, c, kappa, StiffEnd::Clamped,
                                 StiffEnd::Clamped);
    const double det = unwrap(modeFrequency(sys, 1)).value();
    const double fletcher = f0 * std::sqrt(1.0 + b) *
                            (1.0 + 2.0 * std::sqrt(b) / kPi +
                             4.0 * b / (kPi * kPi));
    const double hinged = f0 * std::sqrt(1.0 + b);
    residual.push_back(det / fletcher - 1.0);
    // The clamping raises pitch by about 2 sqrt(B)/pi over hinged ends.
    CHECK(nearlyEqual(det / hinged - 1.0, 2.0 * std::sqrt(b) / kPi, 0.0,
                      0.05));
  }
  // Each decade of B multiplies the residual by about 10^1.5 = 31.6.
  for (std::size_t i = 0; i + 1 < residual.size(); ++i) {
    const double ratio = residual[i + 1] / residual[i];
    CHECK(ratio > 25.0);
    CHECK(ratio < 40.0);
  }
  CHECK(std::fabs(residual.back()) < 2e-5);
}

TEST_CASE("clamped and free FDTD converges to the determinant frequencies",
          "[d13ends]") {
  // L = 1, c = 20, kappa = 0.3; refine at fixed mu = 0.25 (dt ~ dx^2) over
  // a fixed 4 s window, measuring partials by FFT.
  const double c = 20.0;
  const double kappa = 0.3;
  struct Ends {
    StiffEnd left, right;
  };
  for (const auto& e : {Ends{StiffEnd::Clamped, StiffEnd::Clamped},
                        Ends{StiffEnd::Free, StiffEnd::Free},
                        Ends{StiffEnd::Clamped, StiffEnd::Free}}) {
    const auto sys = stiffSystem(1.0, c, kappa, e.left, e.right);
    // Pluck at 0.29 and pickup at 0.65: away from the nodes of modes 2-3 of
    // all three end pairs. (First choices failed for a physical reason, not
    // a numerical one: a pluck at 0.37 sits on the clamped-free mode-3 node
    // (~0.36), leaving that partial at -81 dB, and a pickup at 0.75 sits
    // near the free-free mode-2 node (~0.78). The spectrum showed only the
    // determinant's modes, no spurious ones.)
    const auto shape =
        unwrap(InitialShape::pluck(sys, DistanceMeters{0.29}, 1e-3));
    for (const int n : {2, 3}) {
      const double target = unwrap(modeFrequency(sys, n)).value();
      std::vector<double> errors;
      for (const std::size_t m : {40u, 80u, 160u}) {
        const double h = 1.0 / static_cast<double>(m);
        auto fdtd = unwrap(Fdtd1D::create(shape, m, c * 0.25 * h / kappa));
        const double fs = 1.0 / fdtd.timeStep().value();
        std::size_t frames = 1;
        while (static_cast<double>(frames) < 4.0 * fs) frames <<= 1;
        std::vector<double> x(frames);
        for (auto& v : x) {
          v = fdtd.field()[13 * m / 20];
          fdtd.step();
        }
        const auto est = unwrap(estimateSinusoid(
            x, SampleRateHz{fs}, Window::BlackmanHarris,
            FrequencyHz{target * 0.9}, FrequencyHz{target * 1.1}));
        errors.push_back(std::fabs(est.frequency.value() - target));
      }
      const double r = errors[1] / errors[2];
      CHECK(r > 3.3);
      CHECK(r < 4.7);
      CHECK(errors[2] / target < 5e-3);
    }
  }
}

TEST_CASE("clamped and free FDTD stays bounded at the stability limit",
          "[d13ends]") {
  const double c = 20.0;
  const double kappa = 0.3;
  const std::size_t m = 60;
  const double h = 1.0 / static_cast<double>(m);
  const double a = kappa / (c * h);
  // lam^2 + 4 mu^2 = 0.99 with mu = a lam.
  const double lam = std::sqrt(0.99 / (1.0 + 4.0 * a * a));
  struct Ends {
    StiffEnd left, right;
  };
  for (const auto& e : {Ends{StiffEnd::Clamped, StiffEnd::Clamped},
                        Ends{StiffEnd::Free, StiffEnd::Free},
                        Ends{StiffEnd::Clamped, StiffEnd::Free},
                        Ends{StiffEnd::Hinged, StiffEnd::Free}}) {
    const auto sys = stiffSystem(1.0, c, kappa, e.left, e.right);
    const auto shape =
        unwrap(InitialShape::pluck(sys, DistanceMeters{0.37}, 1e-3));
    auto fdtd = unwrap(Fdtd1D::create(shape, m, lam));
    double worst = 0.0;
    for (int s = 0; s < 200000; ++s) {
      fdtd.step();
      if (s % 97 == 0)
        for (const double v : fdtd.field())
          worst = (std::max)(worst, std::fabs(v));
    }
    CHECK(std::isfinite(worst));
    CHECK(worst < 2e-3);  // started at 1e-3; measured <= 1.21e-3
  }
}

TEST_CASE("sigma0 loss on clamped ends decays at sigma0", "[d13ends]") {
  auto sys = stiffSystem(1.0, 20.0, 0.3, StiffEnd::Clamped, StiffEnd::Free);
  sys = unwrap(withLoss(sys, LossModel{0.8, 0.0}));
  CHECK(unwrap(modeDecayRate(sys, 2)) == 0.8);
  const double w = 2.0 * kPi *
                   unwrap(modeFrequency(unwrap(withLoss(sys, LossModel{})), 2))
                       .value();
  CHECK(nearlyEqual(unwrap(modeFrequency(sys, 2)).value(),
                    std::sqrt(w * w - 0.64) / (2.0 * kPi), 0.0, 1e-12));
  // Lossy clamped/free FDTD runs and decays (sigma1 too, via the ghost-row
  // implicit solve).
  auto both = unwrap(withLoss(sys, LossModel{0.8, 0.002}));
  const auto shape =
      unwrap(InitialShape::pluck(both, DistanceMeters{0.37}, 1e-3));
  auto fdtd = unwrap(Fdtd1D::create(shape, 60, 0.4));
  double early = 0.0;
  double late = 0.0;
  for (int s = 0; s < 40000; ++s) {
    fdtd.step();
    for (const double v : fdtd.field()) {
      if (s < 2000) early = (std::max)(early, std::fabs(v));
      if (s > 38000) late = (std::max)(late, std::fabs(v));
    }
  }
  CHECK(std::isfinite(late));
  CHECK(late < 0.5 * early);
}

TEST_CASE("stiff end conditions are validated and solver limits explicit",
          "[d13ends]") {
  const auto fixedFixed = unwrap(makeWaveSystem1D(
      DistanceMeters{1.0}, SpeedMetersPerSecond{20.0}, Boundary::Fixed,
      Boundary::Fixed));
  // Free stiff end needs a Free boundary; clamped needs a Fixed one.
  CHECK(withStiffness(fixedFixed, 0.3, StiffEnd::Free, StiffEnd::Clamped)
            .error() == QuantityError::DomainError);
  const auto freeFree = unwrap(makeWaveSystem1D(
      DistanceMeters{1.0}, SpeedMetersPerSecond{20.0}, Boundary::Free,
      Boundary::Free));
  CHECK(withStiffness(freeFree, 0.3, StiffEnd::Clamped, StiffEnd::Free)
            .error() == QuantityError::DomainError);
  CHECK(withStiffness(fixedFixed, 0.0, StiffEnd::Clamped, StiffEnd::Clamped)
            .error() == QuantityError::InvalidArgument);
  // Solver compatibility, stated by rejection.
  const auto clamped =
      stiffSystem(1.0, 20.0, 0.3, StiffEnd::Clamped, StiffEnd::Clamped);
  const auto shape =
      unwrap(InitialShape::pluck(clamped, DistanceMeters{0.37}, 1e-3));
  CHECK(ModalSolution::create(shape, 10).error() == QuantityError::DomainError);
  CHECK(fdtdDiscreteMode(clamped, 60, 0.4, 2).error() ==
        QuantityError::DomainError);
  const auto sigma1 = unwrap(withLoss(clamped, LossModel{0.0, 0.01}));
  CHECK(modeFrequency(sigma1, 1).error() == QuantityError::DomainError);
  CHECK(modeDecayRate(sigma1, 1).error() == QuantityError::DomainError);
  CHECK(stiffModeWavenumbers(fixedFixed, 3).error() ==
        QuantityError::InvalidArgument);  // no stiffness
}
