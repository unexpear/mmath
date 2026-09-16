// Header self-containment: mmathen/music/pcset.h compiles standalone.
#include <mmathen/music/pcset.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/pcset.h is self-contained", "[headers]") {
  const auto s = mmathen::makePitchClassSet({0, 4, 7});
  CHECK(s.ok());
}
