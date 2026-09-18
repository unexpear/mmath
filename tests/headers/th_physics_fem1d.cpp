// Header self-containment: mmathen/physics/fem1d.h compiles standalone.
#include <mmathen/physics/fem1d.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/fem1d.h is self-contained", "[headers]") {
  const auto s = mmathen::makeWaveSystem1D(
      mmathen::DistanceMeters{1.0}, mmathen::SpeedMetersPerSecond{2.0},
      mmathen::Boundary::Fixed, mmathen::Boundary::Fixed);
  REQUIRE(s.ok());
  CHECK(mmathen::femStringFrequencies(s.value(), 8, 2).ok());
}
