// Header self-containment: mmathen/instruments/electric_guitar.h compiles standalone.
#include <mmathen/instruments/electric_guitar.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/electric_guitar.h is self-contained", "[headers]") {
  CHECK(mmathen::electricGuitar().ok());
}
