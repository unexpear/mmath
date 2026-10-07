// Header self-containment: mmathen/instruments/trumpet.h compiles standalone.
#include <mmathen/instruments/trumpet.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/trumpet.h is self-contained", "[headers]") {
  const auto trumpet = mmathen::makeTrumpet(mmathen::TrumpetSpec{
      0.7, 1.2e-4, 101325.0, 293.15, 2.0e-6, 1.0e-5, 1.5e-5, 1.0e-2, 2.0e-5,
      500.0, 1.0e-4, 0.0});
  REQUIRE(trumpet.ok());
  CHECK(trumpet.value().pressureReflection == -1.0);
  CHECK(mmathen::trumpetCylinderMode(trumpet.value(), 1).ok());
  CHECK(mmathen::trumpetCylinderMode(trumpet.value(), 2).error() ==
        mmathen::QuantityError::DomainError);
  CHECK(mmathen::trumpetMouthpieceImpedance(trumpet.value(), 200.0).ok());
  CHECK(mmathen::trumpetEmbouchure(trumpet.value(), 1000.0, 1.0, 0.0, 0.0, 0.0)
            .ok());
}
