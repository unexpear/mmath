// Header self-containment: mmathen/music/mos.h compiles standalone.
#include <mmathen/music/mos.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/mos.h is self-contained", "[headers]") {
  const auto s = mmathen::mosWord(mmathen::Cents{700.0}, 5);
  CHECK(s.ok());
}
