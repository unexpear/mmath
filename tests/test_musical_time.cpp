// M4 — tempo curves and meter tests (Catch2, test-only dependency).
//
// Ramp anchors are independent literals (verified against a Python reference
// at authoring time); round trips supplement, never replace, them.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/music/meter.h>
#include <mmathen/time/tempo.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

static_assert(TempoCurve<ConstantTempo>);
static_assert(TempoCurve<LinearRampTempo>);

TEST_CASE("constant tempo converts both directions", "[musical-time]") {
  const auto tempo = unwrap(ConstantTempo::create(TempoBPM{120.0}));
  CHECK(unwrap(tempo.beatsAt(TimePointSeconds{0.5})) == Beat{1.0});
  CHECK(unwrap(tempo.beatsAt(TimePointSeconds{2.0})) == Beat{4.0});
  CHECK(unwrap(tempo.secondsAt(Beat{3.0})) == TimePointSeconds{1.5});
  // Round trip is arithmetically exact here up to floating-point
  // reassociation (same operations, reversed): tight tolerance, not ==.
  CHECK(nearlyEqual(unwrap(tempo.secondsAt(unwrap(tempo.beatsAt(
                          TimePointSeconds{1.7}))))
                        .value(),
                    1.7, 0.0, 1e-12));
  CHECK(ConstantTempo::create(TempoBPM{0.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("linear ramp integrates to the quadratic", "[musical-time]") {
  // 60 -> 120 BPM over 60 s from t = 0.
  const auto ramp = unwrap(LinearRampTempo::create(
      TempoBPM{60.0}, TempoBPM{120.0}, TimePointSeconds{0.0},
      DurationSeconds{60.0}));
  CHECK(unwrap(ramp.beatsAt(TimePointSeconds{0.0})) == Beat{0.0});
  CHECK(unwrap(ramp.beatsAt(TimePointSeconds{60.0})) == Beat{90.0});
  CHECK(unwrap(ramp.beatsAt(TimePointSeconds{30.0})) == Beat{37.5});
  // Constant extension outside the ramp span.
  CHECK(unwrap(ramp.beatsAt(TimePointSeconds{-10.0})) == Beat{-10.0});
  CHECK(unwrap(ramp.beatsAt(TimePointSeconds{70.0})) == Beat{110.0});
}

TEST_CASE("ramp inverse recovers absolute times", "[musical-time]") {
  const auto ramp = unwrap(LinearRampTempo::create(
      TempoBPM{60.0}, TempoBPM{120.0}, TimePointSeconds{0.0},
      DurationSeconds{60.0}));
  CHECK(nearlyEqual(unwrap(ramp.secondsAt(Beat{45.0})).value(),
                    34.868329805051374, 1e-9, 1e-9));
  CHECK(nearlyEqual(unwrap(ramp.secondsAt(Beat{90.0})).value(), 60.0, 1e-9,
                    1e-9));
  CHECK(nearlyEqual(unwrap(ramp.secondsAt(Beat{110.0})).value(), 70.0, 1e-9,
                    1e-9));
  // Round trips across all three regions.
  for (const double b : {-5.0, 0.0, 37.5, 90.0, 150.0}) {
    const auto there = unwrap(ramp.secondsAt(Beat{b}));
    CHECK(nearlyEqual(unwrap(ramp.beatsAt(there)).value(), b, 1e-9, 1e-9));
  }
  // Decelerando inverts too (negative quadratic coefficient).
  const auto down = unwrap(LinearRampTempo::create(
      TempoBPM{120.0}, TempoBPM{60.0}, TimePointSeconds{0.0},
      DurationSeconds{60.0}));
  CHECK(unwrap(down.beatsAt(TimePointSeconds{60.0})) == Beat{90.0});
  CHECK(nearlyEqual(unwrap(down.secondsAt(Beat{90.0})).value(), 60.0, 1e-9,
                    1e-9));
}

TEST_CASE("ramp creation validates", "[musical-time]") {
  CHECK(LinearRampTempo::create(TempoBPM{60.0}, TempoBPM{120.0},
                                TimePointSeconds{0.0}, DurationSeconds{0.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(LinearRampTempo::create(TempoBPM{-60.0}, TempoBPM{120.0},
                                TimePointSeconds{0.0}, DurationSeconds{60.0})
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("meter maps measures to beats to seconds", "[musical-time]") {
  const auto four4 = unwrap(Meter::create(4, 4));
  CHECK(unwrap(four4.measureDuration(TempoBPM{120.0})) ==
        DurationSeconds{2.0});
  CHECK(unwrap(four4.measureBeatToBeats(5, 3)) == Beat{18.0});
  const auto seven8 = unwrap(Meter::create(7, 8));
  CHECK(unwrap(seven8.measureDuration(TempoBPM{120.0})) ==
        DurationSeconds{1.75});
  CHECK(Meter::create(0, 4).error() == QuantityError::InvalidArgument);
  CHECK(Meter::create(4, 0).error() == QuantityError::InvalidArgument);
  CHECK(four4.measureBeatToBeats(1, 5).error() ==
        QuantityError::InvalidArgument);
  CHECK(four4.measureBeatToBeats(0, 1).error() ==
        QuantityError::InvalidArgument);

  const auto additive =
      unwrap(AdditiveMeter::create({3, 2, 2}, 8));  // 3+2+2 eighths
  CHECK(nearlyEqual(additive.totalBeats(), 3.5, 1e-12, 0.0));
  CHECK(AdditiveMeter::create({}, 8).error() == QuantityError::InvalidArgument);
  CHECK(AdditiveMeter::create({3, 0, 2}, 8).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("meter plus tempo schedules absolute onsets", "[musical-time]") {
  // Measure 2, beat 1 of 4/4 at 120 BPM starts at 2.0 s.
  const auto four4 = unwrap(Meter::create(4, 4));
  const auto tempo = unwrap(ConstantTempo::create(TempoBPM{120.0}));
  const Beat b = unwrap(four4.measureBeatToBeats(2, 1));
  CHECK(b == Beat{4.0});
  CHECK(unwrap(tempo.secondsAt(b)) == TimePointSeconds{2.0});
}
