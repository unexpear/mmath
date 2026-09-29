// Header self-containment: mmathen/instruments/viola.h compiles standalone.
#include <mmathen/instruments/viola.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/viola.h is self-contained", "[headers]") {
  CHECK(mmathen::viola().ok());
}
