// Header self-containment: mmathen/sink/wav_reader.h compiles standalone.
#include <mmathen/sink/wav_reader.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sink/wav_reader.h is self-contained", "[headers]") {
  CHECK(mmathen::parseWav({}).ok() == false);
}
