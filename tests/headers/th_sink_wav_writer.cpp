// Header self-containment: mmathen/sink/wav_writer.h compiles standalone.
#include <mmathen/sink/wav_writer.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sink/wav_writer.h is self-contained", "[headers]") {
  CHECK(mmathen::WavWriter::encode(0.5, 16) == 16384);
}
