// Header self-containment: mmathen/signal/fm.h compiles standalone.
#include <mmathen/signal/fm.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/fm.h is self-contained", "[headers]") {
  const auto s = mmathen::fmTone(mmathen::FrequencyHz{440.0},
                                 mmathen::FrequencyHz{110.0}, 1.0);
  CHECK(s.ok());
}
