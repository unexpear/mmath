// Header self-containment: mmathen/music/pitch.h compiles standalone.
#include <mmathen/music/pitch.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/pitch.h is self-contained", "[headers]") {
  CHECK(mmathen::makePitch("A4").ok());
}
