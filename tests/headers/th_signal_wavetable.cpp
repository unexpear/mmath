// Header self-containment: mmathen/signal/wavetable.h compiles standalone.
#include <mmathen/signal/wavetable.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/wavetable.h is self-contained", "[headers]") {
  const auto t = mmathen::Wavetable::fromSamples({0.0, 1.0, 0.0, -1.0});
  CHECK(t.ok());
}
