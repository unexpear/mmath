// Header self-containment: mmathen/instruments/double_bass.h compiles standalone.
#include <mmathen/instruments/double_bass.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/double_bass.h is self-contained", "[headers]") {
  CHECK(mmathen::doubleBass().ok());
}
