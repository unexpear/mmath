// Header self-containment: mmathen/dsp/biquad.h compiles standalone.
#include <mmathen/dsp/biquad.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/biquad.h is self-contained", "[headers]") {
  const auto k = mmathen::makeLowpass(mmathen::FrequencyHz{1000.0}, 0.7071,
                                      mmathen::SampleRateHz{48000.0});
  CHECK(k.ok());
  CHECK(mmathen::isStable(k.value()));
}
