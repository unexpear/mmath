// Header self-containment: mmathen/signal/phase.h compiles standalone.
#include <mmathen/signal/phase.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/phase.h is self-contained", "[headers]") {
  const mmathen::ConstantFrequency c{mmathen::FrequencyHz{440.0}};
  CHECK(c.frequencyAt(mmathen::TimePointSeconds{1.0}) ==
        mmathen::FrequencyHz{440.0});
}
