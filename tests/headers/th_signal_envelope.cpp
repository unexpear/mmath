// Header self-containment: mmathen/signal/envelope.h compiles standalone.
#include <mmathen/signal/envelope.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/envelope.h is self-contained", "[headers]") {
  const auto segs = mmathen::makeADSR(
      mmathen::DurationSeconds{0.1}, mmathen::DurationSeconds{0.1},
      mmathen::AmplitudeLinear{0.5}, mmathen::DurationSeconds{0.1});
  CHECK(segs.ok());
}
