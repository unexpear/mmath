// Header self-containment: mmathen/analysis/analyzer.h compiles standalone.
#include <mmathen/analysis/analyzer.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/analyzer.h is self-contained", "[headers]") {
  CHECK(mmathen::MeterBallistics{}.releaseDbPerSecond > 0.0);
}
