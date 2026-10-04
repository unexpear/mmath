// Header self-containment: mmathen/instruments/harpsichord.h compiles standalone.
#include <mmathen/instruments/harpsichord.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/harpsichord.h is self-contained", "[headers]") {
  CHECK(mmathen::frenchDouble().ok());
}
