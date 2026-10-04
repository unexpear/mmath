// Header self-containment: mmathen/instruments/reed.h compiles standalone.
#include <mmathen/instruments/reed.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/reed.h is self-contained", "[headers]") {
  const auto reed = mmathen::makeReedCharacteristic(
      mmathen::ReedSpec{1.0e-5, 8.0e3, 101325.0, 293.15});
  REQUIRE(reed.ok());
  CHECK(mmathen::reedVolumeFlow(reed.value(), 0.0).ok());
}
