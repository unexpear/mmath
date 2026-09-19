// Header self-containment: mmathen/dsp/crossfade.h compiles standalone.
#include <mmathen/dsp/crossfade.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/crossfade.h is self-contained", "[headers]") {
  CHECK(mmathen::SpliceReader<float>::create(2, 64).ok());
}
