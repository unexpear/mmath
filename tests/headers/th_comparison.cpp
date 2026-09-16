// Header self-containment: mmathen/math/comparison.h compiles standalone.
#include <mmathen/math/comparison.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("comparison.h is self-contained", "[headers]") {
  CHECK(mmathen::nearlyEqual(1.0, 1.0, 0.0, 0.0));
}
