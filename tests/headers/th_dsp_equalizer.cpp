// Header self-containment: mmathen/dsp/equalizer.h compiles standalone.
#include <mmathen/dsp/equalizer.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/equalizer.h is self-contained", "[headers]") {
  CHECK(mmathen::ParametricEq::create({mmathen::EqBand{}},
                                      mmathen::SampleRateHz{48000.0})
            .ok());
}
