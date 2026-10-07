// Header self-containment: mmathen/instruments/flute.h compiles standalone.
#include <mmathen/instruments/flute.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/flute.h is self-contained", "[headers]") {
  const auto flute = mmathen::makeFlute(mmathen::FluteSpec{
      0.6, 2.8e-4, 101325.0, 293.15, 4.0e-6, 1.0e-3, 8.0e-3, 0.5});
  REQUIRE(flute.ok());
  CHECK(flute.value().pressureReflection == -1.0);
  CHECK(mmathen::flutePipeMode(flute.value(), 2).ok());
  CHECK(mmathen::fluteEmbouchure(flute.value(), 400.0, 1.0, 0.0, 0.0).ok());
}
