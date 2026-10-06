// Header self-containment: mmathen/physics/circular_plate.h compiles standalone.
#include <mmathen/physics/circular_plate.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/circular_plate.h is self-contained", "[headers]") {
  CHECK(mmathen::freeCircularPlateFrequency(mmathen::DistanceMeters{0.2}, 0.001,
                                            1.0e11, 7800.0, 0.3, 2, 1)
            .ok());
}
