// D8 — 1-D wave system and its solvers: analytical, modal, FDTD, waveguide.
//
// Evidence strategy: every rung is checked against a different path.
// Mode frequencies against D2's independent formulas; pluck coefficients
// against composite Simpson quadrature written here; d'Alembert against
// separation of variables (single modes) and its own periodicity; modal
// against d'Alembert within the stated truncation bound; FDTD at Courant 1
// and the waveguide against d'Alembert at grid points (exact) and each
// other; FDTD below Courant 1 by its convergence order and its conserved
// discrete energy.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/physics/fdtd1d.h>
#include <mmathen/physics/modes.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/physics/waveguide1d.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

constexpr double kL = 1.3;
constexpr double kC = 2.0;

WaveSystem1D waveSystem(Boundary left, Boundary right) {
  return unwrap(makeWaveSystem1D(DistanceMeters{kL}, SpeedMetersPerSecond{kC},
                                 left, right));
}

std::vector<WaveSystem1D> allSystems() {
  return {waveSystem(Boundary::Fixed, Boundary::Fixed),
          waveSystem(Boundary::Free, Boundary::Free),
          waveSystem(Boundary::Fixed, Boundary::Free),
          waveSystem(Boundary::Free, Boundary::Fixed)};
}

InitialShape pluckOf(const WaveSystem1D& s) {
  return unwrap(InitialShape::pluck(s, DistanceMeters{0.37}, 0.01));
}

double nodeX(std::size_t j, std::size_t m) {
  return j == m ? kL : static_cast<double>(j) * kL / static_cast<double>(m);
}

double analyticAt(const InitialShape& shape, double x, double t) {
  return unwrap(analyticField(shape, DistanceMeters{x}, TimePointSeconds{t}));
}

// Composite Simpson on [a, b], n even intervals.
template <typename F>
double simpson(F f, double a, double b, int n) {
  const double h = (b - a) / n;
  double s = f(a) + f(b);
  for (int i = 1; i < n; ++i) s += (i % 2 == 1 ? 4.0 : 2.0) * f(a + i * h);
  return s * h / 3.0;
}
}  // namespace

TEST_CASE("wave system modes match the D2 formulas", "[d8wave]") {
  const auto str = unwrap(idealString(DistanceMeters{0.65},
                                      TensionNewtons{97.344},
                                      LinearDensityKgPerM{0.001}));
  CHECK(nearlyEqual(str.speed.value(), 312.0, 0.0, 1e-14));
  const SpeedMetersPerSecond c{343.2368};
  const auto open = unwrap(openPipe(DistanceMeters{0.7}, c));
  const auto stopped = unwrap(stoppedPipe(DistanceMeters{0.7}, c));
  CHECK(stopped.left == Boundary::Free);
  CHECK(stopped.right == Boundary::Fixed);
  for (int n = 1; n <= 6; ++n) {
    CHECK(nearlyEqual(
        unwrap(modeFrequency(str, n)).value(),
        unwrap(stringMode(DistanceMeters{0.65}, TensionNewtons{97.344},
                          LinearDensityKgPerM{0.001}, n))
            .value(),
        0.0, 1e-12));
    CHECK(nearlyEqual(unwrap(modeFrequency(open, n)).value(),
                      unwrap(openPipeMode(DistanceMeters{0.7}, c, n)).value(),
                      0.0, 1e-12));
    // Stopped-pipe mode n is the (2n-1)-th odd harmonic.
    CHECK(nearlyEqual(
        unwrap(modeFrequency(stopped, n)).value(),
        unwrap(stoppedPipeMode(DistanceMeters{0.7}, c, 2 * n - 1)).value(),
        0.0, 1e-12));
  }
}

TEST_CASE("closed-form pluck coefficients match quadrature", "[d8wave]") {
  for (const auto& s : allSystems()) {
    const auto shape = pluckOf(s);
    const bool sinShape = s.left == Boundary::Fixed;
    const bool mixed = s.left != s.right;
    for (std::size_t n = 1; n <= 10; ++n) {
      const double nn = static_cast<double>(n);
      const double k = mixed ? (2.0 * nn - 1.0) * std::numbers::pi / (2.0 * kL)
                             : nn * std::numbers::pi / kL;
      auto integrand = [&](double x) {
        const double f = x <= 0.37 ? 0.01 * x / 0.37
                                   : 0.01 * (kL - x) / (kL - 0.37);
        return f * (sinShape ? std::sin(k * x) : std::cos(k * x));
      };
      const double numeric = 2.0 / kL * (simpson(integrand, 0.0, 0.37, 4000) +
                                         simpson(integrand, 0.37, kL, 4000));
      CHECK(nearlyEqual(shape.modalCoefficient(n), numeric, 1e-12, 1e-9));
    }
  }
  const auto freeFree = pluckOf(waveSystem(Boundary::Free, Boundary::Free));
  CHECK(freeFree.constantTerm() == 0.005);
}

TEST_CASE("d'Alembert solves single modes by separation of variables",
          "[d8wave]") {
  for (const auto& s : allSystems()) {
    const auto shape = unwrap(InitialShape::mode(s, 3, 0.02));
    const double omega =
        std::numbers::pi * 2.0 * unwrap(modeFrequency(s, 3)).value();
    for (const double t : {0.0, 0.113, 0.77, 2.9})
      for (const double x : {0.0, 0.21, 0.65, 1.1, kL}) {
        const double phi = s.left == Boundary::Fixed
                               ? std::sin(omega / kC * x)
                               : std::cos(omega / kC * x);
        CHECK(nearlyEqual(analyticAt(shape, x, t),
                          0.02 * phi * std::cos(omega * t), 1e-12, 0.0));
      }
  }
}

TEST_CASE("d'Alembert periods and reflections", "[d8wave]") {
  const double roundTrip = kL / kC;  // time to cross the system once
  const auto ff = pluckOf(waveSystem(Boundary::Fixed, Boundary::Fixed));
  const auto freeFree = pluckOf(waveSystem(Boundary::Free, Boundary::Free));
  const auto mixed = pluckOf(waveSystem(Boundary::Fixed, Boundary::Free));
  for (const double x : {0.0, 0.2, 0.37, 0.9, kL}) {
    CHECK(nearlyEqual(analyticAt(ff, x, 0.0), ff.valueAt(x), 1e-15, 0.0));
    // Same-type ends: period 2L/c; at L/c the shape is mirrored, negated
    // for Fixed ends and not for Free ends.
    CHECK(nearlyEqual(analyticAt(ff, x, 2.0 * roundTrip), ff.valueAt(x),
                      1e-12, 0.0));
    CHECK(nearlyEqual(analyticAt(ff, x, roundTrip), -ff.valueAt(kL - x),
                      1e-12, 0.0));
    CHECK(nearlyEqual(analyticAt(freeFree, x, roundTrip),
                      freeFree.valueAt(kL - x), 1e-12, 0.0));
    // Mixed ends: negated at 2L/c, period 4L/c (odd harmonics only).
    CHECK(nearlyEqual(analyticAt(mixed, x, 2.0 * roundTrip),
                      -mixed.valueAt(x), 1e-12, 0.0));
    CHECK(nearlyEqual(analyticAt(mixed, x, 4.0 * roundTrip),
                      mixed.valueAt(x), 1e-12, 0.0));
  }
}

TEST_CASE("modal solution converges within its truncation bound",
          "[d8wave]") {
  for (const auto& s : allSystems()) {
    const auto shape = pluckOf(s);
    double previousWorst = std::numeric_limits<double>::infinity();
    for (const std::size_t terms : {10u, 100u, 1000u}) {
      const auto modal = unwrap(ModalSolution::create(shape, terms));
      double worst = 0.0;
      for (const double t : {0.0, 0.113, 0.77, 2.9})
        for (const double x : {0.05, 0.37, 0.8, 1.25}) {
          const double diff = std::fabs(
              unwrap(modal.at(DistanceMeters{x}, TimePointSeconds{t})) -
              analyticAt(shape, x, t));
          worst = (std::max)(worst, diff);
        }
      CHECK(worst <= modal.truncationBound() + 1e-15);
      CHECK(worst < previousWorst);
      previousWorst = worst;
    }
  }
  // A single-mode shape is represented exactly once its mode is included.
  const auto s = waveSystem(Boundary::Fixed, Boundary::Fixed);
  const auto m3 = unwrap(InitialShape::mode(s, 3, 0.02));
  CHECK(m3.truncationBound(2) == 0.02);
  CHECK(unwrap(ModalSolution::create(m3, 3)).truncationBound() == 0.0);
}

TEST_CASE("FDTD at Courant 1 and the waveguide are exact at grid points",
          "[d8wave]") {
  const std::size_t m = 64;
  for (const auto& s : allSystems()) {
    const auto shape = pluckOf(s);
    auto fdtd = unwrap(Fdtd1D::create(shape, m, 1.0));
    auto wg = unwrap(Waveguide1D::create(shape, m));
    CHECK(fdtd.timeStep() == wg.timeStep());
    double worst = 0.0;
    double worstPair = 0.0;
    // 1000 steps: several round trips, both reflections exercised.
    for (int n = 0; n < 1000; ++n) {
      fdtd.step();
      wg.step();
      const double t = fdtd.time().value();
      for (std::size_t j = 0; j <= m; j += 3) {
        const double exact = analyticAt(shape, nodeX(j, m), t);
        const double a = unwrap(fdtd.valueAt(j));
        const double b = unwrap(wg.valueAt(j));
        worst = (std::max)(worst, (std::max)(std::fabs(a - exact),
                                             std::fabs(b - exact)));
        worstPair = (std::max)(worstPair, std::fabs(a - b));
      }
    }
    CHECK(worst <= 1e-14);
    CHECK(worstPair <= 1e-14);
  }
}

TEST_CASE("FDTD below Courant 1 converges at second order", "[d8wave]") {
  const auto s = waveSystem(Boundary::Fixed, Boundary::Fixed);
  const auto shape = unwrap(InitialShape::mode(s, 3, 0.02));
  const double omega =
      2.0 * std::numbers::pi * unwrap(modeFrequency(s, 3)).value();
  std::vector<double> errors;
  for (const std::size_t m : {40u, 80u, 160u}) {
    auto fdtd = unwrap(Fdtd1D::create(shape, m, 0.5));
    // M steps at Courant 0.5 reach t = L/(2c) for every M, where
    // omega*t = 1.5*pi: cos crosses zero with full slope, so the O(dx^2)
    // phase error shows at first order. (At t = L/c, omega*t = 3*pi sits on
    // a cos extremum and the error appears squared — measured ratio 16,
    // not 4 — a blind spot this time choice avoids.)
    for (std::size_t n = 0; n < m; ++n) fdtd.step();
    const double t = fdtd.time().value();
    double worst = 0.0;
    for (std::size_t j = 0; j <= m; ++j) {
      const double x = nodeX(j, m);
      const double exact =
          0.02 * std::sin(omega / kC * x) * std::cos(omega * t);
      worst = (std::max)(worst, std::fabs(fdtd.field()[j] - exact));
    }
    errors.push_back(worst);
  }
  REQUIRE(errors[1] > 0.0);
  const double r1 = errors[0] / errors[1];
  const double r2 = errors[1] / errors[2];
  CHECK(r1 > 3.5);
  CHECK(r1 < 4.5);
  CHECK(r2 > 3.5);
  CHECK(r2 < 4.5);
}

TEST_CASE("FDTD conserves its discrete energy", "[d8wave]") {
  const auto s = waveSystem(Boundary::Fixed, Boundary::Fixed);
  auto fdtd = unwrap(Fdtd1D::create(pluckOf(s), 100, 0.7));
  const double dt = fdtd.timeStep().value();
  const double dx = fdtd.gridSpacing().value();
  auto energy = [&]() {
    const auto& u1 = fdtd.field();
    const auto& u0 = fdtd.previousField();
    double kinetic = 0.0;
    double potential = 0.0;
    for (std::size_t j = 0; j < u1.size(); ++j) {
      const double v = (u1[j] - u0[j]) / dt;
      kinetic += v * v;
    }
    for (std::size_t j = 0; j + 1 < u1.size(); ++j)
      potential += (u1[j + 1] - u1[j]) * (u0[j + 1] - u0[j]);
    return 0.5 * kinetic + 0.5 * kC * kC / (dx * dx) * potential;
  };
  fdtd.step();
  const double e0 = energy();
  REQUIRE(e0 > 0.0);
  double worstDrift = 0.0;
  for (int n = 0; n < 5000; ++n) {
    fdtd.step();
    worstDrift = (std::max)(worstDrift, std::fabs(energy() - e0) / e0);
  }
  CHECK(worstDrift < 1e-10);
}

TEST_CASE("stopped pipe is antiperiodic; string is not", "[d8wave]") {
  // Odd harmonics only <=> u(t + 2L/c) = -u(t).
  const std::size_t m = 50;
  const auto pipe = pluckOf(waveSystem(Boundary::Free, Boundary::Fixed));
  auto fdtd = unwrap(Fdtd1D::create(pipe, m, 1.0));
  const std::vector<double> start = fdtd.field();
  for (std::size_t n = 0; n < 2 * m; ++n) fdtd.step();
  double worst = 0.0;
  for (std::size_t j = 0; j <= m; ++j)
    worst = (std::max)(worst, std::fabs(fdtd.field()[j] + start[j]));
  CHECK(worst <= 1e-14);

  const auto str = pluckOf(waveSystem(Boundary::Fixed, Boundary::Fixed));
  auto sfd = unwrap(Fdtd1D::create(str, m, 1.0));
  const std::vector<double> sStart = sfd.field();
  for (std::size_t n = 0; n < m; ++n) sfd.step();  // half period of a string
  double largest = 0.0;
  for (std::size_t j = 0; j <= m; ++j)
    largest = (std::max)(largest, std::fabs(sfd.field()[j] + sStart[j]));
  CHECK(largest > 1e-3);  // even harmonics present: not a negation
}

TEST_CASE("solvers reset bit-identically", "[d8wave]") {
  const auto shape = pluckOf(waveSystem(Boundary::Fixed, Boundary::Free));
  auto fdtd = unwrap(Fdtd1D::create(shape, 64, 0.8));
  auto wg = unwrap(Waveguide1D::create(shape, 64));
  std::vector<double> fdFirst;
  std::vector<double> wgFirst;
  for (int n = 0; n < 300; ++n) {
    fdtd.step();
    wg.step();
    fdFirst.push_back(fdtd.field()[17]);
    wgFirst.push_back(unwrap(wg.valueAt(17)));
  }
  fdtd.reset();
  wg.reset();
  CHECK(fdtd.steps() == 0);
  CHECK(wg.steps() == 0);
  for (int n = 0; n < 300; ++n) {
    fdtd.step();
    wg.step();
    CHECK(fdtd.field()[17] == fdFirst[static_cast<std::size_t>(n)]);
    CHECK(unwrap(wg.valueAt(17)) == wgFirst[static_cast<std::size_t>(n)]);
  }
}

TEST_CASE("audio-rate grids refuse to retune silently", "[d8wave]") {
  const auto str = unwrap(idealString(DistanceMeters{0.65},
                                      TensionNewtons{97.344},
                                      LinearDensityKgPerM{0.001}));
  CHECK(unwrap(gridIntervalsForRate(str, SampleRateHz{48000.0})) == 100);
  const auto off = unwrap(idealString(DistanceMeters{0.651},
                                      TensionNewtons{97.344},
                                      LinearDensityKgPerM{0.001}));
  CHECK(gridIntervalsForRate(off, SampleRateHz{48000.0}).error() ==
        QuantityError::DomainError);
  CHECK(gridIntervalsForRate(str, SampleRateHz{0.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("wave system and solvers reject invalid input", "[d8wave]") {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const auto s = waveSystem(Boundary::Fixed, Boundary::Fixed);
  CHECK(makeWaveSystem1D(DistanceMeters{0.0}, SpeedMetersPerSecond{kC},
                         Boundary::Fixed, Boundary::Fixed)
            .error() == QuantityError::InvalidArgument);
  CHECK(idealString(DistanceMeters{1.0}, TensionNewtons{-1.0},
                    LinearDensityKgPerM{0.001})
            .error() == QuantityError::DomainError);
  CHECK(InitialShape::pluck(s, DistanceMeters{0.0}, 0.01).error() ==
        QuantityError::InvalidArgument);
  CHECK(InitialShape::pluck(s, DistanceMeters{kL}, 0.01).error() ==
        QuantityError::InvalidArgument);
  CHECK(InitialShape::pluck(s, DistanceMeters{0.5}, nan).error() ==
        QuantityError::InvalidArgument);
  CHECK(InitialShape::mode(s, 0, 1.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(modeFrequency(s, 0).error() == QuantityError::InvalidArgument);
  const auto shape = pluckOf(s);
  CHECK(analyticField(shape, DistanceMeters{kL + 0.1}, TimePointSeconds{0.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(ModalSolution::create(shape, 0).error() ==
        QuantityError::InvalidArgument);
  // CFL: Courant > 1 is unstable and never run.
  CHECK(Fdtd1D::create(shape, 64, std::nextafter(1.0, 2.0)).error() ==
        QuantityError::DomainError);
  CHECK(Fdtd1D::create(shape, 64, 0.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(Fdtd1D::create(shape, 1, 1.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(Waveguide1D::create(shape, 1).error() ==
        QuantityError::InvalidArgument);
  const auto fdtd = unwrap(Fdtd1D::create(shape, 8, 1.0));
  CHECK(fdtd.valueAt(9).error() == QuantityError::InvalidArgument);
}
