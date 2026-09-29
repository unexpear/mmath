// Instruments — violin and viola assembly (docs/48).
//
// The open string is the ideal-string fundamental at the pitch-adapter
// frequency. The nominal density is Guettler's range midpoint, and the
// tension that hits that pitch must sit inside his published tension
// interval. A pluck is the existing modal sum, checked against d'Alembert
// within the truncation bound. The bridge hook is the coupled-string
// limits (rigid end, free end). Bow, hammer, and reed are refused.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/math/comparison.h>
#include <mmathen/music/pitch.h>
#include <mmathen/physics/coupled_string.h>
#include <mmathen/physics/wave1d.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

double waveSpeed(const StringCourse& course) {
  return 2.0 * course.geometry.vibratingLength.value() *
         course.tuning.frequency.value();
}

StringInstrumentSpec violinSpec() {
  return StringInstrumentSpec{
      "violin",
      DistanceMeters{0.356},
      "body",
      DistanceMeters{0.328},
      kGuettlerBowedStringSource,
      std::array<BowedStringRow, kStringInstrumentCourses>{
          BowedStringRow{"G3", 196.0, 2.12, 3.09, 35.0, 51.1},
          BowedStringRow{"D4", 293.7, 0.92, 1.63, 34.3, 60.6},
          BowedStringRow{"A4", 440.0, 0.58, 0.75, 48.3, 62.7},
          BowedStringRow{"E5", 659.3, 0.38, 0.48, 71.4, 90.7},
      },
      "G3",
      "E7",
      196.0,
      2637.0,
      kHyperphysicsPlayingRange};
}
}  // namespace

TEST_CASE("violin and viola construct as four-string assemblies",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const auto violaInstrument = unwrap(viola());
  CHECK(formula(violinInstrument) == "violin G3 D4 A4 E5");
  CHECK(formula(violaInstrument) == "viola C3 G3 D4 A4");
  CHECK(violinInstrument.courses.size() == kStringInstrumentCourses);
  CHECK(violaInstrument.courses.size() == kStringInstrumentCourses);
  CHECK(findCourse(violinInstrument, "E4") == nullptr);
  REQUIRE(findCourse(violinInstrument, "A4") != nullptr);
  CHECK(std::strcmp(findCourse(violinInstrument, "A4")->tuning.label, "A4") ==
        0);
}

TEST_CASE("open strings are the pitch adapter and the ideal-string law",
          "[instruments]") {
  const double fifth = std::pow(2.0, 7.0 / 12.0);
  for (const auto& instrument : {unwrap(violin()), unwrap(viola())}) {
    for (std::size_t i = 0; i < instrument.courses.size(); ++i) {
      const StringCourse& course = instrument.courses[i];
      const auto spelled = unwrap(makePitch(course.tuning.label));
      CHECK(course.tuning.frequency == spelled.frequency);
      const auto open = unwrap(openStringFrequency(course));
      CHECK(nearlyEqual(open, course.tuning.frequency, FrequencyHz{0.0},
                        1e-12));
      CHECK(std::fabs(course.tuning.frequency.value() -
                      course.publishedFrequencyHz) <=
            kTabulatedHertzTolerance);
      const double midpointG =
          0.5 * (course.publishedDensityGPerM.min +
                 course.publishedDensityGPerM.max);
      CHECK(course.material.linearDensity.value() == midpointG * 1.0e-3);
      CHECK(course.tension.value() > course.publishedTensionN.min);
      CHECK(course.tension.value() < course.publishedTensionN.max);
      if (i > 0) {
        const double ratio = course.tuning.frequency.value() /
                             instrument.courses[i - 1].tuning.frequency.value();
        CHECK(nearlyEqual(ratio, fifth, 0.0, 1e-12));
      }
    }
    CHECK(instrument.compass.low == instrument.courses.front().tuning.frequency);
    CHECK(instrument.compass.high.value() >
          instrument.courses.back().tuning.frequency.value());
    CHECK(std::fabs(instrument.compass.low.value() -
                    instrument.compass.publishedLowHz) <=
          kTabulatedHertzTolerance);
    CHECK(std::fabs(instrument.compass.high.value() -
                    instrument.compass.publishedHighHz) <=
          kTabulatedHertzTolerance);
  }
}

TEST_CASE("viola is longer and lower than the violin", "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const auto violaInstrument = unwrap(viola());
  CHECK(violaInstrument.body.length.value() >
        violinInstrument.body.length.value());
  CHECK(violaInstrument.courses[0].geometry.vibratingLength.value() >
        violinInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(violaInstrument.body.length.value() !=
        violaInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(violinInstrument.body.length.value() !=
        violinInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(violaInstrument.compass.low.value() <
        violinInstrument.compass.low.value());
  CHECK(violaInstrument.compass.high.value() <
        violinInstrument.compass.high.value());
  CHECK(violaInstrument.courses.back().tuning.frequency.value() <
        violinInstrument.courses.back().tuning.frequency.value());
  for (const char* label : {"G3", "D4", "A4"}) {
    const StringCourse* violinCourse = findCourse(violinInstrument, label);
    const StringCourse* violaCourse = findCourse(violaInstrument, label);
    REQUIRE(violinCourse != nullptr);
    REQUIRE(violaCourse != nullptr);
    CHECK(violinCourse->tuning.frequency == violaCourse->tuning.frequency);
    CHECK(waveSpeed(*violaCourse) > waveSpeed(*violinCourse));
    CHECK(violaCourse->material.linearDensity.value() !=
          violinCourse->material.linearDensity.value());
  }
}

TEST_CASE("a pluck follows the modal sum; other excitations are refused",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const StringCourse& course = violinInstrument.courses[0];
  const Excitation pluck{ExcitationKind::Pluck, 0.28, 1.0e-4};
  const auto shape = unwrap(realizeExcitation(course, pluck));
  const auto modal = unwrap(ModalSolution::create(shape, 64));
  const double x = 0.28 * course.geometry.vibratingLength.value();
  const TimePointSeconds time{1.3e-3};
  const double summed = unwrap(modal.at(DistanceMeters{x}, time));
  const double exact = unwrap(analyticField(shape, DistanceMeters{x}, time));
  CHECK(std::fabs(summed - exact) <= modal.truncationBound() + 1e-9);
  CHECK(modal.truncationBound() < 0.05 * std::fabs(pluck.amplitudeMeters));
  CHECK(unwrap(modalPluckResponse(course, pluck, 0.28, time, 64)) == summed);
  const double peak = unwrap(
      modalPluckResponse(course, pluck, 0.28, TimePointSeconds{0.0}, 64));
  CHECK(std::fabs(peak - pluck.amplitudeMeters) <=
        modal.truncationBound() + 1e-9);
  CHECK(std::fabs(unwrap(
            modalPluckResponse(course, pluck, 0.0, time, 64))) <=
        modal.truncationBound() + 1e-9);
  CHECK(std::fabs(unwrap(
            modalPluckResponse(course, pluck, 1.0, time, 64))) <=
        modal.truncationBound() + 1e-9);

  for (const ExcitationKind kind :
       {ExcitationKind::Bow, ExcitationKind::Hammer, ExcitationKind::Reed}) {
    Excitation other = pluck;
    other.kind = kind;
    CHECK(realizeExcitation(course, other).error() ==
          QuantityError::DomainError);
  }
  Excitation atNut = pluck;
  atNut.positionFraction = 0.0;
  CHECK(realizeExcitation(course, atNut).error() ==
        QuantityError::InvalidArgument);
  Excitation atBridge = pluck;
  atBridge.positionFraction = 1.0;
  CHECK(realizeExcitation(course, atBridge).error() ==
        QuantityError::InvalidArgument);
  Excitation missing = pluck;
  missing.amplitudeMeters = std::numeric_limits<double>::quiet_NaN();
  CHECK(realizeExcitation(course, missing).error() ==
        QuantityError::InvalidArgument);
  CHECK(modalPluckResponse(course, pluck, 1.5, time, 64).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("the bridge hook recovers the rigid and free ends",
          "[instruments]") {
  for (const auto& instrument : {unwrap(violin()), unwrap(viola())}) {
    const StringCourse& course = instrument.courses[0];
    const double open = unwrap(openStringFrequency(course)).value();
    const auto rigid = unwrap(
        coupleToBridge(course, BridgeOscillator{0.0, 1.0e12, 0.0}));
    const auto freeEnd =
        unwrap(coupleToBridge(course, BridgeOscillator{0.0, 0.0, 0.0}));
    const auto rigidMode = unwrap(coupledModeFrequencies(rigid, 1));
    const auto freeMode = unwrap(coupledModeFrequencies(freeEnd, 1));
    CHECK(nearlyEqual(rigidMode[0].value(), open, 0.0, 1e-4));
    CHECK(nearlyEqual(freeMode[0].value(), 0.5 * open, 0.0, 1e-6));
    CHECK(coupleToBridge(course, BridgeOscillator{-1.0, 0.0, 0.0}).error() ==
          QuantityError::InvalidArgument);
  }
}

TEST_CASE("assembly rejects a table that does not close", "[instruments]") {
  const BowedStringRow violinA{"A4", 440.0, 0.58, 0.75, 48.3, 62.7};
  CHECK(makeStringCourse(DistanceMeters{0.328}, violinA, nullptr).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeStringCourse(DistanceMeters{0.0}, violinA,
                         kGuettlerBowedStringSource)
            .error() == QuantityError::InvalidArgument);
  BowedStringRow untunable = violinA;
  untunable.tensionMinN = 1.0;
  untunable.tensionMaxN = 2.0;
  CHECK(makeStringCourse(DistanceMeters{0.328}, untunable,
                         kGuettlerBowedStringSource)
            .error() == QuantityError::RangeError);
  BowedStringRow wrongPitch = violinA;
  wrongPitch.publishedFrequencyHz = 430.0;
  CHECK(makeStringCourse(DistanceMeters{0.328}, wrongPitch,
                         kGuettlerBowedStringSource)
            .error() == QuantityError::RangeError);
  BowedStringRow notANote = violinA;
  notANote.label = "H4";
  CHECK(makeStringCourse(DistanceMeters{0.328}, notANote,
                         kGuettlerBowedStringSource)
            .error() == QuantityError::InvalidArgument);

  StringInstrumentSpec spec = violinSpec();
  spec.courses[0] = BowedStringRow{"D4", 293.7, 0.92, 1.63, 34.3, 60.6};
  spec.courses[1] = BowedStringRow{"G3", 196.0, 2.12, 3.09, 35.0, 51.1};
  CHECK(makeStringInstrument(spec).error() == QuantityError::RangeError);

  spec = violinSpec();
  spec.compassLowLabel = "D4";
  CHECK(makeStringInstrument(spec).error() == QuantityError::InvalidArgument);

  spec = violinSpec();
  spec.compassHighLabel = "A4";
  spec.compassPublishedHighHz = 440.0;
  CHECK(makeStringInstrument(spec).error() == QuantityError::RangeError);

  spec = violinSpec();
  spec.name = "";
  CHECK(makeStringInstrument(spec).error() == QuantityError::InvalidArgument);
}
