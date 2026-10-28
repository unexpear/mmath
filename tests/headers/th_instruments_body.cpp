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
  const auto partner =
      mmathen::makeBodyResonator(mmathen::BodyModeSpec{660.0, 15.0, 0.2});
  REQUIRE(partner.ok());
  CHECK(mmathen::coupleBodyModes(mode.value(), partner.value(), 1000.0).ok());
  const auto field = mmathen::compactMonopolePressure(
      std::complex<double>{0.001, 0.0}, 2.0, 200.0, 1.0e5, 300.0);
  REQUIRE(field.ok());
  CHECK(std::abs(field.value().pressure) > 0.0);
  const mmathen::StringCourse course{
      mmathen::StringGeometry{mmathen::DistanceMeters{0.5}},
      mmathen::StringMaterial{mmathen::LinearDensityKgPerM{0.01}},
      mmathen::StringTuning{"synthetic", mmathen::FrequencyHz{200.0}},
      mmathen::TensionNewtons{400.0},
      mmathen::TabulatedRange{},
      mmathen::TabulatedRange{},
      0.0,
      false,
      "synthetic"};
  const auto moved = mmathen::resistiveBridgeMode(course, 40.0, 1);
  REQUIRE(moved.ok());
  CHECK(moved.value().decayPerSecond > 0.0);
  CHECK(mmathen::bridgeResistance(mode.value(), 440.0).ok());
  CHECK(mmathen::movingBridgeModes(course, mode.value(), 1).ok());
}
