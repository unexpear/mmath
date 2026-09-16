// Header self-containment: mmathen/math/scalar.h compiles standalone.
#include <mmathen/math/scalar.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("scalar.h is self-contained", "[headers]") {
  CHECK(mmathen::isFiniteReal(1.0));
}
