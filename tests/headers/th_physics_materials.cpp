// Header self-containment: mmathen/physics/materials.h compiles standalone.
#include <mmathen/physics/materials.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/materials.h is self-contained", "[headers]") {
  CHECK(mmathen::chabassier2013Strings().size() == 5);
}
