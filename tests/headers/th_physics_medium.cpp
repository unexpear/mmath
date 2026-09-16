// Header self-containment: mmathen/physics/medium.h compiles standalone.
#include <mmathen/physics/medium.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/medium.h is self-contained", "[headers]") {
  const auto air = mmathen::makeAir(mmathen::TemperatureKelvin{293.15});
  CHECK(air.ok());
}
