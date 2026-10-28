// Header self-containment: mmathen/instruments/cymbal.h compiles standalone.
#include <mmathen/instruments/cymbal.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/cymbal.h is self-contained", "[headers]") {
  const auto set = mmathen::sabianXsrCompleteSet();
  REQUIRE(set.ok());
  const mmathen::CymbalMaterial material{0.001, 1.0e11, 7800.0, 0.3};
  CHECK(mmathen::cymbalModeFrequency(set.value().ride, material, 2, 1).ok());
}
