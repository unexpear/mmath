// Header self-containment: mmathen/units/music_units.h compiles standalone.
#include <mmathen/units/music_units.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music_units.h is self-contained", "[headers]") {
  CHECK(mmathen::makeFrequencyRatio(1.5).ok());
}
