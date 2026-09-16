// Header self-containment: mmathen/signal/bandlimited.h compiles standalone.
#include <mmathen/signal/bandlimited.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/bandlimited.h is self-contained", "[headers]") {
  CHECK(mmathen::polyblepSawValue(0.25, 0.01).ok());
}
