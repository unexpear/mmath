// D17 — 2-D membrane and 3-D room on a box FDTD (Catch2).
//
// Evidence: an FFT-measured mode frequency equals the exact discrete
// prediction; the discrete prediction converges to Rayleigh's continuous
// frequency at second order (ratio 4 per halving); a single-mode run stays
// on its grid eigenvector; discrete energy is conserved; the room's axial
// modes equal D2's pipe series; CFL 1/sqrt(D) is enforced.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/fdtd_box.h>
#include <mmathen/physics/modes.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

using Mode = std::array<int, 3>;

BoxGrid membrane(std::size_t scale) {
  // 0.6 m x 0.4 m, c = 100 m/s.
  return unwrap(makeBoxGrid(2, {30 * scale, 20 * scale, 0},
                            DistanceMeters{0.02 / static_cast<double>(scale)},
                            SpeedMetersPerSecond{100.0}, BoxWalls::Dirichlet));
}

BoxGrid room() {
  // 1.2 m x 0.9 m x 0.6 m, c = 343 m/s, rigid walls.
  return unwrap(makeBoxGrid(3, {24, 18, 12}, DistanceMeters{0.05},
                            SpeedMetersPerSecond{343.0}, BoxWalls::Neumann));
}
}  // namespace

TEST_CASE("membrane: FFT measures the exact discrete mode frequency",
          "[d17box]") {
  const auto g = membrane(1);
  const double lam = 0.6;
  const auto init =
      unwrap(boxBumpInitial(g, {0.23, 0.17, 0.0}, DistanceMeters{0.05}, 1e-3));
  auto fdtd = unwrap(FdtdBox::create(g, init, lam));
  const double fs = 1.0 / fdtd.timeStep().value();
  std::vector<double> x(32768);
  for (auto& v : x) {
    v = unwrap(fdtd.valueAt(9, 7));
    fdtd.step();
  }
  for (const Mode m : {Mode{1, 1, 0}, Mode{2, 1, 0}, Mode{1, 2, 0}}) {
    const double disc = unwrap(boxDiscreteModeFrequency(g, lam, m)).value();
    const auto est = unwrap(estimateSinusoid(
        x, SampleRateHz{fs}, Window::BlackmanHarris, FrequencyHz{disc * 0.97},
        FrequencyHz{disc * 1.03}));
    CHECK(std::fabs(est.frequency.value() - disc) < 1e-5);  // <= 1e-7 seen
  }
  // Rayleigh anchor: f_11 = (c/2) sqrt(1/0.6^2 + 1/0.4^2).
  CHECK(nearlyEqual(unwrap(boxModeFrequency(g, Mode{1, 1, 0})).value(),
                    50.0 * std::sqrt(1.0 / 0.36 + 1.0 / 0.16), 0.0, 1e-14));
}

TEST_CASE("membrane discretization converges at second order", "[d17box]") {
  const double lam = 0.6;
  for (const Mode m : {Mode{1, 1, 0}, Mode{2, 1, 0}, Mode{1, 2, 0}}) {
    std::vector<double> err;
    for (const std::size_t scale : {1u, 2u, 4u}) {
      const auto g = membrane(scale);
      err.push_back(unwrap(boxModeFrequency(g, m)).value() -
                    unwrap(boxDiscreteModeFrequency(g, lam, m)).value());
    }
    CHECK(err[0] > 0.0);  // grid is flat
    CHECK(nearlyEqual(err[0] / err[1], 4.0, 0.0, 0.01));
    CHECK(nearlyEqual(err[1] / err[2], 4.0, 0.0, 0.01));
  }
}

TEST_CASE("membrane single mode stays on its eigenvector; energy conserved",
          "[d17box]") {
  const auto g = membrane(1);
  const double lam = 0.6;
  const auto init = unwrap(boxModeInitial(g, Mode{2, 3, 0}, 1e-3));
  auto fdtd = unwrap(FdtdBox::create(g, init, lam));
  const double h = g.spacing.value();
  auto phi = [&](std::size_t i, std::size_t j) {
    return std::sin(2.0 * 3.141592653589793 * static_cast<double>(i) * h / 0.6) *
           std::sin(3.0 * 3.141592653589793 * static_cast<double>(j) * h / 0.4);
  };
  for (int s = 0; s < 555; ++s) fdtd.step();
  const double ratio = unwrap(fdtd.valueAt(4, 3)) / phi(4, 3);
  double worst = 0.0;
  for (std::size_t j = 0; j <= 20; ++j)
    for (std::size_t i = 0; i <= 30; ++i)
      worst = (std::max)(worst,
                         std::fabs(unwrap(fdtd.valueAt(i, j)) - ratio * phi(i, j)));
  CHECK(worst < 1e-15);

  // Energy of a bump on the Dirichlet membrane.
  const auto bump =
      unwrap(boxBumpInitial(g, {0.23, 0.17, 0.0}, DistanceMeters{0.07}, 1e-3));
  auto run = unwrap(FdtdBox::create(g, bump, 0.7));
  const double dt = run.timeStep().value();
  const double c2h2 = 100.0 * 100.0 / (h * h);
  auto energy = [&]() {
    const auto& u1 = run.field();
    const auto& u0 = run.previousField();
    double e = 0.0;
    for (std::size_t id = 0; id < u1.size(); ++id) {
      const double v = (u1[id] - u0[id]) / dt;
      e += 0.5 * v * v;
    }
    for (std::size_t j = 0; j <= 20; ++j)
      for (std::size_t i = 0; i <= 30; ++i) {
        const std::size_t id = run.index(i, j, 0);
        if (i < 30) {
          const std::size_t r = run.index(i + 1, j, 0);
          e += 0.5 * c2h2 * (u1[r] - u1[id]) * (u0[r] - u0[id]);
        }
        if (j < 20) {
          const std::size_t u = run.index(i, j + 1, 0);
          e += 0.5 * c2h2 * (u1[u] - u1[id]) * (u0[u] - u0[id]);
        }
      }
    return e;
  };
  run.step();
  const double e0 = energy();
  double drift = 0.0;
  for (int s = 0; s < 3000; ++s) {
    run.step();
    drift = (std::max)(drift, std::fabs(energy() - e0) / e0);
  }
  CHECK(drift < 1e-10);
}

TEST_CASE("room: modes measured, axial series equals the pipe series",
          "[d17box]") {
  const auto g = room();
  const double lam = 0.5;
  const auto init = unwrap(
      boxBumpInitial(g, {0.31, 0.22, 0.17}, DistanceMeters{0.15}, 1.0));
  auto fdtd = unwrap(FdtdBox::create(g, init, lam));
  const double fs = 1.0 / fdtd.timeStep().value();
  std::vector<double> x(32768);
  for (auto& v : x) {
    v = unwrap(fdtd.valueAt(2, 1, 1));
    fdtd.step();
  }
  for (const Mode m : {Mode{1, 0, 0}, Mode{0, 1, 0}, Mode{1, 1, 0},
                       Mode{1, 1, 1}}) {
    const double disc = unwrap(boxDiscreteModeFrequency(g, lam, m)).value();
    const auto est = unwrap(estimateSinusoid(
        x, SampleRateHz{fs}, Window::BlackmanHarris, FrequencyHz{disc * 0.97},
        FrequencyHz{disc * 1.03}));
    CHECK(std::fabs(est.frequency.value() - disc) < 1e-5);
  }
  // Rigid-wall axial modes of a 1.2 m room = D2's n c/2L series.
  for (int n = 1; n <= 3; ++n)
    CHECK(nearlyEqual(
        unwrap(boxModeFrequency(g, Mode{n, 0, 0})).value(),
        unwrap(openPipeMode(DistanceMeters{1.2}, SpeedMetersPerSecond{343.0}, n))
            .value(),
        0.0, 1e-12));
}

TEST_CASE("box FDTD enforces CFL and validates", "[d17box]") {
  const auto g2 = membrane(1);
  const auto g3 = room();
  const auto i2 = unwrap(boxModeInitial(g2, Mode{1, 1, 0}, 1.0));
  const auto i3 = unwrap(boxModeInitial(g3, Mode{1, 0, 0}, 1.0));
  // 1/sqrt(2) = 0.7071..., 1/sqrt(3) = 0.5773...
  CHECK(FdtdBox::create(g2, i2, 0.7071).ok());
  CHECK(FdtdBox::create(g2, i2, 0.7072).error() == QuantityError::DomainError);
  CHECK(FdtdBox::create(g3, i3, 0.5773).ok());
  CHECK(FdtdBox::create(g3, i3, 0.5774).error() == QuantityError::DomainError);
  CHECK(boxModeInitial(g2, Mode{0, 1, 0}, 1.0).error() ==
        QuantityError::InvalidArgument);  // Dirichlet needs m >= 1
  CHECK(boxModeFrequency(g3, Mode{0, 0, 0}).error() ==
        QuantityError::InvalidArgument);  // the constant is not a mode
  CHECK(makeBoxGrid(4, {10, 10, 10}, DistanceMeters{0.1},
                    SpeedMetersPerSecond{1.0}, BoxWalls::Neumann)
            .error() == QuantityError::InvalidArgument);
}
