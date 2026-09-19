// Header self-containment: mmathen/dsp/multiband.h compiles standalone.
#include <mmathen/dsp/multiband.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/multiband.h is self-contained", "[headers]") { CHECK(true); }
