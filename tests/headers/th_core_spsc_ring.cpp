// Header self-containment: mmathen/core/spsc_ring.h compiles standalone.
#include <mmathen/core/spsc_ring.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("core/spsc_ring.h is self-contained", "[headers]") {
  CHECK(mmathen::SpscRing::create(8).ok());
}
