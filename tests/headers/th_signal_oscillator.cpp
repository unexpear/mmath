// Header self-containment: mmathen/signal/oscillator.h compiles standalone.
#include <mmathen/signal/oscillator.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/oscillator.h is self-contained", "[headers]") {
  const auto s = mmathen::sine(mmathen::FrequencyHz{440.0});
  CHECK(s.at(mmathen::TimePointSeconds{0.0}) ==
        mmathen::SignedAudioValue{0.0});
}
