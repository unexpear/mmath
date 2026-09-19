// Header self-containment: mmathen/dsp/dynamics.h compiles standalone.
#include <mmathen/dsp/dynamics.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/dynamics.h is self-contained", "[headers]") {
  CHECK(mmathen::Limiter::create(mmathen::LimiterParams{},
                                 mmathen::SampleRateHz{48000.0})
            .ok());
}
