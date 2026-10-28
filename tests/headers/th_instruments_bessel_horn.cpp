// Header self-containment: mmathen/instruments/bessel_horn.h compiles standalone.
#include <mmathen/instruments/bessel_horn.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/bessel_horn.h is self-contained", "[headers]") {
  const auto cylinder = mmathen::besselHornResonanceHz(0.0, 0.2, 1.4, 343.0, 1);
  REQUIRE(cylinder.ok());
  CHECK(cylinder.value() > 60.0);
  CHECK(cylinder.value() < 62.0);
}
