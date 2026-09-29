// Header self-containment: mmathen/instruments/instrument.h compiles standalone.
#include <mmathen/instruments/instrument.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/instrument.h is self-contained", "[headers]") {
  const mmathen::BowedStringRow row{"A4", 440.0, 0.58, 0.75, 48.3, 62.7};
  CHECK(mmathen::makeStringCourse(mmathen::DistanceMeters{0.328}, row,
                                  mmathen::kGuettlerBowedStringSource)
            .ok());
}
