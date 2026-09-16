// Header self-containment: mmathen/music/euclid.h compiles standalone.
#include <mmathen/music/euclid.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/euclid.h is self-contained", "[headers]") {
  const auto r = mmathen::euclideanRhythm(3, 8);
  CHECK(r.ok());
}
