// Header self-containment: mmathen/instruments/guitar.h compiles standalone.
#include <mmathen/instruments/guitar.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/guitar.h is self-contained", "[headers]") {
  CHECK(mmathen::classicalGuitar().ok());
}
