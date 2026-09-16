// Header self-containment: mmathen/math/constants.h compiles standalone.
#include <mmathen/math/constants.h>

#include <catch2/catch_test_macros.hpp>

#include <numbers>

TEST_CASE("constants.h is self-contained", "[headers]") {
  CHECK(mmathen::constants::pi == std::numbers::pi);
}
