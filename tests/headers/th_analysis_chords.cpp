// Header self-containment: mmathen/analysis/chords.h compiles standalone.
#include <mmathen/analysis/chords.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/chords.h is self-contained", "[headers]") {
  CHECK(mmathen::chordName(mmathen::kNoChord) == "N");
}
