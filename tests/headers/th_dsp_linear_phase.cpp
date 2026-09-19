// Header self-containment: mmathen/dsp/linear_phase.h compiles standalone.
#include <mmathen/dsp/linear_phase.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/linear_phase.h is self-contained", "[headers]") { CHECK(true); }
