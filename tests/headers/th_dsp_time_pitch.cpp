// Header self-containment: mmathen/dsp/time_pitch.h compiles standalone.
#include <mmathen/dsp/time_pitch.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/time_pitch.h is self-contained", "[headers]") {
  CHECK(mmathen::PhaseVocoderOptions{}.frameSize == 2048);
}
