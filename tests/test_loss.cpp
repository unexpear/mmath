// D9 — frequency-dependent loss on the 1-D wave system (Catch2).
//
// References are independent of the code under test: an RK4 integrator
// written here for each mode's damped oscillator (all three regimes), the
// exact damped modal solution for FDTD convergence, and the discrete energy
// balance, which with loss must be non-increasing. T60 fitting is checked by
// round trip through modeT60 and by ordering across modes.

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

WaveSystem1D lossySystem(double length, double speed, Boundary left,
                         Boundary right, double sigma0, double sigma1) {
  auto sys = unwrap(makeWaveSystem1D(DistanceMeters{length},
                                     SpeedMetersPerSecond{speed}, left, right));
  return unwrap(withLoss(sys, LossModel{sigma0, sigma1}));
}

// RK4 for q'' + 2*sigma*q' + w^2 q = 0, q(0) = 1, q'(0) = 0.
double rk4Damped(double omega, double sigma, double tEnd, int steps) {
  const double h = tEnd / steps;
  double q = 1.0;
  double v = 0.0;
  auto acc = [&](double qq, double vv) {
    return -2.0 * sigma * vv - omega * omega * qq;
  };
  for (int i = 0; i < steps; ++i) {
    const double k1q = v;
    const double k1v = acc(q, v);
    const double k2q = v + 0.5 * h * k1v;
    const double k2v = acc(q + 0.5 * h * k1q, v + 0.5 * h * k1v);
    const double k3q = v + 0.5 * h * k2v;
    const double k3v = acc(q + 0.5 * h * k2q, v + 0.5 * h * k2v);
    const double k4q = v + h * k3v;
    const double k4v = acc(q + h * k3q, v + h * k3v);
    q += h / 6.0 * (k1q + 2.0 * k2q + 2.0 * k3q + k4q);
    v += h / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
  }
  return q;
}

double energy(const Fdtd1D& fdtd, double c) {
  const double dt = fdtd.timeStep().value();
  const double dx = fdtd.gridSpacing().value();
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
  return 0.5 * kinetic + 0.5 * c * c / (dx * dx) * potential;
}
}  // namespace

TEST_CASE("two-point T60 fit round-trips through the modes", "[d9loss]") {
  // String: c = 312 m/s, L = 0.65 m -> mode 1 = 240 Hz, mode 5 = 1200 Hz.
  const SpeedMetersPerSecond c{312.0};
  const auto loss = unwrap(lossFromT60(c, FrequencyHz{240.0},
                                       DurationSeconds{4.0},
                                       FrequencyHz{1200.0},
                                       DurationSeconds{0.8}));
  CHECK(loss.sigma0 > 0.0);
  CHECK(loss.sigma1 > 0.0);
  auto base = unwrap(makeWaveSystem1D(DistanceMeters{0.65}, c, Boundary::Fixed,
                                      Boundary::Fixed));
  const auto sys = unwrap(withLoss(base, loss));
  CHECK(nearlyEqual(unwrap(modeT60(sys, 1)).value(), 4.0, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(modeT60(sys, 5)).value(), 0.8, 0.0, 1e-12));
  // T60 = 3 ln10 / sigma: -60 dB is 10^-3 in amplitude.
  const double s1 = unwrap(modeDecayRate(sys, 1));
  CHECK(nearlyEqual(std::exp(-s1 * 4.0), 1e-3, 0.0, 1e-12));
  // Higher modes decay strictly faster.
  double previous = std::numeric_limits<double>::infinity();
  for (int n = 1; n <= 12; ++n) {
    const double t60 = unwrap(modeT60(sys, n)).value();
    CHECK(t60 < previous);
    previous = t60;
  }
  // Light damping barely shifts pitch: mode 1 within 1e-5 relative.
  CHECK(nearlyEqual(unwrap(modeFrequency(sys, 1)).value(), 240.0, 0.0, 1e-5));
  CHECK(unwrap(modeFrequency(sys, 1)).value() < 240.0);
}

TEST_CASE("unrepresentable decay data is reported, not clamped",
          "[d9loss]") {
  const SpeedMetersPerSecond c{312.0};
  // Treble ringing longer than bass would need sigma1 < 0.
  CHECK(lossFromT60(c, FrequencyHz{240.0}, DurationSeconds{1.0},
                    FrequencyHz{1200.0}, DurationSeconds{3.0})
            .error() == QuantityError::DomainError);
  CHECK(lossFromT60(c, FrequencyHz{240.0}, DurationSeconds{1.0},
                    FrequencyHz{240.0}, DurationSeconds{0.5})
            .error() == QuantityError::InvalidArgument);
  CHECK(lossFromT60(c, FrequencyHz{240.0}, DurationSeconds{0.0},
                    FrequencyHz{1200.0}, DurationSeconds{0.5})
            .error() == QuantityError::InvalidArgument);
  auto base = unwrap(makeWaveSystem1D(DistanceMeters{0.65}, c, Boundary::Fixed,
                                      Boundary::Fixed));
  CHECK(withLoss(base, LossModel{-0.1, 0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(withLoss(base, LossModel{0.0, std::numeric_limits<double>::quiet_NaN()})
            .error() == QuantityError::InvalidArgument);
  // A lossless mode never decays: no finite T60 to report.
  CHECK(modeT60(base, 1).error() == QuantityError::DomainError);
}

TEST_CASE("damped modal factor matches RK4 in all three regimes",
          "[d9loss]") {
  // L = 1, c = 1: mode 1 has w = pi exactly. sigma0 picks the regime.
  const double pi = std::numbers::pi;
  for (const double sigma : {1.0, pi, 5.0}) {
    const auto sys = lossySystem(1.0, 1.0, Boundary::Fixed, Boundary::Fixed,
                                 sigma, 0.0);
    const auto shape = unwrap(InitialShape::mode(sys, 1, 0.3));
    const auto modal = unwrap(ModalSolution::create(shape, 1));
    for (const double t : {0.1, 0.7, 2.0}) {
      const double got =
          unwrap(modal.at(DistanceMeters{0.5}, TimePointSeconds{t}));
      CHECK(nearlyEqual(got, 0.3 * rk4Damped(pi, sigma, t, 20000), 1e-10,
                        0.0));
    }
    if (sigma < pi) {
      CHECK(nearlyEqual(unwrap(modeFrequency(sys, 1)).value(),
                        std::sqrt(pi * pi - sigma * sigma) / (2.0 * pi), 0.0,
                        1e-14));
    } else {
      // Critically damped and overdamped modes do not oscillate.
      CHECK(modeFrequency(sys, 1).error() == QuantityError::DomainError);
    }
  }
}

TEST_CASE("lossy FDTD converges to the damped modal solution at second order",
          "[d9loss]") {
  const auto sys = lossySystem(1.3, 2.0, Boundary::Fixed, Boundary::Fixed,
                               0.8, 0.01);
  const auto shape = unwrap(InitialShape::mode(sys, 3, 0.02));
  const auto exact = unwrap(ModalSolution::create(shape, 3));
  std::vector<double> errors;
  for (const std::size_t m : {40u, 80u, 160u}) {
    auto fdtd = unwrap(Fdtd1D::create(shape, m, 0.5));
    for (std::size_t n = 0; n < m; ++n) fdtd.step();  // t = L/(2c) for all M
    const double t = fdtd.time().value();
    double worst = 0.0;
    for (std::size_t j = 0; j <= m; ++j) {
      const double x = j == m ? 1.3 : static_cast<double>(j) * 1.3 / static_cast<double>(m);
      const double ref =
          unwrap(exact.at(DistanceMeters{x}, TimePointSeconds{t}));
      worst = (std::max)(worst, std::fabs(fdtd.field()[j] - ref));
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

TEST_CASE("lossy FDTD energy never increases, even at Courant 1",
          "[d9loss]") {
  for (const double courant : {1.0, 0.7}) {
    // sigma1 large enough that the explicit variant would be unstable at
    // Courant 1 (it needs lam^2 + 4*sigma1*dt/dx^2 <= 1); the implicit
    // scheme only needs lam <= 1.
    const auto sys = lossySystem(1.3, 2.0, Boundary::Fixed, Boundary::Fixed,
                                 0.5, 0.05);
    const auto shape = unwrap(InitialShape::pluck(sys, DistanceMeters{0.37},
                                                  0.01));
    auto fdtd = unwrap(Fdtd1D::create(shape, 100, courant));
    const double dt = fdtd.timeStep().value();
    const double dx = fdtd.gridSpacing().value();
    CHECK(courant * courant + 4.0 * 0.05 * dt / (dx * dx) > 1.0);
    fdtd.step();
    const double e0 = energy(fdtd, 2.0);
    double previous = e0;
    bool monotone = true;
    for (int n = 0; n < 5000; ++n) {
      fdtd.step();
      const double e = energy(fdtd, 2.0);
      monotone = monotone && e <= previous * (1.0 + 1e-12);
      previous = e;
    }
    CHECK(monotone);
    CHECK(previous < 0.5 * e0);
    CHECK(std::isfinite(fdtd.field()[37]));
  }
}

TEST_CASE("lossy solvers reset bit-identically", "[d9loss]") {
  const auto sys = lossySystem(1.3, 2.0, Boundary::Free, Boundary::Fixed,
                               0.3, 0.02);
  const auto shape =
      unwrap(InitialShape::pluck(sys, DistanceMeters{0.37}, 0.01));
  auto fdtd = unwrap(Fdtd1D::create(shape, 64, 0.9));
  std::vector<double> first;
  for (int n = 0; n < 300; ++n) {
    fdtd.step();
    first.push_back(fdtd.field()[0]);
  }
  fdtd.reset();
  bool identical = true;
  for (int n = 0; n < 300; ++n) {
    fdtd.step();
    identical = identical && fdtd.field()[0] == first[static_cast<std::size_t>(n)];
  }
  CHECK(identical);
}

TEST_CASE("free-end lossy FDTD agrees with the damped modal solution",
          "[d9loss]") {
  // Mixed and Free-Free systems exercise the ghost rows of the Thomas solve.
  for (const auto& [left, right] :
       std::vector<std::pair<Boundary, Boundary>>{
           {Boundary::Free, Boundary::Free},
           {Boundary::Free, Boundary::Fixed},
           {Boundary::Fixed, Boundary::Free}}) {
    const auto sys = lossySystem(1.3, 2.0, left, right, 0.4, 0.01);
    const auto shape = unwrap(InitialShape::mode(sys, 2, 0.02));
    const auto exact = unwrap(ModalSolution::create(shape, 2));
    auto fdtd = unwrap(Fdtd1D::create(shape, 400, 0.5));
    for (int n = 0; n < 400; ++n) fdtd.step();
    const double t = fdtd.time().value();
    double worst = 0.0;
    for (std::size_t j = 0; j <= 400; j += 10) {
      const double x = j == 400 ? 1.3 : static_cast<double>(j) * 1.3 / 400.0;
      worst = (std::max)(
          worst, std::fabs(fdtd.field()[j] -
                           unwrap(exact.at(DistanceMeters{x},
                                           TimePointSeconds{t}))));
    }
    CHECK(worst < 1e-6);  // measured 3e-9 .. 1.2e-7 (0.02 amplitude)
  }
}

TEST_CASE("lossless-only rungs reject lossy systems explicitly",
          "[d9loss]") {
  const auto sys = lossySystem(1.3, 2.0, Boundary::Fixed, Boundary::Fixed,
                               0.5, 0.0);
  const auto shape =
      unwrap(InitialShape::pluck(sys, DistanceMeters{0.37}, 0.01));
  CHECK(analyticField(shape, DistanceMeters{0.5}, TimePointSeconds{0.1})
            .error() == QuantityError::DomainError);
  CHECK(Waveguide1D::create(shape, 64).error() == QuantityError::DomainError);
  CHECK(ModalSolution::create(shape, 10).ok());
  CHECK(Fdtd1D::create(shape, 64, 1.0).ok());
}
