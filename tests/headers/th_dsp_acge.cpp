// Header self-containment: mmathen/dsp/acge.h compiles standalone.
#include <mmathen/dsp/acge.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/acge.h is self-contained", "[headers]") { CHECK(true); }
