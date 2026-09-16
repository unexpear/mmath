// Header self-containment: mmathen/physics/fdtd1d.h compiles standalone.
#include <mmathen/physics/fdtd1d.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/fdtd1d.h is self-contained", "[headers]") {
  const auto s = mmathen::makeWaveSystem1D(
      mmathen::DistanceMeters{1.0}, mmathen::SpeedMetersPerSecond{2.0},
      mmathen::Boundary::Fixed, mmathen::Boundary::Fixed);
  CHECK(s.ok());
}
