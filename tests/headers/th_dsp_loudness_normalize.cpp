// Header self-containment: mmathen/dsp/loudness_normalize.h compiles standalone.
#include <mmathen/dsp/loudness_normalize.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/loudness_normalize.h is self-contained", "[headers]") {
  CHECK(mmathen::NormalizeTarget{}.lufs == -14.0);
}
