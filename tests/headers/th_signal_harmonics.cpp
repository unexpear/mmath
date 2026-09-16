// Header self-containment: mmathen/signal/harmonics.h compiles standalone.
#include <mmathen/signal/harmonics.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/harmonics.h is self-contained", "[headers]") {
  const auto series = mmathen::HarmonicSeries::create(
      {mmathen::Harmonic{mmathen::FrequencyRatio{1.0},
                         mmathen::AmplitudeLinear{1.0}}});
  CHECK(series.ok());
}
