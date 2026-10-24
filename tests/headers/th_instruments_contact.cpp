// Header self-containment: the contact voices compile from their headers.
#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/physics/radiation.h>
#include <mmathen/instruments/bowed_string.h>
#include <mmathen/instruments/mallet_voice.h>
#include <mmathen/instruments/cavity_drum.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <mmathen/instruments/struck_string.h>

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <utility>

TEST_CASE("contact voices are self-contained", "[headers]") {
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
  const mmathen::SampleRateHz rate{16000.0};
  mmathen::BowStroke stroke{0.2, 0.05, 0.2, mmathen::BowFriction{0.8, 0.3}};
  auto bowed = mmathen::BowedStringVoice::create(
      course.value(), stroke, loss.value(), 0.0, 2, 0, rate, 8);
  REQUIRE(bowed.ok());
  auto bow = std::move(bowed).value();
  double block[8];
  CHECK(bow.renderBlock(block, mmathen::FrameCount{8}).ok());

  mmathen::HammerStrike hammer{0.2, 0.01, 0.4};
  auto struck = mmathen::FeltHammerVoice::create(
      course.value(), hammer, loss.value(), 0.0, 2000.0, 1.0, 0.0, 2, rate,
      0.4);
  REQUIRE(struck.ok());
  CHECK(std::move(struck).value().renderBlock(block, mmathen::FrameCount{8}).ok());

  const mmathen::BodyModeSpec mode{440.0, 40.0, 1.0};
  const mmathen::HammerBodyDrive hammerBoard{
      std::span<const mmathen::BodyModeSpec>(&mode, 1), 0.01, 1.0, 101325.0, 293.15};
  auto hammered = mmathen::HammeredStringVoice::create(
      course.value(), hammer, loss.value(), 0.0, 2000.0, 1.0, 0.0, 2, 0, rate, hammerBoard);
  REQUIRE(hammered.ok());
  CHECK(std::move(hammered).value().renderBlock(block, mmathen::FrameCount{8}).ok());

  const mmathen::Excitation pluck{mmathen::ExcitationKind::Pluck, 0.3, 1.0e-4};
  auto board = mmathen::SoundboardVoice::create(
      course.value(), pluck, loss.value(), std::span<const mmathen::BodyModeSpec>(&mode, 1),
      0.01, 1.0, 101325.0, 293.15, rate);
  REQUIRE(board.ok());
  CHECK(std::move(board).value().renderBlock(block, mmathen::FrameCount{8}).ok());
}
