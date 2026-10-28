// Header self-containment: mmathen/physics/modes.h compiles standalone.
#include <mmathen/physics/modes.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/modes.h is self-contained", "[headers]") {
  CHECK(mmathen::openPipeMode(mmathen::DistanceMeters{1.0},
                              mmathen::SpeedMetersPerSecond{340.0}, 1)
            .ok());
  CHECK(mmathen::circularMembraneMode(
            mmathen::DistanceMeters{0.2}, mmathen::SurfaceTensionNewtonsPerMeter{100.0},
            mmathen::ArealDensityKgPerSquareMeter{0.25}, 0, 1)
            .ok());
}
