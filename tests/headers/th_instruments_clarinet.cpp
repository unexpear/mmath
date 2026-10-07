// Header self-containment: mmathen/instruments/clarinet.h compiles standalone.
#include <mmathen/instruments/clarinet.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/clarinet.h is self-contained", "[headers]") {
  const auto clarinet = mmathen::makeClarinet(mmathen::ClarinetSpec{
      0.5, 2.0e-4, 101325.0, 293.15, 1.5e-5, 6.0e3, 2.0e-5, 1.0e-4, 1.0e-3,
      0.0});
  REQUIRE(clarinet.ok());
  CHECK(clarinet.value().bore.farEnd == mmathen::BoreFarEnd::Open);
  CHECK(mmathen::clarinetCylinderMode(clarinet.value(), 3).ok());
  CHECK(mmathen::clarinetMouth(clarinet.value(), 2.0e3, 0.0, 0.0, 0.0, 0.0)
            .ok());
}
