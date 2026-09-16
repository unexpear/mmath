// Header self-containment: mmathen/render/offline.h compiles standalone.
#include <mmathen/render/offline.h>

#include <catch2/catch_test_macros.hpp>

#include <mmathen/signal/oscillator.h>
#include <mmathen/time/factories.h>

TEST_CASE("render/offline.h is self-contained", "[headers]") {
  mmathen::NullSink sink;
  const auto sig = mmathen::sine(mmathen::FrequencyHz{440.0});
  const mmathen::RenderConfig cfg{mmathen::SampleRateHz{48000.0},
                                  mmathen::FrameCount{8},
                                  mmathen::TimePointSeconds{0.0}, 8};
  CHECK(mmathen::renderOffline(sig, cfg, sink).ok());
}
