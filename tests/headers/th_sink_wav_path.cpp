// Header self-containment: mmathen/sink/wav_path.h compiles standalone.
#include <mmathen/sink/wav_path.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sink/wav_path.h is self-contained", "[headers]") {
  const auto file = mmathen::wavOutputPath("mmath-header.wav");
  CHECK(file.filename() == "mmath-header.wav");
  CHECK(!file.parent_path().empty());
  const auto artifact = mmathen::artifactWavPath("mmath-header.wav");
  if (!artifact.empty()) CHECK(artifact.filename() == "mmath-header.wav");
  CHECK(mmathen::copyWavFile(file, {}));
}
