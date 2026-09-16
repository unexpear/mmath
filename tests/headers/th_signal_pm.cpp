// Header self-containment: mmathen/signal/pm.h compiles standalone.
#include <mmathen/signal/pm.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/pm.h is self-contained", "[headers]") {
  const auto s = mmathen::pmTone(mmathen::FrequencyHz{440.0},
                                 mmathen::FrequencyHz{110.0}, 1.0);
  CHECK(s.ok());
}
