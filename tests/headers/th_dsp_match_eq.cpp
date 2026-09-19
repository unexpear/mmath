// Header self-containment: mmathen/dsp/match_eq.h compiles standalone.
#include <mmathen/dsp/match_eq.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/match_eq.h is self-contained", "[headers]") {
  CHECK(mmathen::MatchEqOptions{}.amount == 1.0);
}
