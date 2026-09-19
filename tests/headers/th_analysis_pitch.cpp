// Header self-containment: mmathen/analysis/pitch.h compiles standalone.
#include <mmathen/analysis/pitch.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/pitch.h is self-contained", "[headers]") {
  CHECK(mmathen::PitchSearch{}.yinThreshold == 0.1);
}
