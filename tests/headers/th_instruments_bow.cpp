// Header self-containment: mmathen/instruments/bow.h compiles standalone.
#include <mmathen/instruments/bow.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/bow.h is self-contained", "[headers]") {
  const mmathen::BowedStringRow row{"A4", 440.0, 0.58, 0.75, 48.3, 62.7};
  const auto course = mmathen::makeStringCourse(
      mmathen::DistanceMeters{0.328}, row, mmathen::kGuettlerBowedStringSource);
  const mmathen::BowStroke stroke{1.0 / 9.0, 0.2, 0.1, {0.8, 0.3}};
  CHECK(course.ok());
  const auto motion = mmathen::makeHelmholtzStroke(course.value(), stroke);
  REQUIRE(motion.ok());
  CHECK(mmathen::schellengMinimumForce(motion.value(), 50.0).ok());
  const auto captured =
      mmathen::captureStickSlip(course.value(), stroke, 4, 32, 1);
  CHECK(captured.ok());
}
