// Header self-containment: mmathen/music/transforms.h compiles standalone.
#include <mmathen/music/transforms.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/transforms.h is self-contained", "[headers]") {
  CHECK(mmathen::makeTimeStretch(2.0).ok());
  CHECK(mmathen::retrograde(mmathen::Melody{}).notes.empty());
}
