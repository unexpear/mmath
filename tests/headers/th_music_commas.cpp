// Header self-containment: mmathen/music/commas.h compiles standalone.
#include <mmathen/music/commas.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/commas.h is self-contained", "[headers]") {
  CHECK(mmathen::commas::syntonic() == mmathen::Comma{81, 80});
}
