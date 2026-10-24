// Header self-containment: mmathen/instruments/cymbal_vk.h compiles standalone.
#include <mmathen/instruments/cymbal_vk.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/cymbal_vk.h is self-contained", "[headers]") {
  mmathen::VonKarmanPlateSpec spec;
  spec.maxModeHz = 300.0;
  spec.inPlaneModes = 2;
  CHECK(mmathen::buildVonKarmanPlate(spec).ok());
}
