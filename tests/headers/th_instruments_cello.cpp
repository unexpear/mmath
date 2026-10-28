// Header self-containment: mmathen/instruments/cello.h compiles standalone.
#include <mmathen/instruments/cello.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/cello.h is self-contained", "[headers]") {
  CHECK(mmathen::cello().ok());
}
