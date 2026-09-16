// D6 — wavetable oscillator tests (Catch2).
//
// Interpolator anchors use a 4-point table {0, 1, 0, -1} at binary-exact
// phases, so expectations are CHECK-exact (Lagrange midpoint weights are
// -1/16, 9/16, 9/16, -1/16). Error bounds are pinned from BOTH sides: the
// measured worst case never exceeds the bound, and for a single sine it
// reaches at least 90% of it (the bound is attained at interval midpoints).
// Oscillator expectations come from independent paths: additive() and
// sineSweep(), not the table under test.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/harmonics.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/phase.h>
#include <mmathen/signal/signal.h>
#include <mmathen/signal/wavetable.h>
#include <mmathen/sink/sink.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

using Interp = WavetableInterpolation;

HarmonicSeries pureSine() {
  return unwrap(HarmonicSeries::create(
      {Harmonic{FrequencyRatio{1.0}, AmplitudeLinear{1.0}}}));
}

// 1/n series, K partials: a bandlimited saw recipe.
HarmonicSeries inverseSeries(int partials) {
  std::vector<Harmonic> hs;
  for (int k = 1; k <= partials; ++k)
    hs.push_back(Harmonic{FrequencyRatio{static_cast<double>(k)},
                          AmplitudeLinear{1.0 / k}});
  return unwrap(HarmonicSeries::create(std::move(hs)));
}

// Worst |lookup - exact| over a dense grid offset from the table nodes.
double worstError(const Wavetable& table, Interp interp, int partials,
                  bool inverse) {
  double worst = 0.0;
  const int steps = 20011;  // prime: never aligns with power-of-two nodes
  for (int i = 0; i < steps; ++i) {
    const double p = static_cast<double>(i) / steps;
    double exact = 0.0;
    for (int k = 1; k <= partials; ++k)
      exact += (inverse ? 1.0 / k : 1.0) *
               std::sin(2.0 * std::numbers::pi * k * p);
    const double got = unwrap(wavetableValue(table, p, interp));
    worst = (std::max)(worst, std::fabs(got - exact));
  }
  return worst;
}
}  // namespace

TEST_CASE("interpolators hit binary-exact anchors, wrap included",
          "[d6wt]") {
  const auto t = unwrap(Wavetable::fromSamples({0.0, 1.0, 0.0, -1.0}));
  for (const Interp in : {Interp::Linear, Interp::Cubic}) {
    CHECK(unwrap(wavetableValue(t, 0.0, in)) == 0.0);
    CHECK(unwrap(wavetableValue(t, 0.25, in)) == 1.0);
    CHECK(unwrap(wavetableValue(t, 0.5, in)) == 0.0);
    CHECK(unwrap(wavetableValue(t, 0.75, in)) == -1.0);
  }
  CHECK(unwrap(wavetableValue(t, 0.125, Interp::Linear)) == 0.5);
  CHECK(unwrap(wavetableValue(t, 0.875, Interp::Linear)) == -0.5);
  // Cubic midpoint: -1/16*y[-1] + 9/16*y0 + 9/16*y1 - 1/16*y2.
  CHECK(unwrap(wavetableValue(t, 0.125, Interp::Cubic)) == 0.625);
  // Wrap: nodes 2,3,0,1 -> -1/16*0 + 9/16*(-1) + 9/16*0 - 1/16*1.
  CHECK(unwrap(wavetableValue(t, 0.875, Interp::Cubic)) == -0.625);
  CHECK(t.formula() == "table[N=4, samples]");
  CHECK(t.maxHarmonic() == 1);
  CHECK_FALSE(t.spectrumKnown());
}

TEST_CASE("harmonic tables sample the hand-written sum", "[d6wt]") {
  const auto t = unwrap(Wavetable::fromHarmonics(inverseSeries(3), 64));
  REQUIRE(t.size() == 64);
  for (const int j : {0, 1, 7, 16, 33, 63}) {
    const double p = j / 64.0;
    const double expected = std::sin(2.0 * std::numbers::pi * p) +
                            std::sin(2.0 * std::numbers::pi * 2.0 * p) / 2.0 +
                            std::sin(2.0 * std::numbers::pi * 3.0 * p) / 3.0;
    CHECK(nearlyEqual(t.samples()[static_cast<std::size_t>(j)], expected,
                      1e-14, 1e-14));
  }
  CHECK(t.formula() == "table[N=64, harmonics<=3]");
  CHECK(t.maxHarmonic() == 3);
  CHECK(t.spectrumKnown());
}

TEST_CASE("interpolation bounds hold and are attained", "[d6wt]") {
  // Single sine, N = 64: h = 2*pi/64.
  const auto t = unwrap(Wavetable::fromHarmonics(pureSine(), 64));
  const double h = 2.0 * std::numbers::pi / 64.0;
  const double linBound = unwrap(interpolationErrorBound(t, Interp::Linear));
  const double cubBound = unwrap(interpolationErrorBound(t, Interp::Cubic));
  CHECK(nearlyEqual(linBound, h * h / 8.0, 0.0, 1e-15));
  CHECK(nearlyEqual(cubBound, 3.0 / 128.0 * std::pow(h, 4), 0.0, 1e-15));

  const double linWorst = worstError(t, Interp::Linear, 1, false);
  const double cubWorst = worstError(t, Interp::Cubic, 1, false);
  CHECK(linWorst <= linBound);
  CHECK(linWorst >= 0.9 * linBound);
  CHECK(cubWorst <= cubBound);
  CHECK(cubWorst >= 0.9 * cubBound);
  CHECK(cubWorst < linWorst / 100.0);

  // Multi-partial table: the summed bound still holds (not tight).
  const auto saw = unwrap(Wavetable::fromHarmonics(inverseSeries(8), 256));
  CHECK(worstError(saw, Interp::Linear, 8, true) <=
        unwrap(interpolationErrorBound(saw, Interp::Linear)));
  CHECK(worstError(saw, Interp::Cubic, 8, true) <=
        unwrap(interpolationErrorBound(saw, Interp::Cubic)));
  // Sample tables carry no spectrum: no bound to state.
  const auto raw = unwrap(Wavetable::fromSamples({0.0, 1.0, 0.0, -1.0}));
  CHECK(interpolationErrorBound(raw, Interp::Cubic).error() ==
        QuantityError::DomainError);
}

TEST_CASE("wavetable tone agrees with additive within the bound", "[d6wt]") {
  const auto series = inverseSeries(8);
  const auto table = unwrap(Wavetable::fromHarmonics(series, 2048));
  const auto ref = unwrap(additive(FrequencyHz{220.0},
                                   constant(AmplitudeLinear{0.25}), series));
  for (const Interp in : {Interp::Linear, Interp::Cubic}) {
    const double bound = 0.25 * unwrap(interpolationErrorBound(table, in));
    const auto sig =
        unwrap(wavetableTone(table, FrequencyHz{220.0},
                             constant(AmplitudeLinear{0.25}),
                             PhaseRadians{0.0}, in));
    for (const double t : {0.0, 0.000283, 0.0137, 0.371, 0.9999}) {
      const double diff = std::fabs(sig.at(TimePointSeconds{t}).value() -
                                    ref.at(TimePointSeconds{t}).value());
      CHECK(diff <= bound + 1e-11);  // slack: phase roundoff only
    }
  }
  const auto cubic = unwrap(wavetableTone(table, FrequencyHz{220.0}));
  CHECK(cubic.formula() == "wavetable_tone(table[N=2048, harmonics<=8], cubic)");
}

TEST_CASE("wavetable sweep follows the analytic sweep phase", "[d6wt]") {
  const auto table = unwrap(Wavetable::fromHarmonics(pureSine(), 1024));
  const double bound =
      0.5 * unwrap(interpolationErrorBound(table, Interp::Cubic));
  const auto sig = unwrap(wavetableSweep(
      table, FrequencyHz{110.0}, FrequencySlopeHzPerSec{770.0},
      constant(AmplitudeLinear{0.5}), PhaseRadians{0.3}));
  const auto ref =
      sineSweep(FrequencyHz{110.0}, FrequencySlopeHzPerSec{770.0},
                constant(AmplitudeLinear{0.5}), PhaseRadians{0.3});
  for (const double t : {0.0, 0.0021, 0.25, 0.618, 0.999}) {
    const double diff = std::fabs(sig.at(TimePointSeconds{t}).value() -
                                  ref.at(TimePointSeconds{t}).value());
    CHECK(diff <= bound + 1e-11);  // slack: phase roundoff only
  }
}

TEST_CASE("wavetable renders block-invariant and clean", "[d6wt]") {
  const auto table = unwrap(Wavetable::fromHarmonics(inverseSeries(16), 2048));
  const auto sig = unwrap(wavetableTone(table, FrequencyHz{220.0},
                                        constant(AmplitudeLinear{0.25})));
  std::vector<std::vector<double>> runs;
  for (const std::uint64_t bs : {64ULL, 127ULL, 512ULL}) {
    MemorySink mem;
    const auto stats = unwrap(renderOffline(
        sig,
        RenderConfig{SampleRateHz{48000.0}, FrameCount{4800},
                     TimePointSeconds{0.0}, bs},
        mem));
    CHECK(stats.isClean());
    runs.push_back(mem.samples());
  }
  CHECK(runs[0] == runs[1]);
  CHECK(runs[1] == runs[2]);
  CHECK(runs[0].size() == 4800);
}

TEST_CASE("frequency limit is fs over twice the highest harmonic",
          "[d6wt]") {
  const auto saw = unwrap(Wavetable::fromHarmonics(inverseSeries(8), 256));
  CHECK(unwrap(wavetableFrequencyLimit(saw, SampleRateHz{48000.0})) ==
        FrequencyHz{3000.0});
  // Sample table, N = 64: conservative K = 31.
  const auto raw = unwrap(Wavetable::fromSamples(std::vector<double>(64, 0.0)));
  CHECK(raw.maxHarmonic() == 31);
  CHECK(unwrap(wavetableFrequencyLimit(raw, SampleRateHz{48000.0})) ==
        FrequencyHz{48000.0 / 62.0});
  CHECK(wavetableFrequencyLimit(saw, SampleRateHz{0.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("wavetable creation and lookup reject invalid input", "[d6wt]") {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(Wavetable::fromSamples({}).error() == QuantityError::InvalidArgument);
  CHECK(Wavetable::fromSamples({0.0, 1.0, 0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(Wavetable::fromSamples({0.0, nan, 0.0, 1.0}).error() ==
        QuantityError::InvalidArgument);
  // Non-integer partial cannot live in a single cycle.
  const auto fifth = unwrap(HarmonicSeries::create(
      {Harmonic{FrequencyRatio{1.5}, AmplitudeLinear{1.0}}}));
  CHECK(Wavetable::fromHarmonics(fifth, 64).error() ==
        QuantityError::InvalidArgument);
  // N must exceed 2K: K = 8 needs N >= 17.
  CHECK(Wavetable::fromHarmonics(inverseSeries(8), 16).error() ==
        QuantityError::InvalidArgument);
  CHECK(Wavetable::fromHarmonics(inverseSeries(8), 17).ok());
  CHECK(Wavetable::fromHarmonics(pureSine(), kMaxWavetableSize + 1).error() ==
        QuantityError::InvalidArgument);

  const auto t = unwrap(Wavetable::fromHarmonics(pureSine(), 16));
  CHECK(wavetableValue(t, 1.0, Interp::Cubic).error() ==
        QuantityError::InvalidArgument);
  CHECK(wavetableValue(t, -0.1, Interp::Cubic).error() ==
        QuantityError::InvalidArgument);
  CHECK(wavetableValue(t, nan, Interp::Linear).error() ==
        QuantityError::InvalidArgument);
  CHECK(wavetableTone(t, FrequencyHz{0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(wavetableSweep(t, FrequencyHz{nan}, FrequencySlopeHzPerSec{1.0})
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("wavetable lookup stays in range just below phase one", "[d6wt]") {
  const auto t = unwrap(Wavetable::fromHarmonics(pureSine(), 1000003));
  const double p = std::nextafter(1.0, 0.0);
  const double v = unwrap(wavetableValue(t, p, Interp::Cubic));
  CHECK(std::isfinite(v));
  CHECK(std::fabs(v) < 1e-5);
}
