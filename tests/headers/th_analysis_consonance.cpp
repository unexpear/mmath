// Header self-containment: mmathen/analysis/consonance.h compiles standalone.
#include <mmathen/analysis/consonance.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/consonance.h is self-contained", "[headers]") {
  const auto cbw =
      mmathen::criticalBandwidth(mmathen::FrequencyHz{440.0});
  CHECK(cbw.ok());
}
