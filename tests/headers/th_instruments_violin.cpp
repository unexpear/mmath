// Header self-containment: mmathen/instruments/violin.h compiles standalone.
#include <mmathen/instruments/violin.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/violin.h is self-contained", "[headers]") {
  CHECK(mmathen::violin().ok());
}
