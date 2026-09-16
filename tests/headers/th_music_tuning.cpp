// Header self-containment: mmathen/music/tuning.h compiles standalone.
#include <mmathen/music/tuning.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/tuning.h is self-contained", "[headers]") {
  const mmathen::TwelveTET tet{mmathen::FrequencyHz{440.0}, 0};
  CHECK(tet.degreeToFrequency(12).ok());
}
