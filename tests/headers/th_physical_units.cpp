// Header self-containment: mmathen/units/physical_units.h compiles standalone.
#include <mmathen/units/physical_units.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physical_units.h is self-contained", "[headers]") {
  CHECK(mmathen::makeSampleRate(48000.0).ok());
}
