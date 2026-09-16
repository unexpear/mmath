// M1 — tuning system tests (Catch2, test-only dependency).
//
// Anchors are independent literals (checked against a Python reference at
// authoring time), never the implementation's own pow() output alone. Exact
// anchors (octave doubling, just ratios, 19-TET period) pin the math tightly.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/music/tuning.h>
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

static_assert(TuningSystem<TwelveTET>);
static_assert(TuningSystem<NineteenTET>);
static_assert(TuningSystem<ThirtyOneTET>);
static_assert(TuningSystem<JustIntonation>);
static_assert(TuningSystem<CentsTuning>);
static_assert(TuningSystem<CustomMap>);
static_assert(!TuningSystem<FrequencyHz>);  // a bare quantity is not a system

TEST_CASE("12-TET golden table from A440", "[tuning]") {
  const TwelveTET tet{FrequencyHz{440.0}, 0};
  CHECK(unwrap(tet.degreeToFrequency(0)) == FrequencyHz{440.0});
  CHECK(unwrap(tet.degreeToFrequency(12)) == FrequencyHz{880.0});
  CHECK(unwrap(tet.degreeToFrequency(-12)) == FrequencyHz{220.0});
  CHECK(tet.formula() == "f_ref * 2^(n/12)");
}

TEST_CASE("12-TET C major from C4", "[tuning]") {
  // C4 = 440*2^(-9/12) ~ 261.6255653005986; E4/G4 via independent literals.
  const TwelveTET tet{FrequencyHz{261.6255653005986}, 0};
  CHECK(nearlyEqual(unwrap(tet.degreeToFrequency(0)).value(),
                    261.6255653005986, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(tet.degreeToFrequency(4)).value(),
                    329.6275569128699, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(tet.degreeToFrequency(7)).value(),
                    391.99543598174927, 1e-9, 1e-12));
}

TEST_CASE("19-TET instrument pitch(n) = 220*2^(n/19)", "[tuning]") {
  const NineteenTET tet{FrequencyHz{220.0}, 0};
  CHECK(unwrap(tet.degreeToFrequency(0)) == FrequencyHz{220.0});
  CHECK(unwrap(tet.degreeToFrequency(19)) == FrequencyHz{440.0});  // exact
  CHECK(nearlyEqual(unwrap(tet.degreeToFrequency(7)).value(),
                    284.0066235484291, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(tet.degreeToFrequency(18)).value(),
                    424.23743909469783, 1e-9, 1e-12));
  CHECK(tet.formula() == "f_ref * 2^(n/19)");
}

TEST_CASE("31-TET period is exact", "[tuning]") {
  const ThirtyOneTET tet{FrequencyHz{220.0}, 0};
  CHECK(unwrap(tet.degreeToFrequency(31)) == FrequencyHz{440.0});
}

TEST_CASE("just intonation ratios are exact and octave-wrapping", "[tuning]") {
  auto just = unwrap(JustIntonation::create(
      FrequencyHz{440.0},
      {FrequencyRatio{1.0}, FrequencyRatio{5.0 / 4.0}, FrequencyRatio{3.0 / 2.0},
       FrequencyRatio{2.0}}));
  CHECK(unwrap(just.degreeToFrequency(0)) == FrequencyHz{440.0});
  CHECK(unwrap(just.degreeToFrequency(1)) == FrequencyHz{550.0});
  CHECK(unwrap(just.degreeToFrequency(2)) == FrequencyHz{660.0});
  CHECK(unwrap(just.degreeToFrequency(3)) == FrequencyHz{880.0});
  CHECK(unwrap(just.degreeToFrequency(4)) == FrequencyHz{880.0});  // wraps
  CHECK(unwrap(just.degreeToFrequency(-1)) == FrequencyHz{440.0});  // octave
  CHECK(unwrap(just.degreeToFrequency(-2)) == FrequencyHz{330.0});  // fifth
  CHECK(just.formula() == "just[4 ratios]");
}

TEST_CASE("just intonation creation validates its table", "[tuning]") {
  CHECK(JustIntonation::create(FrequencyHz{440.0}, {}).error() ==
        QuantityError::InvalidArgument);
  CHECK(JustIntonation::create(FrequencyHz{440.0}, {FrequencyRatio{0.0}})
            .error() == QuantityError::InvalidArgument);
  CHECK(JustIntonation::create(FrequencyHz{0.0}, {FrequencyRatio{1.0}})
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("cents tuning maps offsets with octave wrap", "[tuning]") {
  auto tuning = unwrap(CentsTuning::create(
      FrequencyHz{440.0}, {Cents{0.0}, Cents{400.0}, Cents{700.0},
                            Cents{1200.0}}));
  CHECK(unwrap(tuning.degreeToFrequency(0)) == FrequencyHz{440.0});
  CHECK(nearlyEqual(unwrap(tuning.degreeToFrequency(1)).value(),
                    554.3652619537442, 1e-9, 1e-12));  // 440*2^(1/3)
  CHECK(unwrap(tuning.degreeToFrequency(3)) == FrequencyHz{880.0});
  CHECK(unwrap(tuning.degreeToFrequency(4)) == FrequencyHz{880.0});
  CHECK(tuning.formula() == "cents[4 offsets]");
  CHECK(CentsTuning::create(FrequencyHz{440.0}, {}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("custom map carries equation and formula", "[tuning]") {
  const CustomMap map(
      [](int n) -> Result<FrequencyHz, QuantityError> {
        using R = Result<FrequencyHz, QuantityError>;
        const double f = 220.0 * std::pow(2.0, n / 19.0);
        if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
        return R::success(FrequencyHz{f});
      },
      "220 * 2^(n/19)");
  CHECK(unwrap(map.degreeToFrequency(19)) == FrequencyHz{440.0});
  CHECK(nearlyEqual(unwrap(map.degreeToFrequency(7)).value(),
                    284.0066235484291, 1e-9, 1e-12));
  CHECK(map.formula() == "220 * 2^(n/19)");
}

TEST_CASE("non-finite reference is a defined error", "[tuning]") {
  const TwelveTET tet{FrequencyHz{std::numeric_limits<double>::quiet_NaN()},
                      0};
  CHECK(tet.degreeToFrequency(0).error() == QuantityError::InvalidArgument);
}

TEST_CASE("extreme degrees are defined errors, not overflow", "[tuning]") {
  // Audit F1: int promotion before arithmetic; pow() reports RangeError.
  const TwelveTET tet{FrequencyHz{440.0}, 0};
  CHECK(tet.degreeToFrequency(std::numeric_limits<int>::max()).error() ==
        QuantityError::RangeError);
}
