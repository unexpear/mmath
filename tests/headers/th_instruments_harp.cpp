// Header self-containment: mmathen/instruments/harp.h compiles standalone.
#include <mmathen/instruments/harp.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/harp.h is self-contained", "[headers]") {
  CHECK(mmathen::concertPedalHarp().ok());
}
