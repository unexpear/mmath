// Header self-containment: mmathen/music/forte_table.h compiles standalone.
#include <mmathen/music/forte_table.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/forte_table.h is self-contained", "[headers]") {
  CHECK(mmathen::kForteTable[0].number == "0-1");
}
