// D17 — FEM rung (Catch2).
//
// Linear elements: the eigenvalues equal the closed-form consistent-mass
// dispersion w^2 = (6c^2/h^2)(1 - cos kh)/(2 + cos kh) to roundoff, and
// converge to the continuous series from above at O(h^2).
// Hermite cubics: a THIRD independent method for D13's clamped and free
// stiff ends — the eigenfrequencies converge to the boundary-determinant
// roots at the Hermite rate h^4 (ratio -> 16) once the mesh resolves the
// kappa/c boundary layer.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/physics/fem1d.h>
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
}  // namespace

TEST_CASE("linear FEM equals its closed-form dispersion", "[d17fem]") {
  const double l = 1.3, c = 2.0;
  const auto ff = unwrap(makeWaveSystem1D(DistanceMeters{l},
                                          SpeedMetersPerSecond{c},
                                          Boundary::Fixed, Boundary::Fixed));
  for (const std::size_t e : {20u, 40u}) {
    const auto f = unwrap(femStringFrequencies(ff, e, 5));
    const double h = l / static_cast<double>(e);
    for (int n = 1; n <= 5; ++n) {
      const double k = n * kPi / l;
      const double w2 = 6.0 * c * c / (h * h) * (1.0 - std::cos(k * h)) /
                        (2.0 + std::cos(k * h));
      CHECK(nearlyEqual(f[static_cast<std::size_t>(n - 1)].value(),
                        std::sqrt(w2) / (2.0 * kPi), 0.0, 1e-12));
    }
  }
  // Converges from above at O(h^2): error ratio 4 per halving.
  const auto f20 = unwrap(femStringFrequencies(ff, 20, 1));
  const auto f40 = unwrap(femStringFrequencies(ff, 40, 1));
  const double cont = c / (2.0 * l);
  CHECK(f20[0].value() > cont);
  CHECK(nearlyEqual((f20[0].value() - cont) / (f40[0].value() - cont), 4.0,
                    0.0, 0.01));
}

TEST_CASE("Hermite FEM converges to the D13 determinant at h^4",
          "[d17fem]") {
  struct Ends {
    StiffEnd left, right;
  };
  for (const auto& e : {Ends{StiffEnd::Clamped, StiffEnd::Clamped},
                        Ends{StiffEnd::Free, StiffEnd::Free},
                        Ends{StiffEnd::Clamped, StiffEnd::Free}}) {
    const Boundary bl =
        e.left == StiffEnd::Free ? Boundary::Free : Boundary::Fixed;
    const Boundary br =
        e.right == StiffEnd::Free ? Boundary::Free : Boundary::Fixed;
    const auto base = unwrap(makeWaveSystem1D(
        DistanceMeters{1.0}, SpeedMetersPerSecond{20.0}, bl, br));
    const auto sys = unwrap(withStiffness(base, 0.3, e.left, e.right));
    std::vector<double> err1, err3;
    for (const std::size_t n : {64u, 128u, 256u}) {
      const auto f = unwrap(femStiffFrequencies(sys, n, 3));
      err1.push_back(f[0].value() - unwrap(modeFrequency(sys, 1)).value());
      err3.push_back(f[2].value() - unwrap(modeFrequency(sys, 3)).value());
    }
    // Measured ratios 12.2 -> 14.8 -> 15.7: approaching 16 as h << kappa/c.
    CHECK(err1[0] > 0.0);  // from above (Rayleigh-Ritz)
    CHECK(err1[1] / err1[2] > 14.5);
    CHECK(err1[1] / err1[2] < 17.0);
    CHECK(err3[1] / err3[2] > 14.5);
    CHECK(err3[1] / err3[2] < 17.0);
    // Agreement at E = 256: three methods (determinant, FDTD, FEM) now
    // agree on these modes; FEM to about 1e-6 relative.
    CHECK(err1[2] / unwrap(modeFrequency(sys, 1)).value() < 5e-6);
  }
}

TEST_CASE("FEM handles free ends and rejects misuse", "[d17fem]") {
  const auto freeFree = unwrap(makeWaveSystem1D(
      DistanceMeters{1.3}, SpeedMetersPerSecond{2.0}, Boundary::Free,
      Boundary::Free));
  // The rigid translation (w = 0) is skipped; first elastic mode ~ c/2L.
  const auto f = unwrap(femStringFrequencies(freeFree, 80, 2));
  CHECK(nearlyEqual(f[0].value(), 2.0 / 2.6, 0.0, 1e-3));
  const auto stiff = unwrap(withStiffness(
      unwrap(makeWaveSystem1D(DistanceMeters{1.0}, SpeedMetersPerSecond{20.0},
                              Boundary::Fixed, Boundary::Fixed)),
      0.3));
  CHECK(femStringFrequencies(stiff, 40, 3).error() ==
        QuantityError::InvalidArgument);
  CHECK(femStiffFrequencies(freeFree, 40, 3).error() ==
        QuantityError::InvalidArgument);
  CHECK(femStringFrequencies(freeFree, 1, 1).error() ==
        QuantityError::InvalidArgument);
}
