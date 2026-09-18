// Header self-containment: mmathen/physics/coupled_string.h compiles
// standalone.
#include <mmathen/physics/coupled_string.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/coupled_string.h is self-contained", "[headers]") {
  const auto s = mmathen::makeWaveSystem1D(
      mmathen::DistanceMeters{1.0}, mmathen::SpeedMetersPerSecond{2.0},
      mmathen::Boundary::Fixed, mmathen::Boundary::Fixed);
  REQUIRE(s.ok());
  CHECK(mmathen::makeCoupledString(s.value(), mmathen::TensionNewtons{1.0},
                                   mmathen::BridgeOscillator{})
            .ok());
}
