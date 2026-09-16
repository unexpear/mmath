// D3 — comma arithmetic and EDO error tests (Catch2).
//
// Cent anchors are mpmath-verified literals; rational identities
// (PC/syntonic == schisma, syntonic*schisma == pythagorean) are EXACT
// integer arithmetic, the strongest assertions in the file.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include <mmathen/math/comparison.h>
#include <mmathen/music/commas.h>
#include <mmathen/music/temperament.h>
#include <mmathen/units/music_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("comma cents match the literature", "[d3]") {
  CHECK(nearlyEqual(unwrap(commaCents(commas::syntonic())).value(),
                    21.50628959671485, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(commaCents(commas::pythagorean())).value(),
                    23.46001038464901, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(commaCents(commas::schisma())).value(),
                    1.9537207879341594, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(commaCents(commas::diaschisma())).value(),
                    19.55256880878069, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(commaCents(commas::lesserDiesis())).value(),
                    41.05885840549555, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(commaCents(commas::greaterDiesis())).value(),
                    62.56514800221040, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(commaCents(commas::septimal())).value(),
                    27.26409180010024, 1e-9, 1e-12));
}

TEST_CASE("comma algebra is exact rational arithmetic", "[d3]") {
  // Pythagorean / syntonic == schisma, exactly (not approximately).
  CHECK(unwrap(divideComma(commas::pythagorean(), commas::syntonic())) ==
        commas::schisma());
  CHECK(unwrap(multiplyComma(commas::syntonic(), commas::schisma())) ==
        commas::pythagorean());
  // Construction normalizes: 162/160 is 81/80.
  CHECK(unwrap(makeComma(162, 160)) == commas::syntonic());
  CHECK(makeComma(0, 80).error() == QuantityError::InvalidArgument);
  CHECK(makeComma(-81, 80).error() == QuantityError::InvalidArgument);
  CHECK(makeComma(81, 0).error() == QuantityError::InvalidArgument);
  // Overflow is RangeError, never wraparound.
  const auto huge = unwrap(makeComma(9223372036854775807LL, 1));
  CHECK(multiplyComma(huge, huge).error() == QuantityError::RangeError);
}

TEST_CASE("prime limits and Tenney heights", "[d3]") {
  CHECK(unwrap(primeLimit(commas::syntonic())) == 5);
  CHECK(unwrap(primeLimit(commas::pythagorean())) == 3);
  CHECK(unwrap(primeLimit(commas::septimal())) == 7);
  CHECK(unwrap(primeLimit(unwrap(makeComma(1, 1)))) == 2);  // vacuous limit
  CHECK(nearlyEqual(unwrap(tenneyHeight(commas::syntonic())), 12.66178, 1e-5,
                    1e-9));
}

TEST_CASE("EDO errors reproduce the published tables", "[d3]") {
  const FrequencyRatio fifth{1.5};
  const FrequencyRatio maj3{1.25};
  const FrequencyRatio min3{1.2};
  CHECK(nearlyEqual(unwrap(nearestDegreeError(12, fifth)).errorCents,
                    -1.9550008653874177, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(nearestDegreeError(12, maj3)).errorCents,
                    13.686286135165183, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(nearestDegreeError(12, min3)).errorCents,
                    -15.641287000552600, 1e-9, 1e-12));
  CHECK(unwrap(nearestDegreeError(19, fifth)).degree == 11);
  CHECK(nearlyEqual(unwrap(nearestDegreeError(19, fifth)).errorCents,
                    -7.218158760124296, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(nearestDegreeError(19, maj3)).errorCents,
                    -7.366345443782216, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(nearestDegreeError(19, min3)).errorCents,
                    0.14818668365792003, 1e-9, 1e-12));
  CHECK(unwrap(nearestDegreeError(31, fifth)).degree == 18);
  CHECK(nearlyEqual(unwrap(nearestDegreeError(31, fifth)).errorCents,
                    -5.180807317000343, 1e-9, 1e-12));
  CHECK(nearestDegreeError(0, fifth).error() == QuantityError::InvalidArgument);
  CHECK(nearestDegreeError(12, FrequencyRatio{0.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("the triad ranking is computed, not asserted", "[d3]") {
  const std::vector<FrequencyRatio> triad{FrequencyRatio{1.5},
                                          FrequencyRatio{1.25},
                                          FrequencyRatio{1.2}};
  const double w12 = unwrap(errorReport(12, triad)).worstAbsCents;
  const double w19 = unwrap(errorReport(19, triad)).worstAbsCents;
  const double w31 = unwrap(errorReport(31, triad)).worstAbsCents;
  CHECK(w12 > 15.0);
  CHECK(w19 > 7.0);
  CHECK(w19 < 8.0);
  CHECK(w31 > 5.0);
  CHECK(w31 < 7.0);
  CHECK(w12 > w19);
  CHECK(w19 > w31);
  CHECK(errorReport(12, {}).error() == QuantityError::InvalidArgument);
}
