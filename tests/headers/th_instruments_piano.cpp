// Header self-containment: mmathen/instruments/piano.h compiles standalone.
#include <mmathen/instruments/piano.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/piano.h is self-contained", "[headers]") {
  CHECK(mmathen::concertGrandStringBank().ok());
}
