// Header self-containment: mmathen/analysis/bs1770.h compiles standalone.
#include <mmathen/analysis/bs1770.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/bs1770.h is self-contained", "[headers]") {
  CHECK(mmathen::kWeighting(mmathen::SampleRateHz{48000.0}).ok());
}
