// Header self-containment: the voice path compiles from its own headers.
#include <mmathen/render/waveguide_voice.h>
#include <mmathen/instruments/render_status.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>

#include <mmathen/physics/wave1d.h>
#include <mmathen/sink/sink.h>

TEST_CASE("render/voice.h is self-contained", "[headers]") {
  CHECK(mmathen::kInstrumentRenderStatusCount == 26);
  CHECK(mmathen::instrumentsThatRenderAudio() == 0);
  auto system = mmathen::idealString(mmathen::DistanceMeters{0.5},
                                     mmathen::TensionNewtons{100.0},
                                     mmathen::LinearDensityKgPerM{0.001});
  REQUIRE(system.ok());
  auto design = mmathen::designTunedLoop(system.value(),
                                         mmathen::SampleRateHz{48000.0}, 2, 0);
  REQUIRE(design.ok());
  auto shape = mmathen::InitialShape::mode(system.value(), 1, 1.0e-4);
  REQUIRE(shape.ok());
  auto made = mmathen::WaveguideVoice::create(
      shape.value(), design.value().filters, mmathen::SampleRateHz{48000.0},
      0.25);
  REQUIRE(made.ok());
  auto voice = std::move(made).value();
  mmathen::NullSink sink;
  CHECK(mmathen::renderVoice(voice,
                             mmathen::VoiceRenderConfig{
                                 mmathen::FrameCount{16}, 8},
                             sink)
            .ok());
}
