// Header self-containment: mmathen/physics/absorption.h compiles standalone.
#include <mmathen/physics/absorption.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/absorption.h is self-contained", "[headers]") {
  const auto air = mmathen::makeHumidAir(
      mmathen::TemperatureKelvin{293.15},
      mmathen::RelativeHumidity{0.5}, mmathen::PressurePascals{101325.0});
  CHECK(air.ok());
}
