// D1 — stiff spectra and sensory dissonance tests (Catch2).
//
// Anchors are scratch-verified literals ( Essentia-coefficient evaluation
// reproduced independently); ordinal claims carry libm-safe margins, and
// exact zeros exercise the documented clamp paths.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include <mmathen/analysis/consonance.h>
#include <mmathen/math/comparison.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/inharmonic.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/sink/sink.h>
#include <mmathen/time/factories.h>
#include <mmathen/time/function.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

Partial partial(double f, double level) {
  return unwrap(
      makePartial(FrequencyHz{f}, AmplitudeLinear{level}));
}

std::vector<Partial> harmonicSpectrum(double f0, int n) {
  std::vector<Partial> out;
  for (int k = 1; k <= n; ++k) out.push_back(partial(f0 * k, 1.0 / k));
  return out;
}

std::vector<Partial> stiffSpectrum(double f0, double B, int n) {
  std::vector<Partial> out;
  for (int k = 1; k <= n; ++k)
    out.push_back(partial(k * f0 * std::sqrt(1.0 + B * k * k), 1.0 / k));
  return out;
}

double dyad(const std::vector<Partial>& a, const std::vector<Partial>& b) {
  return unwrap(dyadDissonance(a, b)).value();
}
}  // namespace

TEST_CASE("stiff partials follow Fletcher's formula", "[d1]") {
  const auto spec =
      unwrap(makeStiffSpectrum(FrequencyHz{100.0}, 0.001, 8));
  CHECK(nearlyEqual(unwrap(partialFrequency(spec, 1)).value(),
                    100.0499875062461, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(partialFrequency(spec, 2)).value(),
                    200.3996007980056, 1e-9, 1e-12));
  CHECK(partialFrequency(spec, 0).error() == QuantityError::InvalidArgument);
  CHECK(partialFrequency(spec, 9).error() == QuantityError::InvalidArgument);
  CHECK(makeStiffSpectrum(FrequencyHz{100.0}, -0.001, 8).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeStiffSpectrum(FrequencyHz{0.0}, 0.001, 8).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeStiffSpectrum(FrequencyHz{100.0}, 0.001, 0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("B estimator inverts synthetic spectra", "[d1]") {
  // Synthesize f1/f4 from B = 2e-4 at 220 Hz, re-estimate.
  const double B = 2e-4;
  const double f1 = 220.0 * std::sqrt(1.0 + B);
  const double f4 = 880.0 * std::sqrt(1.0 + B * 16.0);
  CHECK(nearlyEqual(unwrap(estimateB(FrequencyHz{f1}, 1, FrequencyHz{f4}, 4)),
                    B, 0.0, 1e-9));
  CHECK(estimateB(FrequencyHz{f1}, 2, FrequencyHz{f4}, 2).error() ==
        QuantityError::InvalidArgument);  // identical partials
  CHECK(estimateB(FrequencyHz{f1}, 0, FrequencyHz{f4}, 4).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("audibility threshold follows Smith's approximation", "[d1]") {
  CHECK(nearlyEqual(unwrap(audibilityThresholdB(FrequencyHz{440.0})),
                    0.0001073310908892135, 1e-12, 1e-9));
  CHECK(audibilityThresholdB(FrequencyHz{0.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("critical bandwidth matches the Bark formulation", "[d1]") {
  CHECK(nearlyEqual(unwrap(criticalBandwidth(FrequencyHz{440.0})).value(),
                    109.6725644559101, 1e-9, 1e-12));
  CHECK(criticalBandwidth(FrequencyHz{0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makePartial(FrequencyHz{0.0}, AmplitudeLinear{1.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makePartial(FrequencyHz{440.0}, AmplitudeLinear{-1.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("pairwise dissonance rises, peaks, then clamps to zero", "[d1]") {
  const Partial a = partial(440.0, 1.0);
  CHECK(unwrap(pairDissonance(a, a)).value() == 0.0);  // unison clamp path
  const double dNear =
      unwrap(pairDissonance(a, partial(445.0, 1.0))).value();
  const double dPeak =
      unwrap(pairDissonance(a, partial(467.4, 1.0))).value();
  CHECK(dNear < dPeak);  // rising flank (~1/4 CBW peak)
  // Far field: beyond the 1.18-CBW window, consonance clamps to 1.
  CHECK(unwrap(pairDissonance(a, partial(600.0, 1.0))).value() == 0.0);
  CHECK(unwrap(pairDissonance(a, partial(880.0, 1.0))).value() == 0.0);
  CHECK(dyadDissonance({}, {a}).error() == QuantityError::InvalidArgument);
}

TEST_CASE("harmonic fifth is the deep minimum", "[d1]") {
  const auto A = harmonicSpectrum(220.0, 6);
  const double dFifth = dyad(A, harmonicSpectrum(330.0, 6));
  CHECK(dFifth < dyad(A, harmonicSpectrum(220.0 * 4.0 / 3.0, 6)));
  CHECK(dyad(A, harmonicSpectrum(220.0 * 4.0 / 3.0, 6)) <
        dyad(A, harmonicSpectrum(220.0 * 1.4142135623730951, 6)));
  CHECK(dFifth < dyad(A, harmonicSpectrum(275.0, 6)));  // major third 5/4
}

TEST_CASE("stiffness moves the minimum sharp: the Railsback mechanism",
          "[d1]") {
  // Same 3/2 ratio is rougher on stiff strings...
  const auto hA = harmonicSpectrum(220.0, 6);
  const auto sA = stiffSpectrum(220.0, 2e-4, 6);
  CHECK(dyad(sA, stiffSpectrum(330.0, 2e-4, 6)) >
        dyad(hA, harmonicSpectrum(330.0, 6)));
  // ...and the local minimum sits sharp of 3/2 on the fine grid.
  CHECK(dyad(sA, stiffSpectrum(220.0 * 1.501, 2e-4, 6)) <
        dyad(sA, stiffSpectrum(220.0 * 1.500, 2e-4, 6)));
  CHECK(dyad(sA, stiffSpectrum(220.0 * 1.501, 2e-4, 6)) <
        dyad(sA, stiffSpectrum(220.0 * 1.502, 2e-4, 6)));
}

TEST_CASE("stiff additive note renders clean", "[d1]") {
  const auto spec =
      unwrap(makeStiffSpectrum(FrequencyHz{220.0}, 2e-4, 6));
  // Six voices share the 0.25 peak budget, as in the progression demo.
  const auto sig = unwrap(stiffAdditive(
      spec, constant(AmplitudeLinear{0.25 / 6.0})));
  MemorySink mem;
  const auto stats = unwrap(renderOffline(
      sig,
      RenderConfig{SampleRateHz{48000.0}, FrameCount{24000},
                   TimePointSeconds{0.0}, 512},
      mem));
  CHECK(stats.framesRendered == 24000);
  CHECK(stats.isClean());
}
