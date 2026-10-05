// Header self-containment: mmathen/instruments/reed.h compiles standalone.
#include <mmathen/instruments/reed.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/reed.h is self-contained", "[headers]") {
  const auto reed = mmathen::makeReedCharacteristic(
      mmathen::ReedSpec{1.0e-5, 8.0e3, 101325.0, 293.15});
  REQUIRE(reed.ok());
  CHECK(mmathen::reedVolumeFlow(reed.value(), 0.0).ok());
  const auto bore = mmathen::makeReedBore(
      mmathen::ReedBoreSpec{0.5, 1.0e-4, 101325.0, 293.15, mmathen::BoreFarEnd::Open});
  REQUIRE(bore.ok());
  CHECK(mmathen::boreWaveSplit(bore.value(), 1.0, 0.0).ok());
  const auto inertia = mmathen::makeReedInertia(
      reed.value(), mmathen::ReedInertiaSpec{1.0e-5, 1.0e-4, 1.0e-3, 0.0});
  REQUIRE(inertia.ok());
  CHECK(mmathen::reedTipAcceleration(inertia.value(), 0.0, 0.0, 0.0).ok());
}
