// Header self-containment: mmathen/signal/inharmonic.h compiles standalone.
#include <mmathen/signal/inharmonic.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/inharmonic.h is self-contained", "[headers]") {
  const auto spec = mmathen::makeStiffSpectrum(mmathen::FrequencyHz{220.0},
                                               2e-4, 6);
  CHECK(spec.ok());
}
