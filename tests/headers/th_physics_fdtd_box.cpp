// Header self-containment: mmathen/physics/fdtd_box.h compiles standalone.
#include <mmathen/physics/fdtd_box.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/fdtd_box.h is self-contained", "[headers]") {
  const auto g = mmathen::makeBoxGrid(
      2, {10, 10, 0}, mmathen::DistanceMeters{0.1},
      mmathen::SpeedMetersPerSecond{1.0}, mmathen::BoxWalls::Dirichlet);
  CHECK(g.ok());
}
