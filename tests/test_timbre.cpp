// M3 — additive timbre and ADSR tests (Catch2, test-only dependency).
//
// Segment anchors use binary-exact values (attack 0.25, decay/sustain/
// release 0.5, on 0.0, off 2.0) so boundary expectations are CHECK-exact,
// not tolerance-based. Harmonic expectations are hand-written sin sums,
// independent of the additive() builder.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/envelope.h>
#include <mmathen/signal/harmonics.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
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

HarmonicSeries twoPartial() {
  return unwrap(HarmonicSeries::create(
      {Harmonic{FrequencyRatio{1.0}, AmplitudeLinear{1.0}},
       Harmonic{FrequencyRatio{2.0}, AmplitudeLinear{0.5}}}));
}

Function1D<AmplitudeLinear> adsrExact() {
  auto segs = unwrap(makeADSR(DurationSeconds{0.25}, DurationSeconds{0.5},
                              AmplitudeLinear{0.5}, DurationSeconds{0.5}));
  return unwrap(adsr(segs, TimePointSeconds{0.0}, TimePointSeconds{2.0}));
}
}  // namespace

TEST_CASE("additive matches a hand-written harmonic sum", "[timbre]") {
  const auto sig =
      unwrap(additive(FrequencyHz{440.0}, constant(AmplitudeLinear{1.0}),
                      twoPartial()));
  for (const double t : {0.0, 0.000283, 0.01, 0.371}) {
    const double expected =
        std::sin(2.0 * std::numbers::pi * 440.0 * t) +
        0.5 * std::sin(2.0 * std::numbers::pi * 880.0 * t);
    CHECK(nearlyEqual(sig.at(TimePointSeconds{t}),
                      SignedAudioValue{expected}, SignedAudioValue{1e-9},
                      1e-9));
  }
}

TEST_CASE("harmonic tables validate at creation", "[timbre]") {
  CHECK(twoPartial().formula() == "series[2 partials]");  CHECK(HarmonicSeries::create({}).error() == QuantityError::InvalidArgument);
  CHECK(HarmonicSeries::create({{FrequencyRatio{0.0}, AmplitudeLinear{1.0}}})
            .error() == QuantityError::InvalidArgument);
  CHECK(HarmonicSeries::create({{FrequencyRatio{2.0}, AmplitudeLinear{-1.0}}})
            .error() == QuantityError::InvalidArgument);
  CHECK(additive(FrequencyHz{0.0}, constant(AmplitudeLinear{1.0}),
                 twoPartial())
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("ADSR segment boundaries are exact", "[timbre]") {
  const auto env = adsrExact();
  CHECK(env.at(TimePointSeconds{-0.5}) == AmplitudeLinear{0.0});
  CHECK(env.at(TimePointSeconds{0.0}) == AmplitudeLinear{0.0});
  CHECK(env.at(TimePointSeconds{0.125}) == AmplitudeLinear{0.5});
  CHECK(env.at(TimePointSeconds{0.25}) == AmplitudeLinear{1.0});
  CHECK(env.at(TimePointSeconds{0.75}) == AmplitudeLinear{0.5});
  CHECK(env.at(TimePointSeconds{1.0}) == AmplitudeLinear{0.5});
  CHECK(env.at(TimePointSeconds{2.0}) == AmplitudeLinear{0.5});
  CHECK(env.at(TimePointSeconds{2.25}) == AmplitudeLinear{0.25});
  CHECK(env.at(TimePointSeconds{2.5}) == AmplitudeLinear{0.0});
  CHECK(env.at(TimePointSeconds{3.0}) == AmplitudeLinear{0.0});
  CHECK(env.formula() == "adsr(a,d,s,r)");
}

TEST_CASE("ADSR creation and note range validate", "[timbre]") {
  CHECK(makeADSR(DurationSeconds{-0.1}, DurationSeconds{0.5},
                 AmplitudeLinear{0.5}, DurationSeconds{0.5})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeADSR(DurationSeconds{0.1}, DurationSeconds{0.5},
                 AmplitudeLinear{1.5}, DurationSeconds{0.5})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeADSR(DurationSeconds{0.1}, DurationSeconds{0.5},
                 AmplitudeLinear{0.5},
                 DurationSeconds{
                     std::numeric_limits<double>::quiet_NaN()})
            .error() == QuantityError::InvalidArgument);
  const auto segs =
      unwrap(makeADSR(DurationSeconds{0.25}, DurationSeconds{0.5},
                      AmplitudeLinear{0.5}, DurationSeconds{0.5}));
  CHECK(adsr(segs, TimePointSeconds{2.0}, TimePointSeconds{1.0}).error() ==
        QuantityError::InvalidArgument);  // off before on
}

TEST_CASE("shaped additive note renders clean", "[timbre]") {
  const auto env = adsrExact();
  // Two partials (1.0 + 0.5x) at full scale peak at 1.5, so applyGain 0.5
  // keeps the render inside [-1,1] — overload stays an export-boundary fact.
  const auto sig = applyGain(
      unwrap(additive(FrequencyHz{220.0}, env, twoPartial())),
      GainLinear{0.5});
  MemorySink mem;
  const auto stats = unwrap(renderOffline(
      sig,
      RenderConfig{SampleRateHz{48000.0}, FrameCount{24000},
                   TimePointSeconds{0.0}, 512},
      mem));
  CHECK(stats.framesRendered == 24000);
  CHECK(stats.isClean());
}
