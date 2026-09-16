// Phase 0 — units tests (Catch2, test-only dependency).
//
// Compile-time guards use requires-expressions so a passing build proves the
// operator table; isolated try_compile probes in tests/compile_fail/ provide
// the companion per-TU evidence with preserved diagnostics.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <mmathen/math/comparison.h>
#include <mmathen/math/constants.h>
#include <mmathen/units/conversions.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return r.value();
}
}  // namespace

// --- Operator table guards ---
//
// Negative cases (forbidden operations) are proven by the isolated
// try_compile probes in tests/compile_fail/ (same headers, same C++20
// configuration, diagnostics preserved). requires-expression probing is
// deliberately NOT used for negative operator checks: MSVC emits a hard
// error instead of a substitution failure for some failing operator lookups.
// Positive companions below prove the table entries exist in this TU.

static_assert(!std::is_convertible_v<SampleRateHz, FrequencyHz>);
static_assert(!std::is_convertible_v<FrequencyHz, double>);
static_assert(!std::is_convertible_v<double, FrequencyHz>);
static_assert(!std::is_convertible_v<FrequencyRatio, AmplitudeLinear>);

// --- Valid companions using the same headers and configuration ---
// (Positive requires-checks: these expressions must exist.)

static_assert(requires(DurationSeconds a, DurationSeconds b) {
  a + b;
  a - b;
  -a;
});
static_assert(requires(TimePointSeconds t, DurationSeconds d) {
  t + d;
  t - d;
});
static_assert(requires(TimePointSeconds a, TimePointSeconds b) { a - b; });
static_assert(requires(FrequencyHz f, FrequencyHz g) { f / g; });
static_assert(requires(AngularFrequencyRadPerSec w, DurationSeconds d) {
  w * d;
});
static_assert(
    std::same_as<decltype(unwrap(period(FrequencyHz{440.0}))),
                 DurationSeconds>);

TEST_CASE("period identity T = 1/f", "[units]") {
  // T(440) = 1/440 s ~ 0.0022727272727272727 s (prose anchor; the definitional
  // check against 1.0/440.0 is the tight one).
  const DurationSeconds t = unwrap(period(FrequencyHz{440.0}));
  CHECK(nearlyEqual(t, DurationSeconds{1.0 / 440.0},
                    DurationSeconds{1e-18}, 0.0));
  CHECK(nearlyEqual(t.value(), 0.0022727272727272727, 1e-15, 0.0));
  CHECK(period(FrequencyHz{0.0}).error() == QuantityError::DomainError);
  CHECK(period(FrequencyHz{-440.0}).error() == QuantityError::DomainError);
}

TEST_CASE("angular conversion is explicit: w = 880*pi", "[units]") {
  // w(440) = 880*pi ~ 2764.601535159018 rad/s.
  const auto w = unwrap(toAngular(FrequencyHz{440.0}));
  CHECK(nearlyEqual(w.value(), 2764.601535159018, 1e-9, 1e-12));
}

TEST_CASE("frequency ratio from division", "[units]") {
  const FrequencyRatio r =
      unwrap(FrequencyHz{660.0} / FrequencyHz{440.0});
  CHECK(r == FrequencyRatio{1.5});
  CHECK((FrequencyHz{440.0} / FrequencyHz{0.0}).error() ==
        QuantityError::DomainError);
  CHECK((FrequencyHz{-440.0} / FrequencyHz{440.0}).error() ==
        QuantityError::DomainError);
}

TEST_CASE("phase from angular frequency times duration", "[units]") {
  // 2*pi rad/s * 0.5 s = pi rad.
  const AngularFrequencyRadPerSec w{constants::twoPi};
  const PhaseRadians p = unwrap(w * DurationSeconds{0.5});
  CHECK(nearlyEqual(p.value(), 3.141592653589793, 1e-12, 1e-12));
  CHECK(unwrap(DurationSeconds{0.5} * w) == p);  // symmetric overload
}

TEST_CASE("cents and ratio independent anchors", "[units]") {
  // Exact binary anchors (not round trips): log2(1) = 0, log2(1/2) = -1,
  // 2^0 = 1, 2^-1 = 1/2 are all exact in binary floating point.
  CHECK(unwrap(cents(FrequencyRatio{1.0})) == Cents{0.0});
  CHECK(unwrap(cents(FrequencyRatio{0.5})) == Cents{-1200.0});
  CHECK(unwrap(ratio(Cents{0.0})) == FrequencyRatio{1.0});
  CHECK(unwrap(ratio(Cents{-1200.0})) == FrequencyRatio{0.5});
  // Inexact anchor with documented tolerance (libm log2 varies by platform):
  // cents(3/2) ~ 701.9550008653874.
  CHECK(nearlyEqual(unwrap(cents(FrequencyRatio{1.5})), Cents{701.9550008653874},
                    Cents{1e-9}, 0.0));
  // Round trip is supplementary, never the sole evidence.
  CHECK(nearlyEqual(unwrap(ratio(unwrap(cents(FrequencyRatio{1.5})))).value(),
                    1.5, 1e-12, 1e-12));
}

TEST_CASE("binary-exact ratios are stored exactly", "[units]") {
  // 2, 3/2 = 1+1/2, 5/4 = 1+1/4 are binary-exact values. This pins storage,
  // not a general exact-rational system (deferred).
  CHECK(FrequencyRatio{2.0}.value() == 2.0);
  CHECK(FrequencyRatio{1.5}.value() == 1.5);
  CHECK(FrequencyRatio{1.25}.value() == 1.25);
}

TEST_CASE("beat duration", "[units]") {
  // 1 beat @120BPM = 0.5 s; dotted quarter = 0.75 s.
  CHECK(unwrap(beatDuration(Beat{1.0}, TempoBPM{120.0})) ==
        DurationSeconds{0.5});
  CHECK(unwrap(beatDuration(Beat{1.5}, TempoBPM{120.0})) ==
        DurationSeconds{0.75});
  CHECK(beatDuration(Beat{1.0}, TempoBPM{0.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("frame count is authoritative and exact", "[units]") {
  // 1.0 s @48kHz = exactly 48000 frames (integer-exact, no epsilon).
  CHECK(unwrap(frameCountForDuration(DurationSeconds{1.0},
                                     SampleRateHz{48000.0})) ==
        FrameCount{48000});
  // Last sample of N frames is at t_0 + (N-1)/fs, never t_0 + 1.
  const TimePointSeconds last = unwrap(
      timeAt(TimePointSeconds{0.0}, FrameIndex{47999}, SampleRateHz{48000.0}));
  CHECK(nearlyEqual(last, TimePointSeconds{47999.0 / 48000.0},
                    DurationSeconds{0.0}, 1e-12));
  CHECK(last < TimePointSeconds{1.0});
  CHECK(unwrap(timeAt(TimePointSeconds{2.0}, FrameIndex{0},
                      SampleRateHz{48000.0})) == TimePointSeconds{2.0});
  CHECK(frameCountForDuration(DurationSeconds{-1.0}, SampleRateHz{48000.0})
            .error() == QuantityError::DomainError);
  CHECK(timeAt(TimePointSeconds{0.0}, FrameIndex{0}, SampleRateHz{0.0})
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("time arithmetic follows the operator table", "[units]") {
  CHECK(TimePointSeconds{2.0} - TimePointSeconds{0.5} == DurationSeconds{1.5});
  CHECK(TimePointSeconds{2.0} + DurationSeconds{0.5} == TimePointSeconds{2.5});
  CHECK(TimePointSeconds{2.0} - DurationSeconds{0.5} == TimePointSeconds{1.5});
  CHECK(DurationSeconds{0.5} + DurationSeconds{0.25} == DurationSeconds{0.75});
  CHECK(-DurationSeconds{0.5} == DurationSeconds{-0.5});
  CHECK(2.0 * FrequencyHz{440.0} == FrequencyHz{880.0});
}

TEST_CASE("checked factories accept and reject per the table", "[units]") {
  CHECK(makeSampleRate(48000.0).ok());
  CHECK(makeSampleRate(0.0).error() == QuantityError::InvalidArgument);
  CHECK(makeSampleRate(-1.0).error() == QuantityError::InvalidArgument);
  CHECK(makeFrequencyRatio(1.5).ok());
  CHECK(makeFrequencyRatio(0.0).error() == QuantityError::InvalidArgument);
  CHECK(makeFrequencyRatio(-2.0).error() == QuantityError::InvalidArgument);
  // Negative cents and unwrapped phase are accepted; out-of-range signed
  // audio values are accepted (overload is an export-boundary concern).
  CHECK(Cents{-120.0}.value() == -120.0);
  CHECK(PhaseRadians{-3.0}.value() == -3.0);
  CHECK(SignedAudioValue{1.5}.value() == 1.5);
}

TEST_CASE("frame counters: uint64 rules", "[units]") {
  CHECK(unwrap(makeFrameCount(0)) == FrameCount{0});  // zero accepted
  CHECK(unwrap(makeFrameCount(48000)) == FrameCount{48000});
  CHECK(unwrap(makeFrameCountFromReal(48000.0)) == FrameCount{48000});
  CHECK(makeFrameCountFromReal(-1.0).error() ==
        QuantityError::InvalidArgument);  // negative rejected pre-conversion
  CHECK(makeFrameCountFromReal(1.5).error() ==
        QuantityError::InvalidArgument);  // fractional rejected
  CHECK(makeFrameCountFromReal(
            std::numeric_limits<double>::quiet_NaN())
            .error() == QuantityError::InvalidArgument);
  CHECK(tryIncrement(FrameCount{std::numeric_limits<std::uint64_t>::max()})
            .error() == QuantityError::RangeError);  // overflow defined
  CHECK(tryDecrement(FrameCount{0}).error() ==
        QuantityError::InvalidArgument);  // below zero defined
  CHECK(unwrap(tryIncrement(FrameCount{41})) == FrameCount{42});
  CHECK(unwrap(tryDecrement(FrameCount{42})) == FrameCount{41});
  // Frame equality is exact; index->time conversion is approximate (covered
  // by the endpoint test above, not by an epsilon on the counter itself).
  CHECK(FrameCount{7} == FrameCount{7});
  CHECK_FALSE(FrameCount{7} == FrameCount{8});
}

TEST_CASE("named alternative-unit conversions", "[units]") {
  CHECK(unwrap(durationFromMilliseconds(500.0)) == DurationSeconds{0.5});
  CHECK(unwrap(frequencyFromKilohertz(0.44)) == FrequencyHz{440.0});
  CHECK(durationFromMilliseconds(
            std::numeric_limits<double>::quiet_NaN())
            .error() == QuantityError::InvalidArgument);
}
