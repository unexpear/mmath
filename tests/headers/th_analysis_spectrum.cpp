// Header self-containment: mmathen/analysis/spectrum.h compiles standalone.
#include <mmathen/analysis/spectrum.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/spectrum.h is self-contained", "[headers]") {
  CHECK(mmathen::fractionalOctaveBands(3, mmathen::FrequencyHz{100.0},
                                       mmathen::FrequencyHz{1000.0})
            .ok());
}
