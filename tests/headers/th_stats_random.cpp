// Header self-containment: mmathen/stats/random.h compiles standalone.
#include <mmathen/stats/random.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stats/random.h is self-contained", "[headers]") {
  mmathen::SplitMix64 r{1};
  CHECK(r.uniform() < 1.0);
}
