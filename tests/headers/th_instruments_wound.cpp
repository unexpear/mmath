// Header self-containment: mmathen/instruments/wound.h compiles standalone.
#include <mmathen/instruments/wound.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/wound.h is self-contained", "[headers]") {
  const mmathen::StringCourse course{
      mmathen::StringGeometry{mmathen::DistanceMeters{0.5}},
      mmathen::StringMaterial{mmathen::LinearDensityKgPerM{0.01}},
      mmathen::StringTuning{"synthetic", mmathen::FrequencyHz{100.0}},
      mmathen::TensionNewtons{40.0},
      mmathen::TabulatedRange{},
      mmathen::TabulatedRange{},
      0.0,
      false,
      "synthetic"};
  const auto wound = mmathen::makeWoundString(course, 1.0e-3, 2.0e11, 8000.0);
  REQUIRE(wound.ok());
  CHECK(wound.value().windingFactor > 1.0);
  CHECK(wound.value().stiffness > 0.0);
}
