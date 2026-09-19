// Header self-containment: mmathen/dsp/filters.h compiles standalone.
#include <mmathen/dsp/filters.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/filters.h is self-contained", "[headers]") {
  CHECK(mmathen::makeNotch(mmathen::FrequencyHz{1000.0}, 2.0,
                           mmathen::SampleRateHz{48000.0})
            .ok());
}
