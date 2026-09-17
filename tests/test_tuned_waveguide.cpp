// D14/D15 — fine tuning (FDTD pre-warping, tuned waveguide) and waveguide
// loss/dispersion (Catch2).
//
// Evidence per claim:
// - Pre-warped FDTD: the D12 prediction and an FFT measurement both put the
//   chosen mode exactly on the model pitch.
// - Loop pole solver: exact anchors where the answer is known in closed form
//   (F = z^-1: f_k = k fs/(2M+1); pure gain g: decay = -ln g fs/(2M+1)).
// - Tuned waveguide: FFT-measured partials equal the exact loop poles
//   (lossless designs, long windows) and measured decay equals pole decay
//   (lossy designs, short windows, as D12). The design's errors against the
//   CONTINUOUS model are asserted at the values its report states.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/fdtd1d.h>
#include <mmathen/physics/tuned_waveguide.h>
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
const SampleRateHz kRate{kFs};

// The D8 string shortened to 0.6523 m: no longer on an integer grid.
WaveSystem1D offGridString() {
  return unwrap(idealString(DistanceMeters{0.6523}, TensionNewtons{97.344},
                            LinearDensityKgPerM{0.001}));
}

LossModel illustrativeLoss(const WaveSystem1D& s) {
  return unwrap(lossFromT60(s.speed, FrequencyHz{240.0}, DurationSeconds{3.0},
                            FrequencyHz{2400.0}, DurationSeconds{0.4}));
}

std::vector<double> renderLoop(const WaveSystem1D& s, const LoopFilters& f,
                               std::size_t frames) {
  const auto shape = unwrap(InitialShape::pluck(s, DistanceMeters{0.1234}, 1e-3));
  auto wg = unwrap(TunedWaveguide::create(shape, f));
  const auto pickup = static_cast<std::size_t>(0.1234 * f.intervals);
  std::vector<double> x(frames);
  for (auto& v : x) {
    v = unwrap(wg.valueAt(pickup));
    wg.step();
  }
  return x;
}
}  // namespace

TEST_CASE("pre-warped FDTD puts the chosen mode exactly on pitch",
          "[d14tune]") {
  auto base = offGridString();
  auto stiff = unwrap(withStiffness(base, unwrap(stiffnessFromB(base, 4e-4))));
  auto lossy = unwrap(withLoss(stiff, LossModel{1.2, 0.003}));
  for (const auto& sys : {base, stiff, lossy}) {
    const double target = unwrap(modeFrequency(sys, 1)).value();
    for (const std::size_t m : {60u, 70u}) {
      const auto grid = unwrap(fdtdPrewarpForPitch(sys, m, kRate, 1));
      const auto predicted =
          unwrap(fdtdDiscreteMode(grid.system, m, grid.courant, 1));
      CHECK(nearlyEqual(predicted.frequency.value(), target, 0.0, 1e-12));
      const auto shape = unwrap(
          InitialShape::pluck(grid.system, DistanceMeters{0.1234}, 1e-3));
      auto fdtd = unwrap(Fdtd1D::create(shape, m, grid.courant));
      CHECK(nearlyEqual(fdtd.timeStep().value(), 1.0 / kFs, 0.0, 1e-12));
      std::vector<double> x(65536);
      for (auto& v : x) {
        v = fdtd.field()[m / 7];
        fdtd.step();
      }
      const auto est = unwrap(estimateSinusoid(
          x, kRate, Window::BlackmanHarris, FrequencyHz{target * 0.95},
          FrequencyHz{target * 1.05}));
      CHECK(std::fabs(est.frequency.value() - target) < 1e-5);  // <= 2.1e-7
    }
  }
  // No Courant in (0, 1] reaches the pitch on a too-fine grid.
  CHECK(fdtdPrewarpForPitch(base, 140, kRate, 1).error() ==
        QuantityError::DomainError);
  const auto clamped = unwrap(withStiffness(base, 0.5, StiffEnd::Clamped,
                                            StiffEnd::Clamped));
  CHECK(fdtdPrewarpForPitch(clamped, 60, kRate, 1).error() ==
        QuantityError::DomainError);
}

TEST_CASE("loop pole solver hits closed-form anchors", "[d14tune]") {
  // Thiran a = 0 is a pure one-sample delay: loop = 2M + 1 samples.
  LoopFilters pure;
  pure.intervals = 50;
  for (const int k : {1, 7, 30}) {
    const auto p = unwrap(loopPole(pure, kRate, k));
    CHECK(nearlyEqual(p.frequency.value(), k * kFs / 101.0, 0.0, 1e-12));
    CHECK(std::fabs(p.decayRate) < 1e-9);
  }
  // Pure loop gain g (b = 0): every pole has |z| = g^{1/(2M+1)}.
  LoopFilters gain = pure;
  gain.lossGain = 0.99;
  for (const int k : {1, 12}) {
    const auto p = unwrap(loopPole(gain, kRate, k));
    CHECK(nearlyEqual(p.decayRate, -std::log(0.99) * kFs / 101.0, 0.0, 1e-9));
    CHECK(nearlyEqual(p.frequency.value(), k * kFs / 101.0, 0.0, 1e-12));
  }
}

TEST_CASE("tuned ideal loop: exact fundamental, stated Thiran stretch",
          "[d14tune]") {
  const auto s = offGridString();
  const auto design = unwrap(designTunedLoop(s, kRate, 10, 0));
  REQUIRE(design.partials.size() == 10);
  CHECK(std::fabs(design.partials[0].centsError) < 1e-6);
  // First-order Thiran phase delay falls with frequency: partials go sharp,
  // monotonically. Reported worst 1.09 cents at partial 10.
  for (std::size_t i = 1; i < design.partials.size(); ++i)
    CHECK(design.partials[i].centsError > design.partials[i - 1].centsError);
  CHECK(design.worstCents < 1.2);
  // Rendered output matches the exact loop poles.
  const auto x = renderLoop(s, design.filters, 65536);
  for (const int k : {1, 3, 5, 7, 9}) {
    const double pole = design.partials[static_cast<std::size_t>(k - 1)]
                            .frequency.value();
    const auto est = unwrap(estimateSinusoid(
        x, kRate, Window::BlackmanHarris, FrequencyHz{pole - 60.0},
        FrequencyHz{pole + 60.0}));
    CHECK(std::fabs(est.frequency.value() - pole) < 5e-5);  // <= 9.6e-6
  }
}

TEST_CASE("lossy loop: design decay exact at two partials, measured = pole",
          "[d14tune]") {
  const auto base = offGridString();
  const auto s = unwrap(withLoss(base, illustrativeLoss(base)));
  const auto design = unwrap(designTunedLoop(s, kRate, 10, 0));
  CHECK(std::fabs(design.partials[0].centsError) < 1e-6);
  CHECK(std::fabs(design.partials[0].decayError) < 1e-5);
  CHECK(std::fabs(design.partials[9].decayError) < 1e-5);
  // One-pole shape vs sigma0 + sigma1 k^2 between the matched points:
  // reported worst 3.2%.
  CHECK(design.worstDecayError < 0.04);
  const auto x = renderLoop(s, design.filters, 3 * 4096);
  for (const int k : {1, 4}) {
    const auto& pole = design.partials[static_cast<std::size_t>(k - 1)];
    auto measure = [&](std::size_t start) {
      std::vector<double> w(x.begin() + static_cast<std::ptrdiff_t>(start),
                            x.begin() + static_cast<std::ptrdiff_t>(start + 4096));
      return unwrap(estimateSinusoid(
          w, kRate, Window::BlackmanHarris,
          FrequencyHz{pole.frequency.value() - 60.0},
          FrequencyHz{pole.frequency.value() + 60.0}));
    };
    const auto a = measure(0);
    const auto b = measure(8192);
    const double decay = std::log(a.amplitude / b.amplitude) / (8192.0 / kFs);
    CHECK(nearlyEqual(decay, pole.decayRate, 0.0, 1e-2));
  }
}

TEST_CASE("stiff loop: dispersion stages fit Fletcher partials",
          "[d14tune]") {
  const auto base = offGridString();
  const auto stiff =
      unwrap(withStiffness(base, unwrap(stiffnessFromB(base, 4e-4))));
  const auto k2 = unwrap(designTunedLoop(stiff, kRate, 10, 2));
  const auto k8 = unwrap(designTunedLoop(stiff, kRate, 10, 8));
  CHECK(std::fabs(k8.partials[0].centsError) < 1e-6);
  // More stages, better fit: reported 2.40 (K = 2) vs 1.07 cents (K = 8).
  CHECK(k8.worstCents < k2.worstCents);
  CHECK(k8.worstCents < 1.2);
  // Without the dispersion filter the stretch (+21.9 cents at partial 8,
  // D10) would be missed entirely; the fitted loop tracks it.
  const double model8 = unwrap(modeFrequency(stiff, 8)).value();
  CHECK(std::fabs(1200.0 * std::log2(k8.partials[7].frequency.value() /
                                     model8)) < 1.2);
  const auto x = renderLoop(stiff, k8.filters, 65536);
  for (const int k : {1, 4, 8}) {
    const double pole =
        k8.partials[static_cast<std::size_t>(k - 1)].frequency.value();
    const auto est = unwrap(estimateSinusoid(
        x, kRate, Window::BlackmanHarris, FrequencyHz{pole - 60.0},
        FrequencyHz{pole + 60.0}));
    CHECK(std::fabs(est.frequency.value() - pole) < 5e-5);
  }
}

TEST_CASE("tuned waveguide resets and validates", "[d14tune]") {
  const auto base = offGridString();
  const auto s = unwrap(withLoss(base, illustrativeLoss(base)));
  const auto design = unwrap(designTunedLoop(s, kRate, 6, 0));
  const auto shape = unwrap(InitialShape::pluck(s, DistanceMeters{0.1234}, 1e-3));
  auto wg = unwrap(TunedWaveguide::create(shape, design.filters));
  std::vector<double> first;
  for (int i = 0; i < 500; ++i) {
    first.push_back(unwrap(wg.valueAt(9)));
    wg.step();
  }
  wg.reset();
  bool same = true;
  for (int i = 0; i < 500; ++i) {
    same = same && unwrap(wg.valueAt(9)) == first[static_cast<std::size_t>(i)];
    wg.step();
  }
  CHECK(same);
  LoopFilters bad = design.filters;
  bad.lossGain = 1.5;
  CHECK(TunedWaveguide::create(shape, bad).error() ==
        QuantityError::InvalidArgument);
  CHECK(designTunedLoop(s, kRate, 1, 0).error() ==
        QuantityError::InvalidArgument);
  const auto stiff =
      unwrap(withStiffness(base, unwrap(stiffnessFromB(base, 4e-4))));
  CHECK(designTunedLoop(stiff, kRate, 10, 0).error() ==
        QuantityError::InvalidArgument);  // stiff needs dispersion stages
  const auto pipe = unwrap(stoppedPipe(DistanceMeters{0.7},
                                       SpeedMetersPerSecond{343.0}));
  CHECK(designTunedLoop(pipe, kRate, 6, 0).error() ==
        QuantityError::InvalidArgument);
  CHECK(loopPole(design.filters, kRate, 0).error() ==
        QuantityError::InvalidArgument);
}
