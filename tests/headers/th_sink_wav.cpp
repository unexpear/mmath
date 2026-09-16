// Header self-containment: mmathen/sink/wav.h compiles standalone.
#include <mmathen/sink/wav.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sink/wav.h is self-contained", "[headers]") {
  CHECK(mmathen::WavSink::encodeSample(0.0) == 0);
}
