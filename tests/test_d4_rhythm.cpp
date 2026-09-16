// D4 — voice-leading and Euclidean rhythm tests (Catch2).
//
// Euclidean anchors are Toussaint 2005 canonical forms (tresillo,
// cinquillo, Ruchenitza): E(3,8), E(5,8), E(4,7), E(2,5).

#include <catch2/catch_test_macros.hpp>

#include <vector>

#include <mmathen/music/euclid.h>
#include <mmathen/music/voiceleading.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("voice-leading cost is exact motion plus parallels", "[d4]") {
  // C major [60,64,67] -> G major [59,62,67]: motion 1+2+0 = 3, no parallels.
  const auto plain =
      unwrap(voiceLeadingCost({60, 64, 67}, {59, 62, 67}));
  CHECK(plain.totalMotion == 3);
  CHECK(plain.parallelFifths == 0);
  CHECK(plain.parallelOctaves == 0);
  CHECK(plain.cost == 3);
  // Parallel fifths: [60,67] -> [62,69] (both up a step, fifth preserved).
  const auto par5 = unwrap(voiceLeadingCost({60, 67}, {62, 69}));
  CHECK(par5.totalMotion == 4);
  CHECK(par5.parallelFifths == 1);
  CHECK(par5.cost == 4 + kParallelPenalty);
  // Contrary motion kills the parallel: [60,67] -> [62,65]? fifth 7 -> 3.
  const auto contra = unwrap(voiceLeadingCost({60, 67}, {62, 65}));
  CHECK(contra.parallelFifths == 0);
  // Parallel octaves: [60,72] -> [62,74].
  const auto par8 = unwrap(voiceLeadingCost({60, 72}, {62, 74}));
  CHECK(par8.parallelOctaves == 1);
  // Static voices are oblique, never parallel.
  const auto stat = unwrap(voiceLeadingCost({60, 67}, {60, 67}));
  CHECK(stat.totalMotion == 0);
  CHECK(stat.parallelFifths == 0);
  CHECK(stat.cost == 0);
  CHECK(voiceLeadingCost({60}, {60, 64}).error() ==
        QuantityError::InvalidArgument);
  CHECK(voiceLeadingCost({}, {}).error() == QuantityError::InvalidArgument);
  CHECK(voiceLeadingCost({60, 128}, {60, 64}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("Euclidean rhythms match the canonical forms", "[d4]") {
  CHECK(unwrap(euclideanRhythm(3, 8)) ==
        std::vector<int>({1, 0, 0, 1, 0, 0, 1, 0}));  // tresillo
  CHECK(unwrap(euclideanRhythm(5, 8)) ==
        std::vector<int>({1, 0, 1, 1, 0, 1, 1, 0}));  // cinquillo
  CHECK(unwrap(euclideanRhythm(4, 7)) ==
        std::vector<int>({1, 0, 1, 0, 1, 0, 1}));  // Ruchenitza
  CHECK(unwrap(euclideanRhythm(2, 5)) ==
        std::vector<int>({1, 0, 1, 0, 0}));
  CHECK(unwrap(euclideanRhythm(1, 4)) ==
        std::vector<int>({1, 0, 0, 0}));
  CHECK(unwrap(euclideanRhythm(4, 4)) ==
        std::vector<int>({1, 1, 1, 1}));
  // Rotation shifts the cycle; negative wraps the other way.
  CHECK(unwrap(euclideanRhythm(3, 8, 3)) ==
        std::vector<int>({1, 0, 0, 1, 0, 1, 0, 0}));
  CHECK(unwrap(euclideanRhythm(3, 8, -1)) ==
        std::vector<int>({0, 1, 0, 0, 1, 0, 0, 1}));
  CHECK(euclideanRhythm(0, 8).error() == QuantityError::InvalidArgument);
  CHECK(euclideanRhythm(9, 8).error() == QuantityError::InvalidArgument);
  CHECK(euclideanRhythm(3, 0).error() == QuantityError::InvalidArgument);
}

TEST_CASE("swing delays off-beats along the triplet grid", "[d4]") {
  const DurationSeconds eighth{0.25};
  CHECK(unwrap(swingDelay(false, eighth, 1.0)) == DurationSeconds{0.0});
  CHECK(unwrap(swingDelay(true, eighth, 0.0)) == DurationSeconds{0.0});
  // Full swing: off-beat lands on the last triplet third (delay = 1/3 eighth
  // here expressed in the same beat grid the caller schedules with).
  CHECK(unwrap(swingDelay(true, eighth, 1.0)) ==
        DurationSeconds{0.25 / 3.0});
  CHECK(unwrap(swingDelay(true, eighth, 0.5)) ==
        DurationSeconds{0.25 * 0.5 / 3.0});
  CHECK(swingDelay(true, eighth, 1.5).error() ==
        QuantityError::InvalidArgument);
  CHECK(swingDelay(true, eighth, -0.5).error() ==
        QuantityError::InvalidArgument);
}
