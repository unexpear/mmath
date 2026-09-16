// D10 — hinged bending stiffness on the 1-D wave system (Catch2).
//
// The anchor is cross-domain: a hinged stiff string's modes must equal D1's
// Fletcher partials f_n = n f0 sqrt(1 + B n^2) computed by the independent
// signal/inharmonic.h. FDTD is checked by second-order convergence to the
// exact stiff modal solution (refined at fixed mu so the scheme stays
// stable), by its conserved discrete energy including the stiffness term
// (written here), by energy decay with loss, and by the derived stability
// limit lam^2 + 4 mu^2 <= 1 at its boundary.

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
#include <mmathen/signal/inharmonic.h>
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
constexpr double kKappa = 0.05;

WaveSystem1D stiffString(double kappa, double sigma0 = 0.0,
                         double sigma1 = 0.0) {
  auto sys = unwrap(makeWaveSystem1D(DistanceMeters{kL},
                                     SpeedMetersPerSecond{kC},
                                     Boundary::Fixed, Boundary::Fixed));
  sys = unwrap(withStiffness(sys, kappa));
  return unwrap(withLoss(sys, LossModel{sigma0, sigma1}));
}

// Discrete energy with the stiffness term; hinged ends give D u = 0 there.
double energy(const Fdtd1D& fdtd, double kappa) {
  const double dt = fdtd.timeStep().value();
  const double dx = fdtd.gridSpacing().value();
  const auto& u1 = fdtd.field();
  const auto& u0 = fdtd.previousField();
  const std::size_t m = u1.size() - 1;
  double kinetic = 0.0;
  double tension = 0.0;
  double bending = 0.0;
  for (std::size_t j = 0; j <= m; ++j) {
    const double v = (u1[j] - u0[j]) / dt;
    kinetic += v * v;
  }
  for (std::size_t j = 0; j < m; ++j)
    tension += (u1[j + 1] - u1[j]) * (u0[j + 1] - u0[j]);
  for (std::size_t j = 1; j < m; ++j)
    bending += (u1[j + 1] - 2.0 * u1[j] + u1[j - 1]) *
               (u0[j + 1] - 2.0 * u0[j] + u0[j - 1]);
  return 0.5 * kinetic + 0.5 * kC * kC / (dx * dx) * tension +
         0.5 * kappa * kappa / (dx * dx * dx * dx) * bending;
}

// Courant number that puts mu = kappa*dt/dx^2 at the given value.
double courantForMu(double mu, std::size_t m, double kappa) {
  const double dx = kL / static_cast<double>(m);
  return kC * mu * dx / kappa;
}
}  // namespace

TEST_CASE("hinged stiff string modes are D1's Fletcher partials",
          "[d10stiff]") {
  // D8 string, c = 312 m/s, L = 0.65 m, f0 = 240 Hz; B = 4e-4.
  auto base = unwrap(idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                                 LinearDensityKgPerM{0.001}));
  const double b = 4e-4;
  const double kappa = unwrap(stiffnessFromB(base, b));
  const auto stiff = unwrap(withStiffness(base, kappa));
  CHECK(nearlyEqual(inharmonicityB(stiff), b, 0.0, 1e-12));
  const auto spectrum = unwrap(makeStiffSpectrum(FrequencyHz{240.0}, b, 20));
  for (int n = 1; n <= 20; ++n)
    CHECK(nearlyEqual(unwrap(modeFrequency(stiff, n)).value(),
                      unwrap(partialFrequency(spectrum, n)).value(), 0.0,
                      1e-12));
  // Partials are stretched sharp, increasingly with n.
  CHECK(unwrap(modeFrequency(stiff, 10)).value() > 2400.0 * 1.01);
  // No stiffness: exactly the ideal series, and B = 0.
  CHECK(inharmonicityB(base) == 0.0);
  CHECK(unwrap(modeFrequency(base, 7)).value() ==
        unwrap(modeFrequency(unwrap(withStiffness(base, 0.0)), 7)).value());
}

TEST_CASE("stiffness is limited to hinged Fixed-Fixed systems",
          "[d10stiff]") {
  const SpeedMetersPerSecond c{343.0};
  const auto stopped = unwrap(stoppedPipe(DistanceMeters{0.7}, c));
  const auto freeFree = unwrap(makeWaveSystem1D(
      DistanceMeters{1.0}, c, Boundary::Free, Boundary::Free));
  CHECK(withStiffness(stopped, 0.1).error() == QuantityError::DomainError);
  CHECK(withStiffness(freeFree, 0.1).error() == QuantityError::DomainError);
  CHECK(withStiffness(stopped, 0.0).ok());  // zero stiffness is no stiffness
  const auto s = stiffString(0.0);
  CHECK(withStiffness(s, -1.0).error() == QuantityError::InvalidArgument);
  CHECK(withStiffness(s, std::numeric_limits<double>::quiet_NaN()).error() ==
        QuantityError::InvalidArgument);
  CHECK(stiffnessFromB(s, -1e-4).error() == QuantityError::InvalidArgument);
  // Ideal-only rungs refuse a stiff object.
  const auto shape = unwrap(InitialShape::pluck(stiffString(kKappa),
                                                DistanceMeters{0.37}, 0.01));
  CHECK(analyticField(shape, DistanceMeters{0.5}, TimePointSeconds{0.1})
            .error() == QuantityError::DomainError);
  CHECK(Waveguide1D::create(shape, 64).error() == QuantityError::DomainError);
}

TEST_CASE("stiff modal solution follows the stiff dispersion relation",
          "[d10stiff]") {
  const auto sys = stiffString(kKappa);
  const auto shape = unwrap(InitialShape::mode(sys, 4, 0.02));
  const auto modal = unwrap(ModalSolution::create(shape, 4));
  const double k = 4.0 * std::numbers::pi / kL;
  const double omega = std::sqrt(kC * kC * k * k + kKappa * kKappa * k * k * k * k);
  for (const double t : {0.0, 0.037, 0.61, 3.3})
    for (const double x : {0.1, 0.5, 1.2})
      CHECK(nearlyEqual(
          unwrap(modal.at(DistanceMeters{x}, TimePointSeconds{t})),
          0.02 * std::sin(k * x) * std::cos(omega * t), 1e-12, 0.0));
}

TEST_CASE("stiff FDTD converges at second order", "[d10stiff]") {
  // Refine at fixed mu = 0.25 (dt ~ dx^2): lam shrinks with dx, the stability
  // condition lam^2 + 4 mu^2 <= 1 holds on every grid, and the error is
  // dominated by the O(dx^2) spatial term. Equal end times: steps x4 per
  // level since dt / 4.
  const auto sys = stiffString(kKappa);
  const auto shape = unwrap(InitialShape::mode(sys, 3, 0.02));
  const auto exact = unwrap(ModalSolution::create(shape, 3));
  std::vector<double> errors;
  std::size_t steps = 100;
  for (const std::size_t m : {40u, 80u, 160u}) {
    auto fdtd = unwrap(Fdtd1D::create(shape, m, courantForMu(0.25, m, kKappa)));
    for (std::size_t n = 0; n < steps; ++n) fdtd.step();
    const double t = fdtd.time().value();
    double worst = 0.0;
    for (std::size_t j = 0; j <= m; ++j) {
      const double x = j == m ? kL : static_cast<double>(j) * kL /
                                         static_cast<double>(m);
      worst = (std::max)(worst,
                         std::fabs(fdtd.field()[j] -
                                   unwrap(exact.at(DistanceMeters{x},
                                                   TimePointSeconds{t}))));
    }
    errors.push_back(worst);
    steps *= 4;
  }
  REQUIRE(errors[1] > 0.0);
  const double r1 = errors[0] / errors[1];
  const double r2 = errors[1] / errors[2];
  CHECK(r1 > 3.5);
  CHECK(r1 < 4.5);
  CHECK(r2 > 3.5);
  CHECK(r2 < 4.5);
}

TEST_CASE("stiff FDTD energy: conserved lossless, decaying lossy",
          "[d10stiff]") {
  const std::size_t m = 100;
  // lam^2 + 4 mu^2 = 0.9 at the chosen mu.
  const double mu = 0.4;
  const double lam = courantForMu(mu, m, kKappa);
  REQUIRE(lam * lam + 4.0 * mu * mu <= 1.0);

  const auto ideal = unwrap(
      InitialShape::pluck(stiffString(kKappa), DistanceMeters{0.37}, 0.01));
  auto fdtd = unwrap(Fdtd1D::create(ideal, m, lam));
  fdtd.step();
  const double e0 = energy(fdtd, kKappa);
  REQUIRE(e0 > 0.0);
  double drift = 0.0;
  for (int n = 0; n < 5000; ++n) {
    fdtd.step();
    drift = (std::max)(drift, std::fabs(energy(fdtd, kKappa) - e0) / e0);
  }
  CHECK(drift < 1e-10);

  const auto lossy = unwrap(InitialShape::pluck(
      stiffString(kKappa, 0.3, 0.02), DistanceMeters{0.37}, 0.01));
  auto damped = unwrap(Fdtd1D::create(lossy, m, lam));
  damped.step();
  double previous = energy(damped, kKappa);
  const double start = previous;
  bool monotone = true;
  for (int n = 0; n < 5000; ++n) {
    damped.step();
    const double e = energy(damped, kKappa);
    monotone = monotone && e <= previous * (1.0 + 1e-12);
    previous = e;
  }
  CHECK(monotone);
  CHECK(previous < 0.5 * start);
}

TEST_CASE("stiff lossy FDTD agrees with the stiff damped modal solution",
          "[d10stiff]") {
  const auto sys = stiffString(kKappa, 0.4, 0.01);
  const auto shape = unwrap(InitialShape::mode(sys, 2, 0.02));
  const auto exact = unwrap(ModalSolution::create(shape, 2));
  const std::size_t m = 200;
  auto fdtd = unwrap(Fdtd1D::create(shape, m, courantForMu(0.25, m, kKappa)));
  for (int n = 0; n < 4000; ++n) fdtd.step();
  const double t = fdtd.time().value();
  double worst = 0.0;
  for (std::size_t j = 0; j <= m; j += 5) {
    const double x = j == m ? kL : static_cast<double>(j) * kL /
                                       static_cast<double>(m);
    worst = (std::max)(worst, std::fabs(fdtd.field()[j] -
                                        unwrap(exact.at(DistanceMeters{x},
                                                        TimePointSeconds{t}))));
  }
  CHECK(worst < 1e-5);
}

TEST_CASE("stiff stability limit is enforced at its boundary", "[d10stiff]") {
  const auto shape = unwrap(
      InitialShape::pluck(stiffString(kKappa), DistanceMeters{0.37}, 0.01));
  const std::size_t m = 100;
  // For mu = 0.4 the limit is lam = sqrt(1 - 0.64) = 0.6; mu itself scales
  // with lam at fixed dx, so solve lam^2 + 4 (a lam)^2 = 1 with
  // a = kappa/(c dx): lam* = 1/sqrt(1 + 4 a^2).
  const double dx = kL / static_cast<double>(m);
  const double a = kKappa / (kC * dx);
  const double limit = 1.0 / std::sqrt(1.0 + 4.0 * a * a);
  CHECK(Fdtd1D::create(shape, m, limit * (1.0 - 1e-12)).ok());
  CHECK(Fdtd1D::create(shape, m, limit * (1.0 + 1e-9)).error() ==
        QuantityError::DomainError);
  // Without stiffness the same Courant number is fine.
  const auto plain = unwrap(
      InitialShape::pluck(stiffString(0.0), DistanceMeters{0.37}, 0.01));
  CHECK(Fdtd1D::create(plain, m, limit * (1.0 + 1e-9)).ok());
}

TEST_CASE("stable audio-rate grid is the finest stable one", "[d10stiff]") {
  auto base = unwrap(idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                                 LinearDensityKgPerM{0.001}));
  const auto stiff =
      unwrap(withStiffness(base, unwrap(stiffnessFromB(base, 4e-4))));
  const SampleRateHz fs{48000.0};
  const auto grid = unwrap(stableGridForRate(stiff, fs));
  const auto shape =
      unwrap(InitialShape::pluck(stiff, DistanceMeters{0.13}, 0.002));
  CHECK(Fdtd1D::create(shape, grid.intervals, grid.courant).ok());
  // One more interval at the same dt = 1/fs would be unstable.
  const double m1 = static_cast<double>(grid.intervals + 1);
  const double courant1 = 312.0 / 48000.0 * m1 / 0.65;
  const auto finer = Fdtd1D::create(shape, grid.intervals + 1, courant1);
  CHECK((courant1 > 1.0 || finer.error() == QuantityError::DomainError));
  // The time step is one audio frame.
  CHECK(nearlyEqual(Fdtd1D::create(shape, grid.intervals, grid.courant)
                        .value()
                        .timeStep()
                        .value(),
                    1.0 / 48000.0, 0.0, 1e-12));
  // No stiffness: the exact lam = 1 grid of D8 (M = 100).
  const auto ideal = unwrap(stableGridForRate(base, fs));
  CHECK(ideal.intervals == 100);
  CHECK(stableGridForRate(base, SampleRateHz{0.0}).error() ==
        QuantityError::InvalidArgument);
}
