// Header self-containment: mmathen/music/structures.h compiles standalone.
#include <mmathen/music/structures.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/structures.h is self-contained", "[headers]") {
  const auto chord = mmathen::Chord::create(
      mmathen::FrequencyHz{440.0}, {mmathen::FrequencyRatio{1.0}});
  CHECK(chord.ok());
}
