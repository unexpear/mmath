// Header self-containment: mmathen/instruments/body.h compiles standalone.
#include <mmathen/instruments/body.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/body.h is self-contained", "[headers]") {
  const auto mode =
      mmathen::makeBodyResonator(mmathen::BodyModeSpec{440.0, 10.0, 0.1});
  REQUIRE(mode.ok());
  CHECK(mmathen::bodyMobility(mode.value(), 440.0).ok());
  const mmathen::BodyModeSpec specs[] = {{440.0, 10.0, 0.1},
                                          {880.0, 12.0, 0.2}};
  mmathen::BodyResonator modes[2];
  REQUIRE(mmathen::makeBodyResonators(specs, modes).ok());
  CHECK(mmathen::bodyMobility(modes, 440.0).ok());
  const auto cavity = mmathen::makeCavityHelmholtz(
      mmathen::CavityHelmholtzSpec{0.04, 0.2, 0.1, 1.0e5, 300.0});
  REQUIRE(cavity.ok());
  CHECK(cavity.value().frequencyHz > 0.0);
}
