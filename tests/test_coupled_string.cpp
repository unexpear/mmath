// D17 — string coupled to a bridge oscillator (Catch2).
//
// Evidence: the transcendental mode equation recovers both limits exactly
// (k -> infinity: fixed-fixed; m = k = 0: fixed-free); FDTD's FFT-measured
// coupled frequencies converge to its roots at second order; the discrete
// energy (string + bridge) is conserved without damping and non-increasing
// with it; the Gershgorin stability condition rejects an over-stiff bridge.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/coupled_string.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

WaveSystem1D d8String() {
  return unwrap(idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                            LinearDensityKgPerM{0.001}));
}
const TensionNewtons kT{97.344};
}  // namespace

TEST_CASE("coupled mode equation recovers both end limits", "[d17couple]") {
  const auto str = d8String();
  const auto rigid = unwrap(
      makeCoupledString(str, kT, BridgeOscillator{0.0, 1e12, 0.0}));
  const auto freeEnd =
      unwrap(makeCoupledString(str, kT, BridgeOscillator{0.0, 0.0, 0.0}));
  const auto a = unwrap(coupledModeFrequencies(rigid, 3));
  const auto b = unwrap(coupledModeFrequencies(freeEnd, 3));
  for (int n = 1; n <= 3; ++n) {
    CHECK(nearlyEqual(a[static_cast<std::size_t>(n - 1)].value(), 240.0 * n,
                      0.0, 1e-6));
    CHECK(nearlyEqual(b[static_cast<std::size_t>(n - 1)].value(),
                      120.0 * (2 * n - 1), 0.0, 1e-12));
  }
}

TEST_CASE("a bridge resonance splits the string's modes", "[d17couple]") {
  // 5 g bridge on 2e4 N/m: alone it rings at 318.31 Hz.
  const auto cs = unwrap(makeCoupledString(d8String(), kT,
                                           BridgeOscillator{0.005, 2.0e4, 0.0}));
  const auto f = unwrap(coupledModeFrequencies(cs, 4));
  // Mode repulsion: 240 is pushed down, 480 up, and a bridge mode appears
  // between them near 318 Hz.
  CHECK(f[0].value() < 240.0);
  CHECK(f[1].value() > 318.31);
  CHECK(f[1].value() < 330.0);
  CHECK(f[2].value() > 480.0);
}

TEST_CASE("coupled FDTD converges to the transcendental roots",
          "[d17couple]") {
  const auto cs = unwrap(makeCoupledString(d8String(), kT,
                                           BridgeOscillator{0.005, 2.0e4, 0.0}));
  const auto roots = unwrap(coupledModeFrequencies(cs, 4));
  const auto shape =
      unwrap(InitialShape::pluck(d8String(), DistanceMeters{0.1234}, 1e-3));
  std::vector<std::vector<double>> errs(4);
  for (const std::size_t m : {40u, 80u, 160u}) {
    auto fdtd = unwrap(CoupledStringFdtd::create(cs, shape, m, 0.9));
    const double fs = 1.0 / fdtd.timeStep().value();
    std::size_t frames = 1;
    while (static_cast<double>(frames) < 3.0 * fs) frames <<= 1;
    std::vector<double> x(frames);
    for (auto& v : x) {
      v = fdtd.field()[m / 5];
      fdtd.step();
    }
    for (std::size_t n = 0; n < 4; ++n) {
      const double r = roots[n].value();
      const auto est = unwrap(estimateSinusoid(
          x, SampleRateHz{fs}, Window::BlackmanHarris, FrequencyHz{r * 0.97},
          FrequencyHz{r * 1.03}));
      errs[n].push_back(std::fabs(est.frequency.value() - r));
    }
  }
  for (const auto& e : errs) {
    CHECK(nearlyEqual(e[0] / e[1], 4.0, 0.0, 0.05));  // measured 4.00-4.01
    CHECK(nearlyEqual(e[1] / e[2], 4.0, 0.0, 0.05));
  }
}

TEST_CASE("coupled energy: conserved lossless, decaying with damping",
          "[d17couple]") {
  const auto shape =
      unwrap(InitialShape::pluck(d8String(), DistanceMeters{0.1234}, 1e-3));
  const auto lossless = unwrap(makeCoupledString(
      d8String(), kT, BridgeOscillator{0.005, 2.0e4, 0.0}));
  auto a = unwrap(CoupledStringFdtd::create(lossless, shape, 80, 0.9));
  a.step();
  const double e0 = a.energy();
  double drift = 0.0;
  for (int s = 0; s < 20000; ++s) {
    a.step();
    drift = (std::max)(drift, std::fabs(a.energy() - e0) / e0);
  }
  CHECK(drift < 1e-10);  // measured ~1e-13

  const auto damped = unwrap(makeCoupledString(
      d8String(), kT, BridgeOscillator{0.005, 2.0e4, 0.5}));
  auto b = unwrap(CoupledStringFdtd::create(damped, shape, 80, 0.9));
  b.step();
  double prev = b.energy();
  const double start = prev;
  bool monotone = true;
  for (int s = 0; s < 50000; ++s) {
    b.step();
    const double e = b.energy();
    monotone = monotone && e <= prev * (1.0 + 1e-12);
    prev = e;
  }
  CHECK(monotone);
  CHECK(prev < 0.5 * start);
}

TEST_CASE("coupled string validates and enforces stability", "[d17couple]") {
  const auto str = d8String();
  const auto shape =
      unwrap(InitialShape::pluck(str, DistanceMeters{0.1234}, 1e-3));
  const auto hard = unwrap(
      makeCoupledString(str, kT, BridgeOscillator{1e-4, 1e9, 0.0}));
  CHECK(CoupledStringFdtd::create(hard, shape, 80, 0.9).error() ==
        QuantityError::DomainError);
  CHECK(makeCoupledString(str, kT, BridgeOscillator{-1.0, 0.0, 0.0}).error() ==
        QuantityError::InvalidArgument);
  const auto stiff = unwrap(withStiffness(str, 0.5));
  CHECK(makeCoupledString(stiff, kT, BridgeOscillator{}).error() ==
        QuantityError::InvalidArgument);
  const auto cs =
      unwrap(makeCoupledString(str, kT, BridgeOscillator{0.005, 2e4, 0.0}));
  CHECK(CoupledStringFdtd::create(cs, shape, 80, 1.1).error() ==
        QuantityError::DomainError);
}
