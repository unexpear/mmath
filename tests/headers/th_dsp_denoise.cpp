// Header self-containment: mmathen/dsp/denoise.h compiles standalone.
#include <mmathen/dsp/denoise.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/denoise.h is self-contained", "[headers]") {
  CHECK(mmathen::special::e1(1.0) > 0.2);
}
