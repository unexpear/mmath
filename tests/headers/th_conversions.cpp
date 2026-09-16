// Header self-containment: mmathen/units/conversions.h compiles standalone.
#include <mmathen/units/conversions.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("conversions.h is self-contained", "[headers]") {
  CHECK(mmathen::period(mmathen::FrequencyHz{440.0}).ok());
}
