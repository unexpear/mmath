// Header self-containment: mmathen/signal/spectral.h compiles standalone.
#include <mmathen/signal/spectral.h>

#include <mmathen/time/factories.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/spectral.h is self-contained", "[headers]") {
  CHECK(mmathen::SpectralVoice{mmathen::FrequencyRatio{1.0},
                               mmathen::constant(
                                   mmathen::AmplitudeLinear{1.0})}
            .ratio == mmathen::FrequencyRatio{1.0});
}
