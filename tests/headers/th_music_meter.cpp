// Header self-containment: mmathen/music/meter.h compiles standalone.
#include <mmathen/music/meter.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/meter.h is self-contained", "[headers]") {
  CHECK(mmathen::Meter::create(4, 4).ok());
}
