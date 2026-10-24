// Header self-containment: a plucked course compiles from its own header.
#include <mmathen/instruments/plucked_string.h>

#include <catch2/catch_test_macros.hpp>

#include <utility>

#include <mmathen/sink/sink.h>

TEST_CASE("instruments/plucked_string.h is self-contained", "[headers]") {
  auto course = mmathen::makeStringCourseAtPitch(
      mmathen::DistanceMeters{0.33}, "A4", 1.0, 1.0, 1.0, 500.0, "probe");
  REQUIRE(course.ok());
  auto ideal = mmathen::courseString(course.value());
  REQUIRE(ideal.ok());
  auto loss = mmathen::lossFromT60(
      ideal.value().speed, mmathen::FrequencyHz{440.0},
      mmathen::DurationSeconds{1.5}, mmathen::FrequencyHz{1760.0},
      mmathen::DurationSeconds{0.3});
  REQUIRE(loss.ok());
  const mmathen::Excitation pluck{mmathen::ExcitationKind::Pluck, 0.4, 1.0e-4};
  auto made = mmathen::PluckedStringVoice::create(
      course.value(), pluck, loss.value(), 0.0, 2, 0,
      mmathen::SampleRateHz{48000.0}, 0.3, mmathen::StringReadout::Displacement);
  REQUIRE(made.ok());
  auto voice = std::move(made).value();
  mmathen::NullSink sink;
  CHECK(mmathen::renderVoice(
            voice, mmathen::VoiceRenderConfig{mmathen::FrameCount{16}, 8}, sink)
            .ok());
  CHECK(voice.fundamentalHz() > 0.0);
}
