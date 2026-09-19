// Header self-containment: mmathen/dsp/overlap_add.h compiles standalone.
#include <mmathen/dsp/overlap_add.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/overlap_add.h is self-contained", "[headers]") {
  CHECK(mmathen::OverlapAdd::create(256).ok());
}
