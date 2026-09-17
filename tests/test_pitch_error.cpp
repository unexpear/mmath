// D12 — FDTD pitch and decay error at audio rates (Catch2).
//
// fdtdDiscreteMode's closed form is checked three independent ways:
// 1. eigenvector claim: a single-mode run stays proportional to phi_n at
//    every node (the ratio u_j / phi_n(x_j) is the same for all j);
// 2. frequency: the D11 estimator measures the rendered mode at the
//    predicted DISCRETE frequency, and visibly not at the continuous one;
// 3. decay: amplitude measured over two equal windows gives the predicted
//    numerical decay rate.
// Grids are real audio-rate grids (dt = 1/48000) of the D8 string.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

constexpr double kFs = 48000.0;

// D8 string: L = 0.65 m, c = 312 m/s. M intervals, Courant for dt = 1/fs.
WaveSystem1D d8String() {
  return unwrap(idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                            LinearDensityKgPerM{0.001}));
}

double courantAtRate(const WaveSystem1D& s, std::size_t m) {
  return s.speed.value() / kFs * static_cast<double>(m) / s.length.value();
}

// Render node `node` of a single-mode run for `frames` steps.
std::vector<double> renderMode(const WaveSystem1D& s, std::size_t m, int n,
                               std::size_t node, std::size_t frames) {
  const auto shape = unwrap(InitialShape::mode(s, n, 1e-3));
  auto fdtd = unwrap(Fdtd1D::create(shape, m, courantAtRate(s, m)));
  std::vector<double> out(frames);
  for (std::size_t i = 0; i < frames; ++i) {
    out[i] = fdtd.field()[node];
    fdtd.step();
  }
  return out;
}
}  // namespace

TEST_CASE("ideal string at Courant 1 has zero pitch error", "[d12pitch]") {
  const auto s = d8String();
  for (int n = 1; n <= 20; ++n) {
    const double cents = unwrap(fdtdPitchErrorCents(s, 100, 1.0, n));
    CHECK(std::fabs(cents) < 1e-9);
  }
}

TEST_CASE("single-mode runs stay on their grid eigenvector", "[d12pitch]") {
  auto stiff = d8String();
  stiff = unwrap(withStiffness(stiff, unwrap(stiffnessFromB(stiff, 4e-4))));
  stiff = unwrap(withLoss(stiff, LossModel{1.5, 0.004}));
  const auto freeEnds = unwrap(makeWaveSystem1D(
      DistanceMeters{0.65}, SpeedMetersPerSecond{312.0}, Boundary::Free,
      Boundary::Fixed));
  for (const auto& sys : {stiff, freeEnds}) {
    const std::size_t m = 60;
    const auto shape = unwrap(InitialShape::mode(sys, 3, 1e-3));
    auto fdtd = unwrap(Fdtd1D::create(shape, m, courantAtRate(sys, m)));
    for (int i = 0; i < 777; ++i) fdtd.step();
    // Reference node with a large mode value, then every other node.
    const double dx = fdtd.gridSpacing().value();
    std::size_t ref = 0;
    for (std::size_t j = 0; j <= m; ++j)
      if (std::fabs(shape.valueAt(static_cast<double>(j) * dx)) >
          std::fabs(shape.valueAt(static_cast<double>(ref) * dx)))
        ref = j;
    const double ratio =
        fdtd.field()[ref] / shape.valueAt(static_cast<double>(ref) * dx);
    double worst = 0.0;
    for (std::size_t j = 0; j <= m; ++j) {
      const double phi =
          shape.valueAt(j == m ? 0.65 : static_cast<double>(j) * dx);
      worst = (std::max)(worst, std::fabs(fdtd.field()[j] - ratio * phi));
    }
    CHECK(worst < 1e-14);
  }
}

TEST_CASE("FFT measures the discrete, not the continuous, frequency",
          "[d12pitch]") {
  // Courant 0.7 grid (M = 70): noticeable dispersion for higher modes.
  const auto s = d8String();
  const std::size_t m = 70;
  const double courant = courantAtRate(s, m);
  CHECK(nearlyEqual(courant, 0.7, 0.0, 1e-12));
  for (const int n : {1, 5, 12}) {
    const auto x = renderMode(s, m, n, 7, 65536);
    const double predicted =
        unwrap(fdtdDiscreteMode(s, m, courant, n)).frequency.value();
    const double continuous = unwrap(modeFrequency(s, n)).value();
    const auto est = unwrap(estimateSinusoid(
        x, SampleRateHz{kFs}, Window::BlackmanHarris,
        FrequencyHz{predicted * 0.97}, FrequencyHz{predicted * 1.03}));
    const double measured = est.frequency.value();
    CHECK(std::fabs(measured - predicted) < 1e-6);  // measured <= 7e-8 Hz
    // The grid is genuinely flat: for mode 12 the gap to the continuous
    // frequency is far larger than the measurement error.
    if (n == 12) {
      CHECK(std::fabs(measured - continuous) > 1.0);
      CHECK(unwrap(fdtdPitchErrorCents(s, m, courant, n)) < -1.0);
    }
  }
}

TEST_CASE("stiff audio-rate grid: predicted stretch error is measured",
          "[d12pitch]") {
  auto s = d8String();
  s = unwrap(withStiffness(s, unwrap(stiffnessFromB(s, 4e-4))));
  const auto grid = unwrap(stableGridForRate(s, SampleRateHz{kFs}));
  REQUIRE(grid.intervals == 73);
  for (const int n : {1, 4, 9}) {
    const auto x = renderMode(s, grid.intervals, n, 11, 65536);
    const double predicted =
        unwrap(fdtdDiscreteMode(s, grid.intervals, grid.courant, n))
            .frequency.value();
    const auto est = unwrap(estimateSinusoid(
        x, SampleRateHz{kFs}, Window::BlackmanHarris,
        FrequencyHz{predicted * 0.97}, FrequencyHz{predicted * 1.03}));
    CHECK(std::fabs(est.frequency.value() - predicted) < 1e-6);
  }
}

TEST_CASE("lossy grid: measured decay equals predicted numerical decay",
          "[d12pitch]") {
  auto s = d8String();
  s = unwrap(withLoss(s, LossModel{2.0, 0.006}));
  const std::size_t m = 70;
  const double courant = courantAtRate(s, m);
  for (const int n : {2, 9}) {
    const auto mode = unwrap(fdtdDiscreteMode(s, m, courant, n));
    const auto x = renderMode(s, m, n, 13, 3 * 4096);
    // Two equal windows 8192 frames apart: the window acts identically on
    // both, so the amplitude ratio is exactly e^{-sigma * gap}. Windows are
    // short (4096) because fast modes decay many dB within a long window and
    // broaden past the estimator's main-lobe check (mode 9 at 16384 was
    // correctly refused).
    std::vector<double> w1(x.begin(), x.begin() + 4096);
    std::vector<double> w2(x.begin() + 8192, x.begin() + 8192 + 4096);
    auto band = [&](const std::vector<double>& w) {
      return unwrap(estimateSinusoid(
          w, SampleRateHz{kFs}, Window::BlackmanHarris,
          FrequencyHz{mode.frequency.value() * 0.97},
          FrequencyHz{mode.frequency.value() * 1.03}));
    };
    const double a1 = band(w1).amplitude;
    const double a2 = band(w2).amplitude;
    const double measured = std::log(a1 / a2) / (8192.0 / kFs);
    CHECK(nearlyEqual(measured, mode.decayRate, 0.0, 1e-4));
    // And the continuous model's rate is close but not identical.
    const double model = unwrap(modeDecayRate(s, n));
    CHECK(nearlyEqual(mode.decayRate, model, 0.0, 0.05));
  }
}

TEST_CASE("discrete mode queries reject invalid input", "[d12pitch]") {
  const auto s = d8String();
  CHECK(fdtdDiscreteMode(s, 100, 1.0, 0).error() ==
        QuantityError::InvalidArgument);
  CHECK(fdtdDiscreteMode(s, 100, 1.0, 100).error() ==
        QuantityError::InvalidArgument);  // Fixed-Fixed: n <= M-1
  const auto freeFree = unwrap(makeWaveSystem1D(
      DistanceMeters{0.65}, SpeedMetersPerSecond{312.0}, Boundary::Free,
      Boundary::Free));
  CHECK(fdtdDiscreteMode(freeFree, 100, 0.9, 100).ok());
  CHECK(fdtdDiscreteMode(s, 100, 1.01, 3).error() ==
        QuantityError::DomainError);
  CHECK(fdtdDiscreteMode(s, 1, 0.5, 1).error() ==
        QuantityError::InvalidArgument);
}
