// Header self-containment: mmathen/signal/signal.h compiles standalone.
#include <mmathen/signal/signal.h>

#include <mmathen/time/factories.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/signal.h is self-contained", "[headers]") {
  const auto s = mmathen::Signal<mmathen::SignedAudioValue>{
      mmathen::constant(mmathen::SignedAudioValue{0.0})};
  CHECK(s.at(mmathen::TimePointSeconds{0.0}) ==
        mmathen::SignedAudioValue{0.0});
}
