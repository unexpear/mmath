// Header self-containment: mmathen/music/temperament.h compiles standalone.
#include <mmathen/music/temperament.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/temperament.h is self-contained", "[headers]") {
  const auto e = mmathen::nearestDegreeError(12, mmathen::FrequencyRatio{1.5});
  CHECK(e.ok());
}
