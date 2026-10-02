// Header self-containment: mmathen/instruments/body.h compiles standalone.
#include <mmathen/instruments/body.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/body.h is self-contained", "[headers]") {
  const auto mode =
      mmathen::makeBodyResonator(mmathen::BodyModeSpec{440.0, 10.0, 0.1});
  REQUIRE(mode.ok());
  CHECK(mmathen::bodyMobility(mode.value(), 440.0).ok());
}
