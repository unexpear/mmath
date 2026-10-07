// Instruments — bowed strings, a concert pedal harp, a French double
// harpsichord, Helmholtz bow, and a sum of body modes (docs/48).
//
// The open string is the ideal-string fundamental at the pitch-adapter
// frequency. The nominal density is Guettler's range midpoint, and the
// tension that hits that pitch must sit inside his published tension
// interval. A pluck is the existing modal sum, checked against d'Alembert
// within the truncation bound. The bridge hook is the coupled-string
// limits (rigid end, free end). Hammer and reed are refused. A bow is a
// steady Helmholtz stroke: stick/slip velocity, bridge-force sawtooth,
// and Schelleng's maximum normal force. A sum of body modes filters that
// force. Each resonance, Q, and modal mass is caller-supplied. A hard hammer is
// the infinite-string contact law until the first reflection returns. A
// reed is the quasi-static Bernoulli slit. A fingered stop shortens the
// vibrating length at the open course's tension. A cylindrical bore and
// reed inertia take length, area, mass, and damping as arguments. The
// hard hammer stays refused at the first reflection; the first-image sum
// continues until the second image, and felt compliance is K ξ^p.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include <mmathen/instruments/body.h>
#include <mmathen/physics/impedance.h>
#include <mmathen/physics/medium.h>
#include <mmathen/instruments/bow.h>
#include <mmathen/instruments/cymbal.h>
#include <mmathen/instruments/drum.h>
#include <mmathen/instruments/electric_guitar.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/piano.h>
#include <mmathen/instruments/hammer.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/reed.h>
#include <mmathen/instruments/wound.h>
#include <mmathen/physics/materials.h>
#include <mmathen/signal/inharmonic.h>
#include <mmathen/instruments/cello.h>
#include <mmathen/instruments/double_bass.h>
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

TEST_CASE("violin, viola, cello, and double bass construct as four-string assemblies",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const auto violaInstrument = unwrap(viola());
  const auto celloInstrument = unwrap(cello());
  const auto bassInstrument = unwrap(doubleBass());
  CHECK(formula(violinInstrument) == "violin G3 D4 A4 E5");
  CHECK(formula(violaInstrument) == "viola C3 G3 D4 A4");
  CHECK(formula(celloInstrument) == "cello C2 G2 D3 A3");
  CHECK(formula(bassInstrument) == "double bass E1 A1 D2 G2");
  CHECK(violinInstrument.courses.size() == kStringInstrumentCourses);
  CHECK(violaInstrument.courses.size() == kStringInstrumentCourses);
  CHECK(celloInstrument.courses.size() == kStringInstrumentCourses);
  CHECK(bassInstrument.courses.size() == kStringInstrumentCourses);
  CHECK(findCourse(violinInstrument, "E4") == nullptr);
  REQUIRE(findCourse(violinInstrument, "A4") != nullptr);
  CHECK(std::strcmp(findCourse(violinInstrument, "A4")->tuning.label, "A4") ==
        0);
}

TEST_CASE("open strings are the pitch adapter and the ideal-string law",
          "[instruments]") {
  const double fifth = std::pow(2.0, 7.0 / 12.0);
  const double fourth = std::pow(2.0, 5.0 / 12.0);
  for (const auto& instrument :
       {unwrap(violin()), unwrap(viola()), unwrap(cello()), unwrap(doubleBass())}) {
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
        // Violin, viola, and cello ascend by fifths. Orchestral bass
        // tuning E1 A1 D2 G2 ascends by fourths.
        const double step =
            std::strcmp(instrument.name, "double bass") == 0 ? fourth : fifth;
        CHECK(nearlyEqual(ratio, step, 0.0, 1e-12));
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

TEST_CASE("each larger instrument is longer and lower than the one above it",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const auto violaInstrument = unwrap(viola());
  const auto celloInstrument = unwrap(cello());
  const auto bassInstrument = unwrap(doubleBass());
  CHECK(violaInstrument.body.length.value() >
        violinInstrument.body.length.value());
  CHECK(celloInstrument.body.length.value() >
        violaInstrument.body.length.value());
  CHECK(bassInstrument.body.length.value() >
        celloInstrument.body.length.value());
  CHECK(violaInstrument.courses[0].geometry.vibratingLength.value() >
        violinInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(celloInstrument.courses[0].geometry.vibratingLength.value() >
        violaInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(bassInstrument.courses[0].geometry.vibratingLength.value() >
        celloInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(celloInstrument.body.length.value() !=
        celloInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(bassInstrument.body.length.value() !=
        bassInstrument.courses[0].geometry.vibratingLength.value());
  CHECK(bassInstrument.body.length.value() == 1.110);
  CHECK(bassInstrument.courses[0].geometry.vibratingLength.value() == 1.060);
  CHECK(celloInstrument.compass.low.value() <
        violaInstrument.compass.low.value());
  CHECK(bassInstrument.compass.low.value() <
        celloInstrument.compass.low.value());
  CHECK(celloInstrument.courses.back().tuning.frequency.value() <
        violaInstrument.courses.back().tuning.frequency.value());
  CHECK(bassInstrument.courses.back().tuning.frequency.value() <
        celloInstrument.courses.back().tuning.frequency.value());
  CHECK(celloInstrument.compass.high.value() <
        violaInstrument.compass.high.value());
  CHECK(bassInstrument.compass.high.value() <
        celloInstrument.compass.high.value());
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

  // Bow, hammer, and reed are not initial displacements.
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
  for (const auto& instrument :
       {unwrap(violin()), unwrap(viola()), unwrap(cello()), unwrap(doubleBass())}) {
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

TEST_CASE("a bow stroke is steady Helmholtz motion", "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const StringCourse& course = violinInstrument.courses[2];  // A4
  const double beta = 1.0 / 9.0;
  const double speed = 0.2;
  const double muS = 0.8;
  const double muD = 0.3;
  const double density = course.material.linearDensity.value();
  const double wave = std::sqrt(course.tension.value() / density);
  const double impedance = density * wave;
  const double maxNormal = 2.0 * impedance * speed / (beta * (muS - muD));
  const BowStroke stroke{beta, speed, 0.5 * maxNormal, {muS, muD}};
  const auto motion = unwrap(makeHelmholtzStroke(course, stroke));

  const double open = unwrap(openStringFrequency(course)).value();
  CHECK(nearlyEqual(motion.period, 1.0 / open, 0.0, 1e-12));
  CHECK(nearlyEqual(motion.stickDuration / motion.period, 1.0 - beta, 0.0,
                    1e-15));
  CHECK(nearlyEqual(motion.slipDuration / motion.period, beta, 0.0, 1e-15));
  CHECK(motion.stickVelocity == speed);
  CHECK(nearlyEqual(motion.slipVelocity, -speed * (1.0 - beta) / beta, 0.0,
                    1e-15));
  CHECK(nearlyEqual(motion.slipVelocity - motion.bowSpeed, -speed / beta, 0.0,
                    1e-15));
  CHECK(nearlyEqual(motion.maxNormalForce, maxNormal, 0.0, 1e-12));
  CHECK(motion.slipFrictionForce == muD * stroke.normalForceNewtons);

  const double slope =
      course.tension.value() * speed / (beta * course.geometry.vibratingLength.value());
  CHECK(nearlyEqual(motion.bridgeForceSlope, slope, 0.0, 1e-12));
  CHECK(nearlyEqual(motion.bridgeForceFlyback, -2.0 * impedance * speed / beta,
                    0.0, 1e-12));
  CHECK(nearlyEqual(motion.bridgeForceFlyback, -motion.bridgeForceSlope * motion.period,
                    0.0, 1e-12));

  const double midStick = 0.5 * motion.stickDuration;
  CHECK(helmholtzBowVelocity(motion, midStick) == speed);
  CHECK(helmholtzBowVelocity(motion, motion.stickDuration) ==
        motion.slipVelocity);
  CHECK(helmholtzBowVelocity(motion, -0.5 * motion.slipDuration) ==
        motion.slipVelocity);
  const double dt = 1.0e-8;
  const double finiteDifference =
      (helmholtzBowDisplacement(motion, midStick + dt) -
       helmholtzBowDisplacement(motion, midStick)) /
      dt;
  CHECK(nearlyEqual(finiteDifference, speed, 0.0, 1e-6));
  CHECK(nearlyEqual(helmholtzBowDisplacement(motion, 0.0),
                    helmholtzBowDisplacement(motion, motion.period), 0.0,
                    1e-15));
  const double peakToPeak =
      helmholtzBowDisplacement(motion, motion.stickDuration) -
      helmholtzBowDisplacement(motion, 0.0);
  CHECK(nearlyEqual(peakToPeak, speed * motion.stickDuration, 0.0, 1e-12));

  CHECK(helmholtzBridgeForce(motion, motion.stickDuration) ==
        0.5 * motion.bridgeForceFlyback);
  const double ramp =
      (helmholtzBridgeForce(motion, midStick + dt) -
       helmholtzBridgeForce(motion, midStick)) /
      dt;
  CHECK(nearlyEqual(ramp, motion.bridgeForceSlope, 0.0, 1e-9));
  // The ramp's midpoint is zero, so the sawtooth has no DC offset.
  const double midRamp = (0.5 - motion.beta) * motion.period;
  CHECK(nearlyEqual(helmholtzBridgeForce(motion, midRamp), 0.0, 1e-12, 0.0));

  BowStroke tooHeavy = stroke;
  tooHeavy.normalForceNewtons = motion.maxNormalForce * 1.01;
  CHECK(makeHelmholtzStroke(course, tooHeavy).error() ==
        QuantityError::DomainError);
  BowStroke atLimit = stroke;
  atLimit.normalForceNewtons = motion.maxNormalForce;
  CHECK(makeHelmholtzStroke(course, atLimit).ok());

  BowStroke upBow = stroke;
  upBow.speedMetersPerSecond = -speed;
  const auto upward = unwrap(makeHelmholtzStroke(course, upBow));
  CHECK(upward.stickVelocity == -speed);
  CHECK(upward.slipFrictionForce == -motion.slipFrictionForce);
  CHECK(upward.bridgeForceSlope == -motion.bridgeForceSlope);

  BowStroke onTheBridge = stroke;
  onTheBridge.bridgeFraction = 0.0;
  CHECK(makeHelmholtzStroke(course, onTheBridge).error() ==
        QuantityError::InvalidArgument);
  BowStroke stopped = stroke;
  stopped.speedMetersPerSecond = 0.0;
  CHECK(makeHelmholtzStroke(course, stopped).error() ==
        QuantityError::InvalidArgument);
  BowStroke noDrop = stroke;
  noDrop.friction.slidingCoefficient = muS;
  CHECK(makeHelmholtzStroke(course, noDrop).error() ==
        QuantityError::InvalidArgument);

  // The same β and speed on the cello's longer A string is a different
  // impedance, so Schelleng's maximum force is not the violin's.
  const auto celloInstrument = unwrap(cello());
  const StringCourse* celloA = findCourse(celloInstrument, "A3");
  REQUIRE(celloA != nullptr);
  const auto celloMotion = unwrap(makeHelmholtzStroke(*celloA, stroke));
  CHECK(celloMotion.impedance != motion.impedance);
  CHECK(celloMotion.maxNormalForce != motion.maxNormalForce);
  CHECK(celloMotion.period > motion.period);
}

TEST_CASE("Schelleng's minimum bow force uses a caller bridge resistance",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const StringCourse& course = violinInstrument.courses[2];
  const double beta = 1.0 / 9.0;
  const double speed = 0.2;
  const double muS = 0.8;
  const double muD = 0.3;
  const double density = course.material.linearDensity.value();
  const double wave = std::sqrt(course.tension.value() / density);
  const double impedance = density * wave;
  const double maxNormal = 2.0 * impedance * speed / (beta * (muS - muD));
  const BowStroke stroke{beta, speed, 0.5 * maxNormal, {muS, muD}};
  const auto motion = unwrap(makeHelmholtzStroke(course, stroke));
  // Built from this string's Z. Not a measured bridge resistance.
  const double resistance = 20.0 * impedance / beta;
  const auto minimum = unwrap(schellengMinimumForce(motion, resistance));
  const double direct = impedance * impedance * std::fabs(speed) /
                        (2.0 * resistance * beta * beta * (muS - muD));
  CHECK(nearlyEqual(minimum, direct, 0.0, 1e-12));
  CHECK(nearlyEqual(motion.maxNormalForce / minimum,
                    4.0 * beta * resistance / impedance, 0.0, 1e-12));
  CHECK(minimum < stroke.normalForceNewtons);
  CHECK(makeHelmholtzStroke(course, stroke, resistance).ok());

  const auto doubled =
      unwrap(schellengMinimumForce(motion, 2.0 * resistance));
  CHECK(nearlyEqual(doubled, 0.5 * minimum, 0.0, 1e-12));

  BowStroke upBow = stroke;
  upBow.speedMetersPerSecond = -speed;
  const auto upward = unwrap(makeHelmholtzStroke(course, upBow));
  CHECK(nearlyEqual(unwrap(schellengMinimumForce(upward, resistance)), minimum,
                    0.0, 1e-12));

  BowStroke light = stroke;
  light.normalForceNewtons = minimum * 0.5;
  CHECK(light.normalForceNewtons < motion.maxNormalForce);
  CHECK(makeHelmholtzStroke(course, light).ok());
  CHECK(makeHelmholtzStroke(course, light, resistance).error() ==
        QuantityError::DomainError);
  BowStroke atMinimum = stroke;
  atMinimum.normalForceNewtons = minimum;
  CHECK(makeHelmholtzStroke(course, atMinimum, resistance).ok());

  const auto celloInstrument = unwrap(cello());
  const StringCourse* celloA = findCourse(celloInstrument, "A3");
  REQUIRE(celloA != nullptr);
  const auto celloMotion = unwrap(makeHelmholtzStroke(*celloA, stroke));
  CHECK(unwrap(schellengMinimumForce(celloMotion, resistance)) != minimum);

  CHECK(schellengMinimumForce(motion, 0.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(schellengMinimumForce(motion, -resistance).error() ==
        QuantityError::InvalidArgument);
  HelmholtzStroke blank;
  CHECK(schellengMinimumForce(blank, resistance).error() ==
        QuantityError::InvalidArgument);
  BowStroke tooHeavy = stroke;
  tooHeavy.normalForceNewtons = motion.maxNormalForce * 1.01;
  CHECK(makeHelmholtzStroke(course, tooHeavy, resistance).error() ==
        QuantityError::DomainError);
}

TEST_CASE("one body mode filters the Helmholtz bridge force", "[instruments]") {
  // Arguments, not a published mode of any of the four instruments.
  const auto mode = unwrap(makeBodyResonator(BodyModeSpec{300.0, 20.0, 0.05}));
  CHECK(nearlyEqual(mode.stiffness,
                    mode.modalMassKg * mode.omega0 * mode.omega0, 0.0, 1e-15));
  CHECK(nearlyEqual(mode.damping,
                    mode.modalMassKg * mode.omega0 / mode.qualityFactor, 0.0,
                    1e-15));
  CHECK(nearlyEqual(mode.resonantMobility, 1.0 / mode.damping, 0.0, 1e-15));

  const auto atResonance = unwrap(bodyMobility(mode, mode.resonanceHz));
  CHECK(nearlyEqual(atResonance.real(), mode.resonantMobility, 0.0, 1e-12));
  CHECK(nearlyEqual(atResonance.imag(), 0.0, 0.0, 1e-12));
  const auto atRest = unwrap(bodyMobility(mode, 0.0));
  CHECK(atRest.real() == 0.0);
  CHECK(atRest.imag() == 0.0);
  const auto below = unwrap(bodyMobility(mode, mode.resonanceHz * 0.1));
  CHECK(below.imag() > 0.0);
  CHECK(std::fabs(below.real()) < std::fabs(below.imag()));
  const auto above = unwrap(bodyMobility(mode, mode.resonanceHz * 10.0));
  CHECK(above.imag() < 0.0);
  const double omegaHigh = constants::twoPi * mode.resonanceHz * 10.0;
  const double massAsymptote = 1.0 / (mode.modalMassKg * omegaHigh);
  CHECK(std::fabs(std::abs(above) - massAsymptote) / massAsymptote < 0.02);

  CHECK(makeBodyResonator(BodyModeSpec{0.0, 20.0, 0.05}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeBodyResonator(BodyModeSpec{300.0, 0.0, 0.05}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeBodyResonator(BodyModeSpec{300.0, 20.0, -0.05}).error() ==
        QuantityError::InvalidArgument);
  CHECK(bodyMobility(mode, -1.0).error() == QuantityError::InvalidArgument);

  const double beta = 1.0 / 9.0;
  const double speed = 0.2;
  const BowStroke stroke{beta, speed, 0.05, {0.8, 0.3}};
  const int harmonics = 24;
  const StringInstrument instruments[] = {unwrap(violin()), unwrap(viola()),
                                          unwrap(cello()), unwrap(doubleBass())};
  double previousFlyback = 0.0;
  for (const StringInstrument& instrument : instruments) {
    const auto motion =
        unwrap(makeHelmholtzStroke(instrument.courses[0], stroke));
    REQUIRE(stroke.normalForceNewtons < motion.maxNormalForce);
    const double flyback = motion.bridgeForceFlyback;
    CHECK(flyback != previousFlyback);
    previousFlyback = flyback;

    const auto first = unwrap(bodyVelocityCoefficient(mode, motion, 1));
    const auto mobility =
        unwrap(bodyMobility(mode, 1.0 / motion.period));
    const std::complex<double> expected =
        mobility * std::complex<double>(0.0, -flyback / constants::twoPi);
    CHECK(nearlyEqual(first.real(), expected.real(), 0.0, 1e-12));
    CHECK(nearlyEqual(first.imag(), expected.imag(), 0.0, 1e-12));

    for (double fraction : {0.0, 0.17, 0.5, 0.83}) {
      const double time = fraction * motion.period;
      double slip = time - motion.stickDuration;
      if (slip < 0.0) slip += motion.period;
      double velocity = 0.0;
      double displacement = 0.0;
      for (int n = 1; n <= harmonics; ++n) {
        const double omega =
            constants::twoPi * static_cast<double>(n) / motion.period;
        const double sineForce = flyback / (constants::pi * static_cast<double>(n));
        const double a = mode.stiffness - mode.modalMassKg * omega * omega;
        const double rw = mode.damping * omega;
        const double det = a * a + rw * rw;
        const double cosine = -rw * sineForce / det;
        const double sine = a * sineForce / det;
        const double angle = omega * slip;
        velocity += -cosine * omega * std::sin(angle) +
                    sine * omega * std::cos(angle);
        displacement += cosine * std::cos(angle) + sine * std::sin(angle);
      }
      CHECK(nearlyEqual(unwrap(bodyVelocity(mode, motion, time, harmonics)),
                        velocity, 0.0, 1e-9));
      CHECK(nearlyEqual(
          unwrap(bodyDisplacement(mode, motion, time, harmonics)),
          displacement, 0.0, 1e-9));
      CHECK(nearlyEqual(
          unwrap(bodyVelocity(mode, motion, time + 3.0 * motion.period,
                              harmonics)),
          velocity, 0.0, 1e-12));
    }

    double meanPower = 0.0;
    for (int n = 1; n <= harmonics; ++n) {
      const double sineForce =
          flyback / (constants::pi * static_cast<double>(n));
      const auto mobilityN = unwrap(
          bodyMobility(mode, static_cast<double>(n) / motion.period));
      meanPower += 0.5 * sineForce * sineForce * mobilityN.real();
    }
    constexpr int samples = 2048;
    double damper = 0.0;
    for (int i = 0; i < samples; ++i) {
      const double time = motion.period * static_cast<double>(i) / samples;
      const double velocity =
          unwrap(bodyVelocity(mode, motion, time, harmonics));
      damper += mode.damping * velocity * velocity;
    }
    damper /= static_cast<double>(samples);
    CHECK(nearlyEqual(damper, meanPower, 0.0, 1e-6));
  }

  HelmholtzStroke blank;
  CHECK(bodyVelocity(mode, blank, 0.0, 8).error() ==
        QuantityError::InvalidArgument);
  const auto violinMotion =
      unwrap(makeHelmholtzStroke(unwrap(violin()).courses[0], stroke));
  CHECK(bodyVelocity(mode, violinMotion, 0.0, 0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("body mobility is the sum of caller-supplied modes", "[instruments]") {
  // Round arguments. Not a measured A0, B1, or bridge admittance.
  const BodyModeSpec specs[] = {
      {200.0, 20.0, 0.05},
      {400.0, 30.0, 0.02},
      {800.0, 10.0, 0.01},
  };
  BodyResonator modes[3];
  modes[0].resonanceHz = -1.0;
  const BodyModeSpec rejected[] = {{200.0, 20.0, 0.05}, {400.0, 0.0, 0.02}};
  CHECK(makeBodyResonators(rejected, std::span<BodyResonator>(modes, 2)).error() ==
        QuantityError::InvalidArgument);
  CHECK(modes[0].resonanceHz == -1.0);
  CHECK(makeBodyResonators(std::span<const BodyModeSpec>{},
                           std::span<BodyResonator>{})
            .error() == QuantityError::InvalidArgument);
  BodyResonator tooSmall[2];
  CHECK(makeBodyResonators(specs, tooSmall).error() ==
        QuantityError::InvalidArgument);
  const BodyModeSpec overflowing[] = {{1.0e200, 10.0, 1.0e200}};
  BodyResonator overflowOut[1];
  CHECK(makeBodyResonators(overflowing, overflowOut).error() ==
        QuantityError::RangeError);
  REQUIRE(makeBodyResonators(specs, modes).ok());
  CHECK(makeBodyResonators(specs, modes).value() == 3);

  const double probeHz = 250.0;
  std::complex<double> byHand{0.0, 0.0};
  for (const BodyResonator& mode : modes)
    byHand += unwrap(bodyMobility(mode, probeHz));
  const auto summed = unwrap(bodyMobility(modes, probeHz));
  CHECK(nearlyEqual(summed.real(), byHand.real(), 0.0, 1e-12));
  CHECK(nearlyEqual(summed.imag(), byHand.imag(), 0.0, 1e-12));

  const BodyResonator only[] = {modes[0]};
  const auto one = unwrap(bodyMobility(only, probeHz));
  const auto direct = unwrap(bodyMobility(modes[0], probeHz));
  CHECK(nearlyEqual(one.real(), direct.real(), 0.0, 1e-15));
  CHECK(nearlyEqual(one.imag(), direct.imag(), 0.0, 1e-15));

  const BodyResonator twins[] = {modes[0], modes[0]};
  const auto doubled = unwrap(bodyMobility(twins, modes[0].resonanceHz));
  const auto twinTerm = unwrap(bodyMobility(modes[0], modes[0].resonanceHz));
  CHECK(nearlyEqual(doubled.real(), 2.0 * twinTerm.real(), 0.0, 1e-12));
  CHECK(nearlyEqual(doubled.imag(), 0.0, 1e-12, 1e-12));

  const auto atFirst = unwrap(bodyMobility(modes, modes[0].resonanceHz));
  const auto others =
      unwrap(bodyMobility(modes[1], modes[0].resonanceHz)) +
      unwrap(bodyMobility(modes[2], modes[0].resonanceHz));
  CHECK(nearlyEqual(atFirst.real(), twinTerm.real() + others.real(), 0.0,
                    1e-12));
  CHECK(nearlyEqual(atFirst.imag(), others.imag(), 0.0, 1e-12));
  CHECK(atFirst.imag() != 0.0);

  const auto atRest = unwrap(bodyMobility(modes, 0.0));
  CHECK(atRest.real() == 0.0);
  CHECK(atRest.imag() == 0.0);
  const auto below = unwrap(bodyMobility(modes, 20.0));
  CHECK(below.imag() > 0.0);
  const auto above = unwrap(bodyMobility(modes, 8000.0));
  CHECK(above.imag() < 0.0);
  const double omegaHigh = constants::twoPi * 8000.0;
  double massAsymptote = 0.0;
  for (const BodyResonator& mode : modes)
    massAsymptote += 1.0 / (mode.modalMassKg * omegaHigh);
  CHECK(std::fabs(std::abs(above) - massAsymptote) / massAsymptote < 0.02);

  BodyModeSpec heavySpecs[3];
  for (int i = 0; i < 3; ++i) {
    heavySpecs[i] = specs[i];
    heavySpecs[i].modalMassKg *= 2.0;
  }
  BodyResonator heavy[3];
  REQUIRE(makeBodyResonators(heavySpecs, heavy).ok());
  const auto lightY = unwrap(bodyMobility(modes, probeHz));
  const auto heavyY = unwrap(bodyMobility(heavy, probeHz));
  CHECK(nearlyEqual(lightY.real(), 2.0 * heavyY.real(), 0.0, 1e-12));
  CHECK(nearlyEqual(lightY.imag(), 2.0 * heavyY.imag(), 0.0, 1e-12));

  CHECK(bodyMobility(std::span<const BodyResonator>{}, probeHz).error() ==
        QuantityError::InvalidArgument);
  CHECK(bodyMobility(modes, -1.0).error() == QuantityError::InvalidArgument);

  const double beta = 1.0 / 9.0;
  const BowStroke stroke{beta, 0.2, 0.05, {0.8, 0.3}};
  const int harmonics = 16;
  const auto violinMotion =
      unwrap(makeHelmholtzStroke(unwrap(violin()).courses[0], stroke));
  const auto celloMotion =
      unwrap(makeHelmholtzStroke(unwrap(cello()).courses[0], stroke));
  const auto violinCoefficient =
      unwrap(bodyVelocityCoefficient(modes, violinMotion, 1));
  const auto celloCoefficient =
      unwrap(bodyVelocityCoefficient(modes, celloMotion, 1));
  CHECK(violinCoefficient != celloCoefficient);
  std::complex<double> coefficientSum{0.0, 0.0};
  for (const BodyResonator& mode : modes)
    coefficientSum += unwrap(bodyVelocityCoefficient(mode, violinMotion, 1));
  CHECK(nearlyEqual(violinCoefficient.real(), coefficientSum.real(), 0.0,
                    1e-12));
  CHECK(nearlyEqual(violinCoefficient.imag(), coefficientSum.imag(), 0.0,
                    1e-12));

  for (double fraction : {0.0, 0.2, 0.6}) {
    const double time = fraction * violinMotion.period;
    double velocity = 0.0;
    double displacement = 0.0;
    for (const BodyResonator& mode : modes) {
      velocity += unwrap(bodyVelocity(mode, violinMotion, time, harmonics));
      displacement +=
          unwrap(bodyDisplacement(mode, violinMotion, time, harmonics));
    }
    CHECK(nearlyEqual(unwrap(bodyVelocity(modes, violinMotion, time, harmonics)),
                      velocity, 0.0, 1e-9));
    CHECK(nearlyEqual(
        unwrap(bodyDisplacement(modes, violinMotion, time, harmonics)),
        displacement, 0.0, 1e-9));
    CHECK(nearlyEqual(
        unwrap(bodyVelocity(only, violinMotion, time, harmonics)),
        unwrap(bodyVelocity(modes[0], violinMotion, time, harmonics)), 0.0,
        1e-12));
  }

  double meanPower = 0.0;
  for (int n = 1; n <= harmonics; ++n) {
    const double sineForce =
        violinMotion.bridgeForceFlyback /
        (constants::pi * static_cast<double>(n));
    const auto mobilityN = unwrap(
        bodyMobility(modes, static_cast<double>(n) / violinMotion.period));
    meanPower += 0.5 * sineForce * sineForce * mobilityN.real();
  }
  constexpr int samples = 1024;
  double damper = 0.0;
  for (int i = 0; i < samples; ++i) {
    const double time =
        violinMotion.period * static_cast<double>(i) / samples;
    for (const BodyResonator& mode : modes) {
      const double velocity =
          unwrap(bodyVelocity(mode, violinMotion, time, harmonics));
      damper += mode.damping * velocity * velocity;
    }
  }
  damper /= static_cast<double>(samples);
  CHECK(nearlyEqual(damper, meanPower, 0.0, 1e-6));

  HelmholtzStroke blank;
  CHECK(bodyVelocity(modes, blank, 0.0, 8).error() ==
        QuantityError::InvalidArgument);
  CHECK(bodyVelocity(std::span<const BodyResonator>{}, violinMotion, 0.0, 8)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("a cavity Helmholtz frequency uses caller area, volume, and neck",
          "[instruments]") {
  // Synthetic cavity. Not a measured violin, viola, cello, or bass.
  const double area = 0.04;
  const double volume = 0.2;
  const double neck = 0.1;
  const double pressure = 1.0e5;
  const double temperature = 300.0;
  const CavityHelmholtzSpec spec{area, volume, neck, pressure, temperature};
  const auto cavity = unwrap(makeCavityHelmholtz(spec));
  CHECK(cavity.fHoleAreaM2 == area);
  CHECK(cavity.volumeM3 == volume);
  CHECK(cavity.effectiveNeckMeters == neck);

  const double violinBody = unwrap(violin()).body.length.value();
  const double celloBody = unwrap(cello()).body.length.value();
  CHECK(cavity.effectiveNeckMeters != violinBody);
  CHECK(cavity.volumeM3 != violinBody);
  CHECK(cavity.fHoleAreaM2 != celloBody);

  const double rho =
      unwrap(airDensity(PressurePascals{pressure}, TemperatureKelvin{temperature}))
          .value();
  const double c =
      unwrap(speedOfSound(unwrap(makeAir(TemperatureKelvin{temperature})))).value();
  CHECK(cavity.airDensityKgPerM3 == rho);
  CHECK(cavity.soundSpeedMetersPerSecond == c);
  const double acousticMass = rho * neck / area;
  const double acousticCompliance = volume / (rho * c * c);
  CHECK(nearlyEqual(cavity.acousticMass, acousticMass, 0.0, 1e-15));
  CHECK(nearlyEqual(cavity.acousticCompliance, acousticCompliance, 0.0, 1e-15));
  const double fromNeck =
      c / constants::twoPi * std::sqrt(area / (volume * neck));
  const double fromLumped =
      1.0 / (constants::twoPi *
             std::sqrt(cavity.acousticMass * cavity.acousticCompliance));
  CHECK(nearlyEqual(cavity.frequencyHz, fromNeck, 0.0, 1e-12));
  CHECK(nearlyEqual(cavity.frequencyHz, fromLumped, 0.0, 1e-12));

  const auto wider = unwrap(makeCavityHelmholtz(
      CavityHelmholtzSpec{2.0 * area, volume, neck, pressure, temperature}));
  CHECK(nearlyEqual(wider.frequencyHz, cavity.frequencyHz * std::sqrt(2.0), 0.0,
                    1e-12));
  CHECK(nearlyEqual(wider.acousticMass, 0.5 * cavity.acousticMass, 0.0, 1e-12));
  CHECK(nearlyEqual(wider.acousticCompliance, cavity.acousticCompliance, 0.0,
                    1e-15));

  const auto larger = unwrap(makeCavityHelmholtz(
      CavityHelmholtzSpec{area, 2.0 * volume, neck, pressure, temperature}));
  CHECK(nearlyEqual(larger.frequencyHz, cavity.frequencyHz / std::sqrt(2.0), 0.0,
                    1e-12));
  CHECK(nearlyEqual(larger.acousticCompliance, 2.0 * cavity.acousticCompliance,
                    0.0, 1e-12));
  CHECK(nearlyEqual(larger.acousticMass, cavity.acousticMass, 0.0, 1e-15));

  const auto longer = unwrap(makeCavityHelmholtz(
      CavityHelmholtzSpec{area, volume, 2.0 * neck, pressure, temperature}));
  CHECK(nearlyEqual(longer.frequencyHz, cavity.frequencyHz / std::sqrt(2.0), 0.0,
                    1e-12));
  CHECK(nearlyEqual(longer.acousticMass, 2.0 * cavity.acousticMass, 0.0, 1e-12));

  const auto denserAir = unwrap(makeCavityHelmholtz(
      CavityHelmholtzSpec{area, volume, neck, 2.0 * pressure, temperature}));
  CHECK(nearlyEqual(denserAir.frequencyHz, cavity.frequencyHz, 0.0, 1e-12));
  CHECK(nearlyEqual(denserAir.acousticMass, 2.0 * cavity.acousticMass, 0.0,
                    1e-12));
  CHECK(nearlyEqual(denserAir.acousticCompliance, 0.5 * cavity.acousticCompliance,
                    0.0, 1e-12));
  CHECK(denserAir.soundSpeedMetersPerSecond == cavity.soundSpeedMetersPerSecond);

  const auto hotter = unwrap(makeCavityHelmholtz(
      CavityHelmholtzSpec{area, volume, neck, pressure, 4.0 * temperature}));
  CHECK(nearlyEqual(hotter.frequencyHz, 2.0 * cavity.frequencyHz, 0.0, 1e-12));
  CHECK(nearlyEqual(hotter.soundSpeedMetersPerSecond,
                    2.0 * cavity.soundSpeedMetersPerSecond, 0.0, 1e-12));
  CHECK(nearlyEqual(hotter.acousticMass, 0.25 * cavity.acousticMass, 0.0, 1e-12));
  CHECK(nearlyEqual(hotter.acousticCompliance, cavity.acousticCompliance, 0.0,
                    1e-12));

  CHECK(makeCavityHelmholtz(CavityHelmholtzSpec{0.0, volume, neck, pressure,
                                                temperature})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeCavityHelmholtz(CavityHelmholtzSpec{area, -volume, neck, pressure,
                                                temperature})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeCavityHelmholtz(CavityHelmholtzSpec{area, volume,
                                                std::numeric_limits<double>::quiet_NaN(),
                                                pressure, temperature})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeCavityHelmholtz(CavityHelmholtzSpec{area, volume, neck, 0.0,
                                                temperature})
            .error() == QuantityError::DomainError);
  CHECK(makeCavityHelmholtz(CavityHelmholtzSpec{area, volume, neck, pressure, 0.0})
            .error() == QuantityError::DomainError);
  CHECK(makeCavityHelmholtz(CavityHelmholtzSpec{1.0e-300, 1.0, 1.0e300, pressure,
                                                temperature})
            .error() == QuantityError::RangeError);
}

TEST_CASE("two body modes couple through a caller mutual stiffness",
          "[instruments]") {
  // Arguments, not a measured A0 or B1.
  const auto low = unwrap(makeBodyResonator(BodyModeSpec{200.0, 20.0, 0.05}));
  const auto high = unwrap(makeBodyResonator(BodyModeSpec{350.0, 25.0, 0.02}));
  const auto uncoupled = unwrap(coupleBodyModes(low, high, 0.0));
  CHECK(nearlyEqual(uncoupled.lowerHz, low.resonanceHz, 0.0, 1e-12));
  CHECK(nearlyEqual(uncoupled.upperHz, high.resonanceHz, 0.0, 1e-12));
  const auto swapped = unwrap(coupleBodyModes(high, low, 0.0));
  CHECK(nearlyEqual(swapped.lowerHz, uncoupled.lowerHz, 0.0, 1e-12));
  CHECK(nearlyEqual(swapped.upperHz, uncoupled.upperHz, 0.0, 1e-12));

  const double mutual = 2.0e4;
  const auto split = unwrap(coupleBodyModes(low, high, mutual));
  const auto flipped = unwrap(coupleBodyModes(low, high, -mutual));
  CHECK(nearlyEqual(flipped.lowerHz, split.lowerHz, 0.0, 1e-12));
  CHECK(nearlyEqual(flipped.upperHz, split.upperHz, 0.0, 1e-12));
  CHECK(split.lowerHz < low.resonanceHz);
  CHECK(split.upperHz > high.resonanceHz);
  const double uncoupledSum =
      low.omega0 * low.omega0 + high.omega0 * high.omega0;
  const double coupledSum =
      std::pow(constants::twoPi * split.lowerHz, 2.0) +
      std::pow(constants::twoPi * split.upperHz, 2.0);
  CHECK(nearlyEqual(coupledSum, uncoupledSum, 0.0, 1e-12));

  const auto twin = unwrap(makeBodyResonator(BodyModeSpec{200.0, 20.0, 0.05}));
  const double within = 0.25 * twin.stiffness;
  const auto twins = unwrap(coupleBodyModes(low, twin, within));
  const double raised =
      std::sqrt(twin.omega0 * twin.omega0 + within / twin.modalMassKg) /
      constants::twoPi;
  const double lowered =
      std::sqrt(twin.omega0 * twin.omega0 - within / twin.modalMassKg) /
      constants::twoPi;
  CHECK(nearlyEqual(twins.upperHz, raised, 0.0, 1e-12));
  CHECK(nearlyEqual(twins.lowerHz, lowered, 0.0, 1e-12));
  CHECK(twins.lowerHz < twin.resonanceHz);
  CHECK(twins.upperHz > twin.resonanceHz);

  const double limit = std::sqrt(low.stiffness * high.stiffness);
  CHECK(coupleBodyModes(low, high, limit).error() == QuantityError::DomainError);
  CHECK(coupleBodyModes(low, high, limit * 1.01).error() ==
        QuantityError::DomainError);
  CHECK(coupleBodyModes(low, high, std::numeric_limits<double>::quiet_NaN())
            .error() == QuantityError::InvalidArgument);
  BodyResonator blank;
  CHECK(coupleBodyModes(low, blank, 0.0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("far-field pressure is a compact monopole with caller area or volume velocity",
          "[instruments]") {
  // Synthetic source. Not a measured violin, viola, cello, or bass area.
  const double pressure = 1.0e5;
  const double temperature = 300.0;
  const double distance = 3.0;
  const double frequency = 220.0;
  const std::complex<double> volume{1.5e-4, 0.0};
  const auto field = unwrap(compactMonopolePressure(
      volume, distance, frequency, pressure, temperature));
  CHECK(field.distanceMeters == distance);
  CHECK(field.distanceMeters != unwrap(violin()).body.length.value());
  CHECK(field.distanceMeters != unwrap(cello()).body.length.value());

  const double rho =
      unwrap(airDensity(PressurePascals{pressure}, TemperatureKelvin{temperature}))
          .value();
  const double c =
      unwrap(speedOfSound(unwrap(makeAir(TemperatureKelvin{temperature})))).value();
  CHECK(field.airDensityKgPerM3 == rho);
  CHECK(field.soundSpeedMetersPerSecond == c);
  const double omega = constants::twoPi * frequency;
  const double wave = omega / c;
  CHECK(nearlyEqual(field.waveNumber, wave, 0.0, 1e-12));
  const double scale = omega * rho / (4.0 * constants::pi * distance);
  const double kr = wave * distance;
  const std::complex<double> expected{scale * std::sin(kr) * volume.real(),
                                      scale * std::cos(kr) * volume.real()};
  CHECK(nearlyEqual(field.pressure.real(), expected.real(), 0.0, 1e-12));
  CHECK(nearlyEqual(field.pressure.imag(), expected.imag(), 0.0, 1e-12));
  CHECK(nearlyEqual(std::abs(field.pressure), scale * std::abs(volume), 0.0,
                    1e-12));

  const auto farther = unwrap(compactMonopolePressure(
      volume, 2.0 * distance, frequency, pressure, temperature));
  CHECK(nearlyEqual(std::abs(farther.pressure), 0.5 * std::abs(field.pressure),
                    0.0, 1e-12));
  const auto higher = unwrap(compactMonopolePressure(
      volume, distance, 2.0 * frequency, pressure, temperature));
  CHECK(nearlyEqual(std::abs(higher.pressure), 2.0 * std::abs(field.pressure),
                    0.0, 1e-12));

  const double area = 0.02;
  const std::complex<double> normal{volume.real() / area, 0.0};
  const auto fromArea = unwrap(compactMonopolePressure(
      area, normal, distance, frequency, pressure, temperature));
  CHECK(nearlyEqual(fromArea.pressure.real(), field.pressure.real(), 0.0,
                    1e-12));
  CHECK(nearlyEqual(fromArea.pressure.imag(), field.pressure.imag(), 0.0,
                    1e-12));
  const auto wider = unwrap(compactMonopolePressure(
      2.0 * area, normal, distance, frequency, pressure, temperature));
  CHECK(nearlyEqual(std::abs(wider.pressure), 2.0 * std::abs(field.pressure),
                    0.0, 1e-12));

  const auto denser = unwrap(compactMonopolePressure(
      volume, distance, frequency, 2.0 * pressure, temperature));
  CHECK(nearlyEqual(denser.pressure.real(), 2.0 * field.pressure.real(), 0.0,
                    1e-12));
  CHECK(nearlyEqual(denser.pressure.imag(), 2.0 * field.pressure.imag(), 0.0,
                    1e-12));
  CHECK(denser.waveNumber == field.waveNumber);

  const auto hotter = unwrap(compactMonopolePressure(
      volume, distance, frequency, pressure, 4.0 * temperature));
  CHECK(nearlyEqual(std::abs(hotter.pressure), 0.25 * std::abs(field.pressure),
                    0.0, 1e-12));
  CHECK(nearlyEqual(hotter.soundSpeedMetersPerSecond,
                    2.0 * field.soundSpeedMetersPerSecond, 0.0, 1e-12));
  CHECK(nearlyEqual(hotter.waveNumber, 0.5 * field.waveNumber, 0.0, 1e-12));

  const auto steady = unwrap(
      compactMonopolePressure(volume, distance, 0.0, pressure, temperature));
  CHECK(steady.pressure.real() == 0.0);
  CHECK(steady.pressure.imag() == 0.0);

  CHECK(compactMonopolePressure(volume, 0.0, frequency, pressure, temperature)
            .error() == QuantityError::InvalidArgument);
  CHECK(compactMonopolePressure(volume, distance, -frequency, pressure,
                                temperature)
            .error() == QuantityError::InvalidArgument);
  CHECK(compactMonopolePressure(0.0, normal, distance, frequency, pressure,
                                temperature)
            .error() == QuantityError::InvalidArgument);
  CHECK(compactMonopolePressure(volume, distance, frequency, 0.0, temperature)
            .error() == QuantityError::DomainError);
}

TEST_CASE("a hard hammer follows the infinite-string law until the reflection",
          "[instruments]") {
  // Mass and speed are arguments, not a published piano hammer.
  const auto violinInstrument = unwrap(violin());
  const StringCourse& course = violinInstrument.courses[2];  // A4
  const double fraction = 0.125;
  const double mass = 0.012;
  const double speed = 1.5;
  const auto blow =
      unwrap(makeHardHammerBlow(course, HammerStrike{fraction, mass, speed}));

  const double length = course.geometry.vibratingLength.value();
  const double density = course.material.linearDensity.value();
  const double wave = std::sqrt(course.tension.value() / density);
  const double impedance = density * wave;
  const double position = fraction * length;
  const double nearer = std::min(position, length - position);
  const double firstReflection = 2.0 * nearer / wave;
  CHECK(nearlyEqual(blow.impedance, impedance, 0.0, 1e-12));
  CHECK(nearlyEqual(blow.timeConstant, mass / (2.0 * impedance), 0.0, 1e-12));
  CHECK(nearlyEqual(blow.firstReflectionTime, firstReflection, 0.0, 1e-12));
  CHECK(nearlyEqual(blow.initialForce, 2.0 * impedance * speed, 0.0, 1e-12));
  CHECK(blow.positionMeters == position);

  const auto centered = unwrap(
      makeHardHammerBlow(course, HammerStrike{0.5, mass, speed}));
  const double period = 2.0 * length / wave;
  CHECK(nearlyEqual(centered.firstReflectionTime, 0.5 * period, 0.0, 1e-12));
  CHECK(blow.firstReflectionTime < centered.firstReflectionTime);

  const double time = 0.25 * blow.firstReflectionTime;
  const double decay = std::exp(-time / blow.timeConstant);
  const double velocity = speed * decay;
  CHECK(nearlyEqual(unwrap(hardHammerVelocity(blow, time)), velocity, 0.0,
                    1e-12));
  CHECK(nearlyEqual(unwrap(hardHammerForce(blow, time)),
                    2.0 * impedance * velocity, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(hardHammerForce(blow, 0.0)), blow.initialForce, 0.0,
                    1e-15));
  CHECK(unwrap(hardHammerDisplacement(blow, 0.0)) == 0.0);
  CHECK(nearlyEqual(unwrap(hardHammerDisplacement(blow, time)),
                    speed * blow.timeConstant * (1.0 - decay), 0.0, 1e-12));
  const double kineticLoss =
      0.5 * mass * (speed * speed - velocity * velocity);
  CHECK(nearlyEqual(kineticLoss,
                    0.5 * mass * speed * speed * (1.0 - decay * decay), 0.0,
                    1e-12));
  CHECK(nearlyEqual(mass * (speed - velocity), mass * speed * (1.0 - decay),
                    0.0, 1e-12));

  const double observe = 0.5 * length;
  const double delay = std::fabs(observe - position) / wave;
  const double reflectionAtObserve =
      std::min((observe + position) / wave,
               (2.0 * length - observe - position) / wave);
  REQUIRE(delay < reflectionAtObserve);
  CHECK(unwrap(hardHammerStringDisplacement(blow, observe, delay * 0.5)) ==
        0.0);
  CHECK(unwrap(hardHammerStringVelocity(blow, observe, delay * 0.5)) == 0.0);
  const double after = delay + 0.2 * (reflectionAtObserve - delay);
  CHECK(nearlyEqual(unwrap(hardHammerStringDisplacement(blow, observe, after)),
                    unwrap(hardHammerDisplacement(blow, after - delay)), 0.0,
                    1e-12));
  CHECK(nearlyEqual(unwrap(hardHammerStringVelocity(blow, observe, after)),
                    unwrap(hardHammerVelocity(blow, after - delay)), 0.0,
                    1e-12));
  CHECK(nearlyEqual(
      unwrap(hardHammerStringDisplacement(blow, position, time)),
      unwrap(hardHammerDisplacement(blow, time)), 0.0, 1e-15));

  CHECK(hardHammerForce(blow, blow.firstReflectionTime).error() ==
        QuantityError::DomainError);
  CHECK(hardHammerVelocity(blow, blow.firstReflectionTime + 1.0e-6).error() ==
        QuantityError::DomainError);
  CHECK(hardHammerDisplacement(blow, -1.0e-9).error() ==
        QuantityError::InvalidArgument);
  CHECK(hardHammerStringDisplacement(blow, observe, reflectionAtObserve)
            .error() == QuantityError::DomainError);
  CHECK(hardHammerStringDisplacement(blow, 0.0, time).error() ==
        QuantityError::InvalidArgument);
  CHECK(hardHammerStringVelocity(blow, length, time).error() ==
        QuantityError::InvalidArgument);

  CHECK(makeHardHammerBlow(course, HammerStrike{0.0, mass, speed}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeHardHammerBlow(course, HammerStrike{1.0, mass, speed}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeHardHammerBlow(course, HammerStrike{fraction, 0.0, speed}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeHardHammerBlow(course, HammerStrike{fraction, mass, 0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeHardHammerBlow(course, HammerStrike{fraction, mass, -speed})
            .error() == QuantityError::InvalidArgument);

  // The same mass and speed on the bass's heavier string is a different
  // impedance, so the contact force and the time constant are not the
  // violin's.
  const auto bassInstrument = unwrap(doubleBass());
  const auto bassBlow = unwrap(makeHardHammerBlow(
      bassInstrument.courses[0], HammerStrike{fraction, mass, speed}));
  CHECK(bassBlow.impedance != blow.impedance);
  CHECK(bassBlow.initialForce != blow.initialForce);
  CHECK(bassBlow.timeConstant != blow.timeConstant);
  CHECK(bassBlow.firstReflectionTime != blow.firstReflectionTime);
}

TEST_CASE("a reed is the quasi-static Bernoulli slit", "[instruments]") {
  // Area and closure pressure are arguments, not a measured mouthpiece,
  // and not a property of the violin, viola, cello, or double bass.
  const double area = 1.5e-5;
  const double closure = 6.0e3;
  const double airPressure = 101325.0;
  const double temperature = 293.15;
  const auto reed = unwrap(makeReedCharacteristic(
      ReedSpec{area, closure, airPressure, temperature}));
  const double density =
      unwrap(airDensity(PressurePascals{airPressure},
                        TemperatureKelvin{temperature}))
          .value();
  CHECK(reed.airDensityKgPerM3 == density);
  CHECK(reed.equilibriumAreaM2 == area);
  CHECK(reed.closurePressurePa == closure);

  const double peak =
      (2.0 / 3.0) * area * std::sqrt(2.0 * closure / (3.0 * density));
  CHECK(nearlyEqual(reed.maximumFlowM3PerS, peak, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(reedVolumeFlow(reed, closure / 3.0)), peak, 0.0,
                    1e-12));
  CHECK(unwrap(reedVolumeFlow(reed, closure / 3.0)) >
        unwrap(reedVolumeFlow(reed, 0.5 * closure / 3.0)));
  CHECK(unwrap(reedVolumeFlow(reed, closure / 3.0)) >
        unwrap(reedVolumeFlow(reed, 0.5 * closure)));

  const double difference = 0.25 * closure;
  const double opening = area * (1.0 - difference / closure);
  CHECK(nearlyEqual(unwrap(reedOpeningArea(reed, difference)), opening, 0.0,
                    1e-15));
  CHECK(nearlyEqual(unwrap(reedVolumeFlow(reed, difference)),
                    opening * std::sqrt(2.0 * difference / density), 0.0,
                    1e-12));
  CHECK(unwrap(reedOpeningArea(reed, 0.0)) == area);
  CHECK(unwrap(reedVolumeFlow(reed, 0.0)) == 0.0);
  CHECK(unwrap(reedOpeningArea(reed, closure)) == 0.0);
  CHECK(unwrap(reedVolumeFlow(reed, closure)) == 0.0);
  CHECK(unwrap(reedOpeningArea(reed, 2.0 * closure)) == 0.0);
  CHECK(unwrap(reedVolumeFlow(reed, 2.0 * closure)) == 0.0);

  CHECK(reedVolumeFlow(reed, -1.0).error() == QuantityError::DomainError);
  CHECK(reedOpeningArea(reed, -closure).error() == QuantityError::DomainError);
  CHECK(makeReedCharacteristic(ReedSpec{0.0, closure, airPressure, temperature})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeReedCharacteristic(ReedSpec{area, -closure, airPressure, temperature})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeReedCharacteristic(ReedSpec{area, closure, airPressure, 0.0})
            .error() == QuantityError::DomainError);
  CHECK(makeReedCharacteristic(ReedSpec{area, closure, -airPressure, temperature})
            .error() == QuantityError::DomainError);

  // A different air state is a different density, so the same slit does
  // not pass the same peak flow.
  const auto warmer = unwrap(makeReedCharacteristic(
      ReedSpec{area, closure, airPressure, temperature + 20.0}));
  CHECK(warmer.airDensityKgPerM3 < reed.airDensityKgPerM3);
  CHECK(warmer.maximumFlowM3PerS != reed.maximumFlowM3PerS);
}

TEST_CASE("a fingered stop shortens the vibrating length and keeps the tension",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const auto violaInstrument = unwrap(viola());
  const StringCourse& violinOpen = violinInstrument.courses[2];  // A4
  const StringCourse& violaOpen = violaInstrument.courses[3];    // A4
  const double alpha = 0.25;
  const auto violinStopped = unwrap(fingerStop(violinOpen, alpha));
  const auto violaStopped = unwrap(fingerStop(violaOpen, alpha));

  CHECK(violinStopped.geometry.vibratingLength.value() ==
        (1.0 - alpha) * violinOpen.geometry.vibratingLength.value());
  CHECK(violinStopped.material.linearDensity.value() ==
        violinOpen.material.linearDensity.value());
  CHECK(violinStopped.tension.value() == violinOpen.tension.value());
  CHECK(violinStopped.publishedTensionN.min == violinOpen.publishedTensionN.min);
  CHECK(violinStopped.publishedTensionN.max == violinOpen.publishedTensionN.max);
  CHECK(violinStopped.tuning.label == nullptr);
  CHECK(violinStopped.frequencyPrinted == false);
  CHECK(violinStopped.publishedFrequencyHz == 0.0);
  CHECK(violinStopped.source == kFingerStopSource);
  CHECK(violinOpen.tuning.label != nullptr);
  CHECK(std::strcmp(violinOpen.tuning.label, "A4") == 0);
  CHECK(violinOpen.geometry.vibratingLength.value() == 0.328);

  const double stoppedHz = violinStopped.tuning.frequency.value();
  const double openHz = violinOpen.tuning.frequency.value();
  const double ratio = 1.0 / (1.0 - alpha);
  CHECK(nearlyEqual(stoppedHz, openHz * ratio, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(openStringFrequency(violinStopped)).value(), stoppedHz,
                    0.0, 1e-12));
  const double length = violinStopped.geometry.vibratingLength.value();
  const double density = violinStopped.material.linearDensity.value();
  const double tension = density * 4.0 * length * length * stoppedHz * stoppedHz;
  CHECK(nearlyEqual(violinStopped.tension.value(), tension, 0.0, 1e-9));
  CHECK(nearlyEqual(std::sqrt(violinStopped.tension.value() / density),
                    std::sqrt(violinOpen.tension.value() /
                              violinOpen.material.linearDensity.value()),
                    0.0, 1e-12));

  const double violaRatio = violaStopped.tuning.frequency.value() /
                            violaOpen.tuning.frequency.value();
  CHECK(nearlyEqual(violaRatio, ratio, 0.0, 1e-12));
  // Both open courses are A4, so the same fraction is the same hertz.
  // The vibrating lengths are not the same.
  CHECK(nearlyEqual(violaStopped.tuning.frequency.value(), stoppedHz, 0.0,
                    1e-12));
  CHECK(violaStopped.geometry.vibratingLength.value() !=
        violinStopped.geometry.vibratingLength.value());
  const auto violinLow = unwrap(fingerStop(violinInstrument.courses[0], alpha));
  const auto violaLow = unwrap(fingerStop(violaInstrument.courses[0], alpha));
  CHECK(nearlyEqual(violinLow.tuning.frequency.value() /
                        violinInstrument.courses[0].tuning.frequency.value(),
                    ratio, 0.0, 1e-12));
  CHECK(nearlyEqual(violaLow.tuning.frequency.value() /
                        violaInstrument.courses[0].tuning.frequency.value(),
                    ratio, 0.0, 1e-12));
  CHECK(violinLow.tuning.frequency.value() != violaLow.tuning.frequency.value());
  CHECK(violinLow.geometry.vibratingLength.value() !=
        violaLow.geometry.vibratingLength.value());

  const Excitation pluck{ExcitationKind::Pluck, 0.2, 1.0e-3};
  CHECK(realizeExcitation(violinStopped, pluck).ok());
  CHECK(realizeExcitation(violinStopped,
                          Excitation{ExcitationKind::Bow, 0.2, 1.0e-3})
            .error() == QuantityError::DomainError);
  CHECK(makeHardHammerBlow(violinStopped, HammerStrike{0.2, 0.01, 1.0}).ok());

  CHECK(fingerStop(violinOpen, 0.0).error() == QuantityError::InvalidArgument);
  CHECK(fingerStop(violinOpen, 1.0).error() == QuantityError::InvalidArgument);
  CHECK(fingerStop(violinOpen, -0.1).error() == QuantityError::InvalidArgument);
  CHECK(fingerStop(violinOpen, 1.1).error() == QuantityError::InvalidArgument);
  CHECK(fingerStop(violinOpen, std::numeric_limits<double>::quiet_NaN()).error() ==
        QuantityError::InvalidArgument);

  StringCourse overtensioned = violinOpen;
  overtensioned.tension =
      TensionNewtons{violinOpen.publishedTensionN.max + 1.0};
  CHECK(fingerStop(overtensioned, alpha).error() == QuantityError::RangeError);
  CHECK(violin().value().courses[2].tuning.label != nullptr);
  CHECK(std::strcmp(violin().value().courses[2].tuning.label, "A4") == 0);
}

TEST_CASE("a reed bore is an ideal cylinder and inertia follows the static line",
          "[instruments]") {
  const double length = 0.5;
  const double area = 2.0e-4;
  const double airPressure = 101325.0;
  const double temperature = 293.15;
  const auto open = unwrap(makeReedBore(ReedBoreSpec{
      length, area, airPressure, temperature, BoreFarEnd::Open}));
  const auto closed = unwrap(makeReedBore(ReedBoreSpec{
      length, area, airPressure, temperature, BoreFarEnd::Closed}));
  const double density =
      unwrap(airDensity(PressurePascals{airPressure},
                        TemperatureKelvin{temperature}))
          .value();
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{temperature}})).value();
  const double impedance = density * speed / area;
  CHECK(open.airDensityKgPerM3 == density);
  CHECK(open.soundSpeedMetersPerSecond == speed);
  CHECK(nearlyEqual(open.acousticImpedance, impedance, 0.0, 1e-12));
  CHECK(nearlyEqual(open.roundTripSeconds, 2.0 * length / speed, 0.0, 1e-12));
  CHECK(nearlyEqual(open.fundamentalHz, speed / (4.0 * length), 0.0, 1e-12));
  CHECK(open.pressureReflection == -1.0);
  CHECK(nearlyEqual(closed.fundamentalHz, speed / (2.0 * length), 0.0, 1e-12));
  CHECK(closed.pressureReflection == 1.0);
  CHECK(unwrap(stoppedPipeMode(DistanceMeters{length},
                               SpeedMetersPerSecond{speed}, 1))
            .value() == open.fundamentalHz);
  CHECK(unwrap(openPipeMode(DistanceMeters{length}, SpeedMetersPerSecond{speed},
                            1))
            .value() == closed.fundamentalHz);

  const double quarter = speed / (8.0 * length);
  const auto openZ = unwrap(boreInputImpedance(open, quarter));
  const auto closedZ = unwrap(boreInputImpedance(closed, quarter));
  CHECK(nearlyEqual(openZ.real(), 0.0, 0.0, 1e-9));
  CHECK(nearlyEqual(openZ.imag(), impedance, 0.0, 1e-9));
  CHECK(nearlyEqual(closedZ.real(), 0.0, 0.0, 1e-9));
  CHECK(nearlyEqual(closedZ.imag(), -impedance, 0.0, 1e-9));
  const auto rest = unwrap(boreInputImpedance(open, 0.0));
  CHECK(rest.real() == 0.0);
  CHECK(rest.imag() == 0.0);

  CHECK(boreInputImpedance(open, open.fundamentalHz).error() ==
        QuantityError::RangeError);
  CHECK(boreInputImpedance(open, 3.0 * open.fundamentalHz).error() ==
        QuantityError::RangeError);
  CHECK(boreInputImpedance(closed, closed.fundamentalHz).error() ==
        QuantityError::RangeError);
  CHECK(boreInputImpedance(closed, 0.0).error() == QuantityError::RangeError);
  CHECK(boreInputImpedance(open, -1.0).error() ==
        QuantityError::InvalidArgument);

  const auto split = unwrap(boreWaveSplit(open, 1.0, 2.0));
  CHECK(split.incomingPressurePa == -2.0);
  CHECK(split.mouthPressurePa == -1.0);
  CHECK(nearlyEqual(split.volumeFlowM3PerS, 3.0 / impedance, 0.0, 1e-12));
  const auto closedSplit = unwrap(boreWaveSplit(closed, 1.0, 2.0));
  CHECK(closedSplit.incomingPressurePa == 2.0);
  CHECK(closedSplit.mouthPressurePa == 3.0);
  CHECK(nearlyEqual(closedSplit.volumeFlowM3PerS, -1.0 / impedance, 0.0,
                    1e-12));

  const auto shorter = unwrap(makeReedBore(ReedBoreSpec{
      0.25, area, airPressure, temperature, BoreFarEnd::Open}));
  CHECK(nearlyEqual(shorter.fundamentalHz, 2.0 * open.fundamentalHz, 0.0,
                    1e-12));
  CHECK(makeReedBore(ReedBoreSpec{0.0, area, airPressure, temperature,
                                  BoreFarEnd::Open})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeReedBore(ReedBoreSpec{length, -area, airPressure, temperature,
                                  BoreFarEnd::Open})
            .error() == QuantityError::InvalidArgument);

  const double jet = 1.5e-5;
  const double closure = 6.0e3;
  const auto reed = unwrap(makeReedCharacteristic(
      ReedSpec{jet, closure, airPressure, temperature}));
  const double mass = 2.0e-5;
  const double tip = 1.0e-4;
  const double closing = 1.0e-3;
  const auto undamped =
      unwrap(makeReedInertia(reed, ReedInertiaSpec{mass, tip, closing, 0.0}));
  const double stiffness = closure * tip / closing;
  CHECK(nearlyEqual(undamped.stiffnessNewtonsPerMeter, stiffness, 0.0, 1e-12));
  CHECK(nearlyEqual(undamped.resonanceHz,
                    std::sqrt(stiffness / mass) / (2.0 * constants::pi), 0.0,
                    1e-12));
  const double half = unwrap(reedEquilibriumDisplacement(undamped, closure / 2.0));
  CHECK(nearlyEqual(half, closing / 2.0, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(reedTipAcceleration(undamped, half, 0.0, closure / 2.0)),
                    0.0, 0.0, 1e-6));
  CHECK(unwrap(reedEquilibriumDisplacement(undamped, 0.0)) == 0.0);
  CHECK(nearlyEqual(unwrap(reedEquilibriumDisplacement(undamped, closure)),
                    closing, 0.0, 1e-15));
  CHECK(reedEquilibriumDisplacement(undamped, closure + 1.0).error() ==
        QuantityError::DomainError);
  CHECK(reedEquilibriumDisplacement(undamped, -1.0).error() ==
        QuantityError::DomainError);
  const double pastLay =
      unwrap(reedTipAcceleration(undamped, closing, 0.0, closure + 50.0));
  CHECK(nearlyEqual(pastLay, tip * 50.0 / mass, 0.0, 1e-9));

  const double damping = 0.02;
  const auto damped = unwrap(
      makeReedInertia(reed, ReedInertiaSpec{mass, tip, closing, damping}));
  CHECK(damped.dampingNewtonSecondsPerMeter == damping);
  CHECK(nearlyEqual(unwrap(reedTipAcceleration(damped, 0.0, 1.0, 0.0)),
                    -damping / mass, 0.0, 1e-12));
  CHECK(makeReedInertia(reed, ReedInertiaSpec{mass, tip, closing, -0.01})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeReedInertia(reed, ReedInertiaSpec{0.0, tip, closing, 0.0}).error() ==
        QuantityError::InvalidArgument);

  CHECK(unwrap(reedAreaAtDisplacement(reed, undamped, 0.0)) == jet);
  CHECK(nearlyEqual(unwrap(reedAreaAtDisplacement(reed, undamped, closing / 2.0)),
                    0.5 * jet, 0.0, 1e-12));
  CHECK(unwrap(reedAreaAtDisplacement(reed, undamped, closing)) == 0.0);
  CHECK(unwrap(reedAreaAtDisplacement(reed, undamped, 2.0 * closing)) == 0.0);
  CHECK(nearlyEqual(unwrap(reedAreaAtDisplacement(reed, undamped, -closing)),
                    2.0 * jet, 0.0, 1e-12));
}

TEST_CASE("felt compliance and the first hammer images keep the hard cutoff",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const StringCourse& course = violinInstrument.courses[2];
  const double fraction = 0.125;
  const double mass = 0.012;
  const double speed = 1.5;
  const auto blow =
      unwrap(makeHardHammerBlow(course, HammerStrike{fraction, mass, speed}));
  const double first = blow.firstReflectionTime;
  const double tau = blow.timeConstant;
  const double impedance = blow.impedance;

  CHECK(hardHammerForce(blow, first).error() == QuantityError::DomainError);
  CHECK(hardHammerVelocity(blow, 0.5 * first).ok());
  CHECK(reflectedHammerVelocity(blow, first).ok());
  CHECK(reflectedHammerForce(blow, 2.0 * first).error() ==
        QuantityError::DomainError);
  CHECK(reflectedHammerDisplacement(blow, 2.0 * first + 1.0e-9).error() ==
        QuantityError::DomainError);
  CHECK(reflectedHammerVelocity(blow, -1.0e-9).error() ==
        QuantityError::InvalidArgument);

  const double before = 0.5 * first;
  CHECK(nearlyEqual(unwrap(reflectedHammerVelocity(blow, before)),
                    unwrap(hardHammerVelocity(blow, before)), 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(reflectedHammerForce(blow, before)),
                    unwrap(hardHammerForce(blow, before)), 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(reflectedHammerDisplacement(blow, before)),
                    unwrap(hardHammerDisplacement(blow, before)), 0.0, 1e-12));

  const double pureAtFirst = speed * std::exp(-first / tau);
  CHECK(nearlyEqual(unwrap(reflectedHammerVelocity(blow, first)), pureAtFirst,
                    0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(reflectedHammerDisplacement(blow, first)),
                    speed * tau * (1.0 - std::exp(-first / tau)), 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(reflectedHammerForce(blow, first)),
                    2.0 * impedance * (pureAtFirst + speed), 0.0, 1e-9));
  const double justBefore = first * (1.0 - 1.0e-9);
  const double forceBefore = unwrap(reflectedHammerForce(blow, justBefore));
  CHECK(forceBefore < unwrap(reflectedHammerForce(blow, first)));

  const double later = 1.5 * first;
  const double nut = 2.0 * blow.positionMeters / blow.waveSpeed;
  const double bridge =
      2.0 * (blow.lengthMeters - blow.positionMeters) / blow.waveSpeed;
  const double age = later - first;
  const double decay = std::exp(-age / tau);
  const double extra = speed * (-age / tau) * decay;
  const double expectedVelocity =
      speed * std::exp(-later / tau) + extra;
  CHECK(nearlyEqual(unwrap(reflectedHammerVelocity(blow, later)),
                    expectedVelocity, 0.0, 1e-12));
  const double expectedForce =
      2.0 * impedance * (expectedVelocity + speed * decay);
  CHECK(nearlyEqual(unwrap(reflectedHammerForce(blow, later)), expectedForce,
                    0.0, 1e-9));
  CHECK((nut == first || bridge == first));
  CHECK(nut != bridge);

  const auto centered =
      unwrap(makeHardHammerBlow(course, HammerStrike{0.5, mass, speed}));
  const double centerTime = 1.5 * centered.firstReflectionTime;
  const double centerAge = centerTime - centered.firstReflectionTime;
  const double centerDecay = std::exp(-centerAge / centered.timeConstant);
  const double centerExtra =
      speed * (-centerAge / centered.timeConstant) * centerDecay;
  const double centerVelocity =
      speed * std::exp(-centerTime / centered.timeConstant) + 2.0 * centerExtra;
  CHECK(nearlyEqual(unwrap(reflectedHammerVelocity(centered, centerTime)),
                    centerVelocity, 0.0, 1e-12));
  CHECK(nearlyEqual(
      unwrap(reflectedHammerForce(centered, centerTime)),
      2.0 * centered.impedance *
          (centerVelocity + 2.0 * speed * centerDecay),
      0.0, 1e-9));

  const double sample = 1.5 * first;
  constexpr int steps = 8000;
  double integral = 0.0;
  double previous = unwrap(reflectedHammerVelocity(blow, 0.0));
  for (int i = 1; i <= steps; ++i) {
    const double time = sample * static_cast<double>(i) / steps;
    const double current = unwrap(reflectedHammerVelocity(blow, time));
    integral += 0.5 * (previous + current) * (sample / steps);
    previous = current;
  }
  CHECK(nearlyEqual(unwrap(reflectedHammerDisplacement(blow, sample)), integral,
                    0.0, 1e-5));

  const double probe = 1.25 * first;
  const double dt = 1.0e-8;
  const double earlier = unwrap(reflectedHammerVelocity(blow, probe - dt));
  const double ahead = unwrap(reflectedHammerVelocity(blow, probe + dt));
  const double slope = (ahead - earlier) / (2.0 * dt);
  double incoming = 0.0;
  if (probe >= nut) incoming += speed * std::exp(-(probe - nut) / tau);
  if (probe >= bridge) incoming += speed * std::exp(-(probe - bridge) / tau);
  const double atProbe = unwrap(reflectedHammerVelocity(blow, probe));
  CHECK(nearlyEqual(slope, -(atProbe + incoming) / tau, 0.0, 1e-4));

  const double stiffness = 1.0e8;
  const double exponent = 2.5;
  const double compression = 1.0e-4;
  const double felt = stiffness * std::pow(compression, exponent);
  CHECK(nearlyEqual(unwrap(feltHammerForce(compression, stiffness, exponent)),
                    felt, 0.0, 1e-12));
  CHECK(unwrap(feltHammerForce(0.0, stiffness, exponent)) == 0.0);
  CHECK(unwrap(feltHammerForce(-compression, stiffness, exponent)) == 0.0);
  CHECK(nearlyEqual(unwrap(feltHammerForce(compression, stiffness, 1.0)),
                    stiffness * compression, 0.0, 1e-12));
  CHECK(feltHammerForce(compression, 0.0, exponent).error() ==
        QuantityError::InvalidArgument);
  CHECK(feltHammerForce(compression, stiffness, -1.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(feltHammerForce(2.0, std::numeric_limits<double>::max(), 1.0).error() ==
        QuantityError::RangeError);

  FeltHammerState touching{compression, 0.5, 0.0, 0.2};
  const auto rate =
      unwrap(feltHammerDerivative(blow, touching, stiffness, exponent));
  CHECK(nearlyEqual(rate.forceNewtons, felt, 0.0, 1e-12));
  CHECK(nearlyEqual(rate.hammerAcceleration, -felt / mass, 0.0, 1e-9));
  CHECK(nearlyEqual(rate.stringVelocity, felt / (2.0 * impedance) + 0.2, 0.0,
                    1e-9));
  FeltHammerState clear{0.0, 0.5, compression, -0.3};
  const auto free = unwrap(feltHammerDerivative(blow, clear, stiffness, exponent));
  CHECK(free.forceNewtons == 0.0);
  CHECK(free.hammerAcceleration == 0.0);
  CHECK(free.stringVelocity == -0.3);
  CHECK(free.compressionMeters < 0.0);
}

namespace {
int harpSourceNumber(const char* text) {
  if (std::strcmp(text, "00") == 0) return -1;
  return std::atoi(text);
}

double idealTension(double lengthM, double frequencyHz, double gramsPerMeter) {
  const double wave = 2.0 * lengthM * frequencyHz;
  return gramsPerMeter * 1.0e-3 * wave * wave;
}
}  // namespace

TEST_CASE("concert pedal harp keeps height off the vibrating lengths",
          "[instruments]") {
  const PedalHarp harp = unwrap(concertPedalHarp());
  CHECK(formula(harp) == "Camac Atlantide Prestige Cb1 Gb7");
  CHECK(harp.strings.size() == kConcertHarpStrings);
  CHECK(harp.height.value() == kConcertHarpHeightM);
  CHECK(harp.reportedTensionSpreadN.min == 45.0);
  CHECK(harp.reportedTensionSpreadN.max == 756.0);
  CHECK(std::strcmp(harp.strings.front().sourceNumber, "45") == 0);
  CHECK(std::strcmp(harp.strings.front().label, "Cb1") == 0);
  CHECK(std::strcmp(harp.strings.back().sourceNumber, "00") == 0);
  CHECK(std::strcmp(harp.strings.back().label, "Gb7") == 0);
  CHECK(harp.compass.low == unwrap(makePitch("Cb1")).frequency);
  CHECK(harp.compass.high == unwrap(makePitch("Gb7")).frequency);
  CHECK(std::fabs(harp.compass.low.value() - 30.9) <= 0.1);
  CHECK(std::fabs(harp.compass.high.value() - 2960.0) <= 1.0);

  int wound = 0;
  int gut = 0;
  int nylon = 0;
  for (std::size_t i = 0; i < harp.strings.size(); ++i) {
    const HarpStringGeometry& string = harp.strings[i];
    const auto spelled = unwrap(makePitch(string.label));
    CHECK(std::fabs(spelled.frequency.value() - string.publishedFrequencyHz) <=
          string.printedHertzUnit);
    const double metres = string.vibratingLengthCm * 1.0e-2;
    CHECK(metres < harp.height.value());
    CHECK(metres != harp.height.value());
    CHECK(string.positionCm != string.vibratingLengthCm);
    CHECK(string.positionCm * 1.0e-2 != harp.height.value());
    CHECK(string.diameterMm > 0.0);
    const int number = harpSourceNumber(string.sourceNumber);
    if (number >= 34) {
      CHECK(string.material == HarpStringMaterial::WoundSteel);
      ++wound;
    } else if (number >= 7) {
      CHECK(string.material == HarpStringMaterial::Gut);
      ++gut;
    } else {
      CHECK(string.material == HarpStringMaterial::Nylon);
      ++nylon;
    }
    if (i > 0) {
      CHECK(spelled.frequency.value() >
            unwrap(makePitch(harp.strings[i - 1].label)).frequency.value());
      CHECK(string.vibratingLengthCm < harp.strings[i - 1].vibratingLengthCm);
    }
  }
  CHECK(wound == 12);
  CHECK(gut == 27);
  CHECK(nylon == 8);
  // The wound string's diameter is not a continuation of the gut gauge.
  const HarpStringGeometry* gutEnd = findHarpStringByNumber(harp, "33");
  const HarpStringGeometry* woundStart = findHarpStringByNumber(harp, "34");
  REQUIRE(gutEnd != nullptr);
  REQUIRE(woundStart != nullptr);
  CHECK(woundStart->diameterMm < gutEnd->diameterMm);
  CHECK(findHarpString(harp, "G4") == nullptr);
}

TEST_CASE("harp courses take density and tension ranges as arguments",
          "[instruments]") {
  const PedalHarp harp = unwrap(concertPedalHarp());
  const HarpStringGeometry* cb4 = findHarpString(harp, "Cb4");
  REQUIRE(cb4 != nullptr);
  CHECK(cb4->vibratingLengthCm == 58.8);
  const double frequency = unwrap(makePitch("Cb4")).frequency.value();
  const double length = cb4->vibratingLengthCm * 1.0e-2;
  const double tension = idealTension(length, frequency, 2.0);
  CHECK(tension > harp.reportedTensionSpreadN.min);
  CHECK(tension < harp.reportedTensionSpreadN.max);

  const StringCourse tuned =
      unwrap(tuneHarpString(*cb4, 1.0, 3.0, tension * 0.5, tension * 1.5));
  CHECK(tuned.frequencyPrinted);
  CHECK(std::fabs(tuned.publishedFrequencyHz - 246.9) <= 0.1);
  CHECK(nearlyEqual(unwrap(openStringFrequency(tuned)), tuned.tuning.frequency,
                    FrequencyHz{0.0}, 1e-12));
  CHECK(tuned.tension.value() > tuned.publishedTensionN.min);
  CHECK(tuned.tension.value() < tuned.publishedTensionN.max);
  CHECK(tuned.geometry.vibratingLength.value() != harp.height.value());

  // Inside Chadefaux's 45–756 N spread, and still refused: that spread is
  // not the per-string interval.
  CHECK(tension * 0.5 > 45.0);
  CHECK(tuneHarpString(*cb4, 1.0, 3.0, tension + 1.0, tension + 2.0).error() ==
        QuantityError::RangeError);
  CHECK(tuneHarpString(*cb4, 3.0, 1.0, tension * 0.5, tension * 1.5).error() ==
        QuantityError::InvalidArgument);

  HarpStringGeometry shifted = *cb4;
  shifted.publishedFrequencyHz = 100.0;
  CHECK(tuneHarpString(shifted, 1.0, 3.0, 1.0, 1.0e6).error() ==
        QuantityError::RangeError);

  // Cb7 is printed as 1975 Hz. The adapter is 1975.53 Hz, inside one
  // printed hertz and outside a tenth.
  const HarpStringGeometry* cb7 = findHarpStringByNumber(harp, "3");
  REQUIRE(cb7 != nullptr);
  const BowedStringRow wide{"Cb7", cb7->publishedFrequencyHz, 1.0, 2.0, 1.0,
                            1.0e6};
  CHECK(makeStringCourse(DistanceMeters{cb7->vibratingLengthCm * 1.0e-2}, wide,
                         "src", 0.1)
            .error() == QuantityError::RangeError);
  CHECK(makeStringCourse(DistanceMeters{cb7->vibratingLengthCm * 1.0e-2}, wide,
                         "src", cb7->printedHertzUnit)
            .ok());

  const HarpStringGeometry* lowest = findHarpStringByNumber(harp, "45");
  REQUIRE(lowest != nullptr);
  const double bassFrequency = unwrap(makePitch("Cb1")).frequency.value();
  const double bassTension =
      idealTension(lowest->vibratingLengthCm * 1.0e-2, bassFrequency, 20.0);
  const StringCourse bass = unwrap(
      tuneHarpString(*lowest, 10.0, 30.0, bassTension * 0.5, bassTension * 1.5));
  CHECK(nearlyEqual(bass.material.linearDensity.value(), 20.0e-3, 0.0, 1e-15));
  CHECK(bass.geometry.vibratingLength.value() !=
        lowest->diameterMm * 1.0e-3);

  const Excitation pluck{ExcitationKind::Pluck, 0.3, 1.0e-4};
  CHECK(realizeExcitation(tuned, pluck).ok());
  CHECK(realizeExcitation(tuned, Excitation{ExcitationKind::Bow, 0.3, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(tuned, Excitation{ExcitationKind::Hammer, 0.3, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(tuned, Excitation{ExcitationKind::Reed, 0.3, 1.0e-4})
            .error() == QuantityError::DomainError);
}

TEST_CASE("French double keeps case length off the speaking lengths",
          "[instruments]") {
  const FrenchDouble instrument = unwrap(frenchDouble());
  CHECK(formula(instrument) == "Goermans-Taskin French double F1 F6");
  CHECK(instrument.manuals == 2);
  CHECK(instrument.eightFootChoirs == 2);
  CHECK(instrument.fourFootChoirs == 1);
  CHECK(instrument.peauDeBuffle);
  CHECK(instrument.buffStop);
  CHECK(instrument.caseLength.value() ==
        kFrenchDoubleCaseLengthMm * 1.0e-3);
  CHECK(instrument.compass.noteCount == kFrenchDoubleCompassNotes);
  CHECK(instrument.compass.low == unwrap(makePitch("F1")).frequency);
  CHECK(instrument.compass.high == unwrap(makePitch("F6")).frequency);
  const auto low = unwrap(makePitch("F1"));
  const auto high = unwrap(makePitch("F6"));
  REQUIRE(low.midi.has_value());
  REQUIRE(high.midi.has_value());
  CHECK(*high.midi - *low.midi + 1 == kFrenchDoubleCompassNotes);

  int longEight = 0;
  int shortEight = 0;
  int fourFoot = 0;
  for (const HarpsichordSpeakingLength& string : instrument.strings) {
    CHECK(string.vibratingLengthMm < kFrenchDoubleCaseLengthMm);
    if (string.choir == HarpsichordChoir::LongEightFoot) {
      ++longEight;
      REQUIRE(string.pluckingPointMm.has_value());
      CHECK(*string.pluckingPointMm != string.vibratingLengthMm);
    } else if (string.choir == HarpsichordChoir::FourFoot) {
      ++fourFoot;
      CHECK(!string.pluckingPointMm.has_value());
    } else {
      ++shortEight;
    }
  }
  CHECK(longEight == 11);
  CHECK(fourFoot == 3);
  CHECK(shortEight == 0);
  CHECK(findSpeakingLength(instrument, HarpsichordChoir::ShortEightFoot,
                           "C4") == nullptr);

  const HarpsichordSpeakingLength* eight =
      findSpeakingLength(instrument, HarpsichordChoir::LongEightFoot, "C3");
  const HarpsichordSpeakingLength* four =
      findSpeakingLength(instrument, HarpsichordChoir::FourFoot, "C3");
  const HarpsichordSpeakingLength* eightC4 =
      findSpeakingLength(instrument, HarpsichordChoir::LongEightFoot, "C4");
  const HarpsichordSpeakingLength* fourC4 =
      findSpeakingLength(instrument, HarpsichordChoir::FourFoot, "C4");
  const HarpsichordSpeakingLength* bass =
      findSpeakingLength(instrument, HarpsichordChoir::LongEightFoot, "F1");
  REQUIRE(eight != nullptr);
  REQUIRE(four != nullptr);
  REQUIRE(eightC4 != nullptr);
  REQUIRE(fourC4 != nullptr);
  REQUIRE(bass != nullptr);
  CHECK(eight->vibratingLengthMm == 1207.0);
  CHECK(four->vibratingLengthMm == 637.0);
  CHECK(eightC4->vibratingLengthMm == 713.0);
  CHECK(fourC4->vibratingLengthMm == 351.0);
  CHECK(bass->vibratingLengthMm == 1835.0);
  CHECK(*bass->pluckingPointMm == 193.0);
  CHECK(std::fabs(four->vibratingLengthMm - 0.5 * eight->vibratingLengthMm) >
        0.5);
  CHECK(std::fabs(fourC4->vibratingLengthMm -
                  0.5 * eightC4->vibratingLengthMm) > 0.5);
  CHECK(bass->vibratingLengthMm * 1.0e-3 != instrument.caseLength.value());

  const double frequency = unwrap(makePitch("C3")).frequency.value();
  const double eightTension =
      idealTension(eight->vibratingLengthMm * 1.0e-3, frequency, 1.0);
  const double fourTension =
      idealTension(four->vibratingLengthMm * 1.0e-3, frequency, 1.0);
  CHECK(eightTension != fourTension);
  const StringCourse longCourse = unwrap(
      tuneHarpsichordString(*eight, 0.5, 1.5, eightTension * 0.5,
                            eightTension * 1.5));
  const StringCourse shortCourse = unwrap(
      tuneHarpsichordString(*four, 0.5, 1.5, fourTension * 0.5,
                            fourTension * 1.5));
  CHECK(!longCourse.frequencyPrinted);
  CHECK(longCourse.publishedFrequencyHz == 0.0);
  CHECK(longCourse.tuning.frequency == shortCourse.tuning.frequency);
  CHECK(longCourse.geometry.vibratingLength.value() >
        shortCourse.geometry.vibratingLength.value());
  CHECK(longCourse.tension.value() != shortCourse.tension.value());
  CHECK(nearlyEqual(unwrap(openStringFrequency(longCourse)),
                    longCourse.tuning.frequency, FrequencyHz{0.0}, 1e-12));
  CHECK(tuneHarpsichordString(*eight, 0.5, 1.5, fourTension * 0.5,
                              fourTension * 1.5)
            .error() == QuantityError::RangeError);

  const Excitation pluck{ExcitationKind::Pluck, 0.2, 1.0e-4};
  CHECK(realizeExcitation(longCourse, pluck).ok());
  CHECK(realizeExcitation(longCourse,
                          Excitation{ExcitationKind::Bow, 0.2, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(longCourse,
                          Excitation{ExcitationKind::Hammer, 0.2, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(longCourse,
                          Excitation{ExcitationKind::Reed, 0.2, 1.0e-4})
            .error() == QuantityError::DomainError);
}

TEST_CASE("classical guitar keeps the body off the 650 mm scale",
          "[instruments]") {
  const ClassicalGuitar guitar = unwrap(classicalGuitar());
  CHECK(formula(guitar) == "classical guitar E2 A2 D3 G3 B3 E4");
  CHECK(guitar.strings.size() == kClassicalGuitarStrings);
  CHECK(guitar.body.length.value() == kClassicalGuitarBodyLengthMm * 1.0e-3);
  CHECK(guitar.vibratingLength.value() ==
        kClassicalGuitarScaleLengthMm * 1.0e-3);
  CHECK(guitar.body.length.value() != guitar.vibratingLength.value());
  CHECK(guitar.vibratingLength.value() != 0.995);
  CHECK(guitar.body.length.value() != 0.052);
  CHECK(guitar.compass.low == unwrap(makePitch("E2")).frequency);
  CHECK(guitar.compass.high == unwrap(makePitch("E4")).frequency);
  CHECK(guitar.compass.high ==
        unwrap(makePitch(guitar.strings.back().label)).frequency);
  CHECK(guitar.compass.publishedLowHz == 82.4);
  CHECK(guitar.compass.publishedHighHz == 329.6);

  const int steps[kClassicalGuitarStrings - 1] = {5, 5, 5, 4, 5};
  const char* labels[kClassicalGuitarStrings] = {"E2", "A2", "D3",
                                                 "G3", "B3", "E4"};
  const char* positions[kClassicalGuitarStrings] = {"6", "5", "4",
                                                    "3", "2", "1"};
  for (std::size_t i = 0; i < guitar.strings.size(); ++i) {
    const ClassicalGuitarString& string = guitar.strings[i];
    CHECK(std::strcmp(string.label, labels[i]) == 0);
    CHECK(std::strcmp(string.position, positions[i]) == 0);
    const auto spelled = unwrap(makePitch(string.label));
    if (i > 0) {
      const auto previous = unwrap(makePitch(guitar.strings[i - 1].label));
      REQUIRE(spelled.midi.has_value());
      REQUIRE(previous.midi.has_value());
      CHECK(*spelled.midi - *previous.midi == steps[i - 1]);
    }
  }
  // Savarez's hundredths for D3 and B3 are not the adapter. E2 and E4,
  // printed to a tenth, are.
  const ClassicalGuitarString* third = findClassicalGuitarString(guitar, "D3");
  const ClassicalGuitarString* second = findClassicalGuitarString(guitar, "B3");
  const ClassicalGuitarString* first = findClassicalGuitarString(guitar, "E4");
  const ClassicalGuitarString* sixth = findClassicalGuitarStringByPosition(guitar, "6");
  REQUIRE(third != nullptr);
  REQUIRE(second != nullptr);
  REQUIRE(first != nullptr);
  REQUIRE(sixth != nullptr);
  CHECK(std::strcmp(sixth->label, "E2") == 0);
  CHECK(std::fabs(unwrap(makePitch("D3")).frequency.value() -
                  third->printedFrequencyHz) > third->printedHertzUnit);
  CHECK(std::fabs(unwrap(makePitch("B3")).frequency.value() -
                  second->printedFrequencyHz) > second->printedHertzUnit);
  CHECK(std::fabs(unwrap(makePitch("E4")).frequency.value() -
                  first->printedFrequencyHz) <= first->printedHertzUnit);
  CHECK(std::fabs(unwrap(makePitch("E2")).frequency.value() -
                  sixth->printedFrequencyHz) <= sixth->printedHertzUnit);
  CHECK(findClassicalGuitarString(guitar, "E5") == nullptr);
  CHECK(findClassicalGuitarStringByPosition(guitar, "7") == nullptr);
}

TEST_CASE("classical guitar courses take density and tension as arguments",
          "[instruments]") {
  const ClassicalGuitar guitar = unwrap(classicalGuitar());
  const ClassicalGuitarString* low =
      findClassicalGuitarStringByPosition(guitar, "6");
  const ClassicalGuitarString* high =
      findClassicalGuitarString(guitar, "E4");
  const ClassicalGuitarString* gString =
      findClassicalGuitarString(guitar, "G3");
  REQUIRE(low != nullptr);
  REQUIRE(high != nullptr);
  REQUIRE(gString != nullptr);
  const double length = guitar.vibratingLength.value();
  const double body = guitar.body.length.value();
  CHECK(length != body);
  const double lowHz = unwrap(makePitch(low->label)).frequency.value();
  const double highHz = unwrap(makePitch(high->label)).frequency.value();
  const double grams = 2.0;
  const double lowTension = idealTension(length, lowHz, grams);
  const double highTension = idealTension(length, highHz, grams);
  const double bodyTension = idealTension(body, lowHz, grams);
  CHECK(lowTension != highTension);
  CHECK(lowTension != bodyTension);

  const StringCourse bass = unwrap(tuneClassicalGuitarString(
      guitar, *low, 1.0, 3.0, lowTension * 0.5, lowTension * 1.5));
  const StringCourse treble = unwrap(tuneClassicalGuitarString(
      guitar, *high, 1.0, 3.0, highTension * 0.5, highTension * 1.5));
  CHECK(bass.frequencyPrinted == false);
  CHECK(bass.publishedFrequencyHz == 0.0);
  CHECK(treble.frequencyPrinted == false);
  CHECK(std::strcmp(bass.tuning.label, "E2") == 0);
  CHECK(std::strcmp(treble.tuning.label, "E4") == 0);
  CHECK(bass.geometry.vibratingLength.value() == length);
  CHECK(treble.geometry.vibratingLength.value() == length);
  CHECK(bass.geometry.vibratingLength.value() != body);
  CHECK(nearlyEqual(bass.material.linearDensity.value(), grams * 1.0e-3, 0.0,
                    1e-15));
  CHECK(nearlyEqual(unwrap(openStringFrequency(bass)), bass.tuning.frequency,
                    FrequencyHz{0.0}, 1e-12));
  CHECK(nearlyEqual(unwrap(openStringFrequency(treble)), treble.tuning.frequency,
                    FrequencyHz{0.0}, 1e-12));
  CHECK(bass.tuning.frequency == unwrap(makePitch("E2")).frequency);
  CHECK(bass.tension.value() != treble.tension.value());
  // A tension interval that fits the 490 mm body does not tune the scale.
  CHECK(tuneClassicalGuitarString(guitar, *low, 1.0, 3.0, bodyTension * 0.99,
                                  bodyTension * 1.01)
            .error() == QuantityError::RangeError);
  CHECK(tuneClassicalGuitarString(guitar, *low, 3.0, 1.0, lowTension * 0.5,
                                  lowTension * 1.5)
            .error() == QuantityError::InvalidArgument);

  const double gHz = unwrap(makePitch("G3")).frequency.value();
  const double gTension = idealTension(length, gHz, grams);
  const StringCourse openG = unwrap(tuneClassicalGuitarString(
      guitar, *gString, 1.0, 3.0, gTension * 0.5, gTension * 1.5));
  const double alpha = 0.2;
  const StringCourse stopped = unwrap(fingerStop(openG, alpha));
  CHECK(stopped.geometry.vibratingLength.value() == (1.0 - alpha) * length);
  CHECK(stopped.geometry.vibratingLength.value() != body);
  CHECK(stopped.tension.value() == openG.tension.value());
  CHECK(stopped.material.linearDensity.value() ==
        openG.material.linearDensity.value());
  CHECK(stopped.tuning.label == nullptr);
  CHECK(stopped.frequencyPrinted == false);
  CHECK(nearlyEqual(stopped.tuning.frequency.value(), gHz / (1.0 - alpha), 0.0,
                    1e-12));
  CHECK(nearlyEqual(unwrap(openStringFrequency(stopped)).value(),
                    stopped.tuning.frequency.value(), 0.0, 1e-12));
  CHECK(fingerStop(openG, 0.0).error() == QuantityError::InvalidArgument);
  CHECK(fingerStop(openG, 1.0).error() == QuantityError::InvalidArgument);

  const Excitation pluck{ExcitationKind::Pluck, 0.25, 1.0e-4};
  const auto shape = unwrap(realizeExcitation(openG, pluck));
  const auto modal = unwrap(ModalSolution::create(shape, 64));
  const double x = 0.25 * openG.geometry.vibratingLength.value();
  const TimePointSeconds time{1.0e-3};
  const double summed = unwrap(modal.at(DistanceMeters{x}, time));
  const double exact = unwrap(analyticField(shape, DistanceMeters{x}, time));
  CHECK(std::fabs(summed - exact) <= modal.truncationBound() + 1e-9);
  CHECK(unwrap(modalPluckResponse(openG, pluck, 0.25, time, 64)) == summed);
  const double peak = unwrap(
      modalPluckResponse(openG, pluck, 0.25, TimePointSeconds{0.0}, 64));
  CHECK(std::fabs(peak - pluck.amplitudeMeters) <=
        modal.truncationBound() + 1e-9);
  CHECK(realizeExcitation(stopped, pluck).ok());
  CHECK(realizeExcitation(openG, Excitation{ExcitationKind::Bow, 0.25, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(openG,
                          Excitation{ExcitationKind::Hammer, 0.25, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(openG, Excitation{ExcitationKind::Reed, 0.25, 1.0e-4})
            .error() == QuantityError::DomainError);
}

TEST_CASE("electric guitar uses the 25.5 in chart scale, not the classical body",
          "[instruments]") {
  const ElectricGuitar guitar = unwrap(electricGuitar());
  const ClassicalGuitar classical = unwrap(classicalGuitar());
  CHECK(formula(guitar) == "electric guitar E2 A2 D3 G3 B3 E4");
  CHECK(guitar.strings.size() == kElectricGuitarStrings);
  CHECK(guitar.vibratingLength.value() ==
        kElectricGuitarScaleInches * kElectricGuitarInchMeters);
  CHECK(guitar.vibratingLength.value() != classical.vibratingLength.value());
  CHECK(guitar.vibratingLength.value() != classical.body.length.value());
  CHECK(guitar.vibratingLength.value() != 0.648);
  CHECK(guitar.compass.low == unwrap(makePitch("E2")).frequency);
  CHECK(guitar.compass.high == unwrap(makePitch("E4")).frequency);
  CHECK(guitar.compass.publishedLowHz == 0.0);
  CHECK(guitar.compass.publishedHighHz == 0.0);

  const char* items[kElectricGuitarStrings] = {"NW046", "NW036", "NW026",
                                               "PL017", "PL013", "PL010"};
  const char* labels[kElectricGuitarStrings] = {"E2", "A2", "D3",
                                                "G3", "B3", "E4"};
  const double pounds[kElectricGuitarStrings] = {16.900, 19.000, 18.400,
                                                 16.600, 15.400, 16.200};
  const int steps[kElectricGuitarStrings - 1] = {5, 5, 5, 4, 5};
  for (std::size_t i = 0; i < guitar.strings.size(); ++i) {
    CHECK(std::strcmp(guitar.strings[i].item, items[i]) == 0);
    CHECK(std::strcmp(guitar.strings[i].label, labels[i]) == 0);
    CHECK(guitar.strings[i].tensionPounds == pounds[i]);
    if (i > 0) {
      const auto spelled = unwrap(makePitch(guitar.strings[i].label));
      const auto previous = unwrap(makePitch(guitar.strings[i - 1].label));
      REQUIRE(spelled.midi.has_value());
      REQUIRE(previous.midi.has_value());
      CHECK(*spelled.midi - *previous.midi == steps[i - 1]);
    }
  }
  // The printed pounds are not a smooth function of pitch.
  CHECK(guitar.strings[1].tensionPounds > guitar.strings[0].tensionPounds);
  CHECK(guitar.strings[1].tensionPounds > guitar.strings[2].tensionPounds);
  CHECK(findElectricGuitarString(guitar, "E5") == nullptr);
  CHECK(findElectricGuitarStringByItem(guitar, "PL009") == nullptr);
}

TEST_CASE("electric guitar density is an argument checked against EXL110",
          "[instruments]") {
  const ElectricGuitar guitar = unwrap(electricGuitar());
  const ElectricGuitarString* low = findElectricGuitarStringByItem(guitar, "NW046");
  const ElectricGuitarString* high = findElectricGuitarString(guitar, "E4");
  REQUIRE(low != nullptr);
  REQUIRE(high != nullptr);
  const double length = guitar.vibratingLength.value();
  const double lowHz = unwrap(makePitch(low->label)).frequency.value();
  const double highHz = unwrap(makePitch(high->label)).frequency.value();
  const double lowNewtons = low->tensionPounds * kPoundForceNewtons;
  const double highNewtons = high->tensionPounds * kPoundForceNewtons;
  const double lowGrams =
      lowNewtons / ((2.0 * length * lowHz) * (2.0 * length * lowHz)) * 1.0e3;
  const double highGrams =
      highNewtons / ((2.0 * length * highHz) * (2.0 * length * highHz)) * 1.0e3;
  CHECK(lowGrams != highGrams);
  const StringCourse bass =
      unwrap(tuneElectricGuitarString(guitar, *low, lowGrams));
  const StringCourse treble =
      unwrap(tuneElectricGuitarString(guitar, *high, highGrams));
  CHECK(bass.frequencyPrinted == false);
  CHECK(bass.publishedFrequencyHz == 0.0);
  CHECK(std::strcmp(bass.tuning.label, "E2") == 0);
  CHECK(bass.geometry.vibratingLength.value() == length);
  CHECK(bass.geometry.vibratingLength.value() != 0.650);
  CHECK(nearlyEqual(unwrap(openStringFrequency(bass)), bass.tuning.frequency,
                    FrequencyHz{0.0}, 1e-12));
  CHECK(bass.tuning.frequency == unwrap(makePitch("E2")).frequency);
  CHECK(std::fabs(bass.tension.value() - lowNewtons) <=
        kElectricGuitarTensionUnitPounds * kPoundForceNewtons);
  CHECK(bass.tension.value() != treble.tension.value());
  const double atClassicalLength =
      (lowGrams * 1.0e-3) * (2.0 * 0.650 * lowHz) * (2.0 * 0.650 * lowHz);
  CHECK(std::fabs(atClassicalLength - lowNewtons) >
        kElectricGuitarTensionUnitPounds * kPoundForceNewtons);
  CHECK(tuneElectricGuitarString(guitar, *low, lowGrams * 1.01).error() ==
        QuantityError::RangeError);
  CHECK(tuneElectricGuitarString(guitar, *low, 0.0).error() ==
        QuantityError::InvalidArgument);

  const double alpha = 0.25;
  const StringCourse stopped = unwrap(fingerStop(bass, alpha));
  CHECK(stopped.geometry.vibratingLength.value() == (1.0 - alpha) * length);
  CHECK(stopped.tension.value() == bass.tension.value());
  CHECK(nearlyEqual(stopped.tuning.frequency.value(), lowHz / (1.0 - alpha),
                    0.0, 1e-12));
  CHECK(fingerStop(bass, 0.0).error() == QuantityError::InvalidArgument);

  const Excitation pluck{ExcitationKind::Pluck, 0.3, 1.0e-4};
  const auto shape = unwrap(realizeExcitation(treble, pluck));
  const auto modal = unwrap(ModalSolution::create(shape, 64));
  const double x = 0.3 * treble.geometry.vibratingLength.value();
  const TimePointSeconds time{8.0e-4};
  const double summed = unwrap(modal.at(DistanceMeters{x}, time));
  const double exact = unwrap(analyticField(shape, DistanceMeters{x}, time));
  CHECK(std::fabs(summed - exact) <= modal.truncationBound() + 1e-9);
  CHECK(unwrap(modalPluckResponse(treble, pluck, 0.3, time, 64)) == summed);
  CHECK(realizeExcitation(treble, Excitation{ExcitationKind::Bow, 0.3, 1.0e-4})
            .error() == QuantityError::DomainError);
  CHECK(realizeExcitation(stopped, pluck).ok());
}

TEST_CASE("the piano string bank is Chabassier's five strings", "[instruments]") {
  const PianoStringBank bank = unwrap(concertGrandStringBank());
  CHECK(formula(bank) == "concert grand D#1 C2 F3 C#5 G6");
  const auto specs = chabassier2013Strings();
  REQUIRE(bank.strings.size() == specs.size());
  CHECK(bank.strings.size() == 5);
  const double lengths[5] = {1.945, 1.600, 0.961, 0.326, 0.124};
  const double tensions[5] = {1781.0, 865.0, 774.0, 684.0, 587.0};
  const char* notes[5] = {"D#1", "C2", "F3", "C#5", "G6"};
  for (std::size_t i = 0; i < bank.strings.size(); ++i) {
    CHECK(std::strcmp(bank.strings[i].note, notes[i]) == 0);
    CHECK(bank.strings[i].length.value() == lengths[i]);
    CHECK(bank.strings[i].length.value() == specs[i].length.value());
    CHECK(bank.strings[i].tension.value() == tensions[i]);
    CHECK(bank.strings[i].tension.value() == specs[i].tension.value());
    if (i > 0) {
      CHECK(bank.strings[i].length.value() < bank.strings[i - 1].length.value());
      CHECK(bank.strings[i].semitonesFromC4 > bank.strings[i - 1].semitonesFromC4);
    }
    const StringCourse course = unwrap(tunePianoString(bank.strings[i]));
    CHECK(course.frequencyPrinted);
    CHECK(course.publishedFrequencyHz == specs[i].publishedF0.value());
    CHECK(course.geometry.vibratingLength.value() == lengths[i]);
    CHECK(course.tension.value() == tensions[i]);
    CHECK(nearlyEqual(unwrap(openStringFrequency(course)).value(),
                      course.tuning.frequency.value(), 0.0, 1e-12));
    CHECK(std::fabs(course.tuning.frequency.value() - specs[i].publishedF0.value()) <=
          kChabassierFundamentalTolerance * specs[i].publishedF0.value());
    const auto stiff = unwrap(tunedPianoString(bank.strings[i]));
    CHECK(unwrap(modeFrequency(stiff, 1)).value() >
          course.tuning.frequency.value());
  }
  const PianoStringSpec* csharp = findPianoString(bank, "C#5");
  REQUIRE(csharp != nullptr);
  CHECK(csharp->publishedF0.value() == 555.6);
  CHECK(std::fabs(unwrap(makePitch("C#5")).frequency.value() - 555.6) > 0.1);
  const StringCourse csharpCourse = unwrap(tunePianoString(*csharp));
  CHECK(std::fabs(csharpCourse.tuning.frequency.value() -
                  unwrap(makePitch("C#5")).frequency.value()) > 0.1);
  CHECK(findPianoString(bank, "A0") == nullptr);
  CHECK(findPianoString(bank, "C4") == nullptr);
  CHECK(findPianoString(bank, "C8") == nullptr);
  CHECK(bank.compass.publishedLowHz == 38.9);
  CHECK(bank.compass.publishedHighHz == 1571.4);
  CHECK(bank.compass.low == unwrap(tunePianoString(bank.strings.front())).tuning.frequency);
  CHECK(bank.strings.front().length.value() != bank.strings.back().length.value());

  const StringCourse lowCourse = unwrap(tunePianoString(bank.strings.front()));
  const StringCourse highCourse = unwrap(tunePianoString(bank.strings.back()));
  const double alpha = 0.2;
  const StringCourse stopped = unwrap(fingerStop(lowCourse, alpha));
  CHECK(stopped.tension.value() == lowCourse.tension.value());
  CHECK(stopped.geometry.vibratingLength.value() ==
        (1.0 - alpha) * lowCourse.geometry.vibratingLength.value());
  CHECK(stopped.geometry.vibratingLength.value() != highCourse.geometry.vibratingLength.value());

  const HammerStrike strike{0.15, 0.012, 1.5};
  const auto lowBlow = unwrap(makeHardHammerBlow(lowCourse, strike));
  const auto highBlow = unwrap(makeHardHammerBlow(highCourse, strike));
  CHECK(lowBlow.initialForce == 2.0 * lowBlow.impedance * strike.speedMetersPerSecond);
  CHECK(lowBlow.firstReflectionTime > 0.0);
  CHECK(lowBlow.lengthMeters != highBlow.lengthMeters);
  CHECK(lowBlow.impedance != highBlow.impedance);
  const double compression = 1.0e-4;
  const double stiffness = 1.0e10;
  const double exponent = 2.5;
  CHECK(nearlyEqual(unwrap(feltHammerForce(compression, stiffness, exponent)),
                    stiffness * std::pow(compression, exponent), 0.0, 1e-12));
  CHECK(unwrap(feltHammerForce(0.0, stiffness, exponent)) == 0.0);
  CHECK(realizeExcitation(lowCourse, Excitation{ExcitationKind::Hammer, 0.15, 1.0e-4})
            .error() == QuantityError::DomainError);
  const Excitation pluck{ExcitationKind::Pluck, 0.15, 1.0e-4};
  CHECK(realizeExcitation(lowCourse, pluck).ok());
}

TEST_CASE("a drum kit head is a circular membrane with caller tension",
          "[instruments]") {
  CHECK(nearlyEqual(unwrap(besselJZero(0, 1)), 2.404825557695773, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(besselJZero(1, 1)), 3.8317059702075125, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(besselJZero(2, 1)), 5.135622301840683, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(besselJZero(0, 2)), 5.520078110286311, 0.0, 1e-12));
  CHECK(besselJZero(-1, 1).error() == QuantityError::InvalidArgument);
  CHECK(besselJZero(0, 0).error() == QuantityError::InvalidArgument);

  const auto kit = unwrap(yamahaTourCustomKit());
  CHECK(kit.bassDrum.diameterInches == 22.0);
  CHECK(kit.bassDrum.depthInches == 16.0);
  CHECK(kit.rackTom10.diameterInches == 10.0);
  CHECK(kit.rackTom10.depthInches == 7.0);
  CHECK(kit.rackTom12.diameterInches == 12.0);
  CHECK(kit.rackTom12.depthInches == 8.0);
  CHECK(kit.floorTom.diameterInches == 16.0);
  CHECK(kit.floorTom.depthInches == 15.0);
  CHECK(kit.snare55.diameterInches == 14.0);
  CHECK(kit.snare55.depthInches == 5.5);
  CHECK(kit.snare65.diameterInches == 14.0);
  CHECK(kit.snare65.depthInches == 6.5);
  CHECK(kit.bassDrum.model != nullptr);
  CHECK(std::strcmp(kit.bassDrum.model, "TMB-2216") == 0);
  CHECK(std::strcmp(kit.rackTom10.model, "TMT-1007") == 0);
  CHECK(std::strcmp(kit.rackTom12.model, "TMT-1208") == 0);
  CHECK(std::strcmp(kit.floorTom.model, "TMF-1615") == 0);
  CHECK(std::strcmp(kit.snare55.model, "TMS-1455") == 0);
  CHECK(std::strcmp(kit.snare65.model, "TMS-1465") == 0);
  CHECK(nearlyEqual(kit.bassDrum.headRadius.value(), 11.0 * 0.0254, 0.0, 1e-15));
  CHECK(nearlyEqual(kit.snare55.shellDepth.value(), 5.5 * 0.0254, 0.0, 1e-15));
  CHECK(kit.snare55.headRadius.value() == kit.snare65.headRadius.value());
  CHECK(kit.snare55.shellDepth.value() != kit.snare65.shellDepth.value());
  CHECK(kit.bassDrum.shellThicknessMeters == 0.0056);
  CHECK(kit.bassDrum.shellThicknessMeters != kit.bassDrum.headRadius.value());
  CHECK(kit.bassDrum.bearingEdgePrinted);
  CHECK(kit.bassDrum.bearingEdgeDegrees == 45.0);
  CHECK(!kit.snare55.bearingEdgePrinted);
  CHECK(std::strcmp(formula(kit).c_str(),
                    "Yamaha Tour Custom TMB-2216 TMT-1007 TMT-1208 TMF-1615 "
                    "TMS-1455 TMS-1465") == 0);

  const double densityMin = 0.20;
  const double densityMax = 0.30;
  const double density = 0.5 * (densityMin + densityMax);
  const double frequency = 100.0;
  const double alpha = unwrap(besselJZero(0, 1));
  auto requiredTension = [&](const MembraneShell& shell) {
    const double wave =
        constants::twoPi * shell.headRadius.value() * frequency / alpha;
    return density * wave * wave;
  };
  const double bassTension = requiredTension(kit.bassDrum);
  const double rackTension = requiredTension(kit.rackTom10);
  CHECK(nearlyEqual(bassTension / rackTension,
                    (22.0 / 10.0) * (22.0 / 10.0), 0.0, 1e-12));

  const auto bass = unwrap(tuneMembraneHead(kit.bassDrum, frequency, densityMin,
                                            densityMax, bassTension * 0.5,
                                            bassTension * 1.5));
  const auto rack = unwrap(tuneMembraneHead(kit.rackTom10, frequency, densityMin,
                                            densityMax, rackTension * 0.5,
                                            rackTension * 1.5));
  CHECK(nearlyEqual(bass.tension.value(), bassTension, 0.0, 1e-12));
  CHECK(nearlyEqual(rack.tension.value(), rackTension, 0.0, 1e-12));
  CHECK(bass.arealDensity.value() == density);
  CHECK(nearlyEqual(unwrap(circularMembraneMode(bass.radius, bass.tension,
                                                bass.arealDensity, 0, 1))
                        .value(),
                    frequency, 0.0, 1e-12));
  const double ratio01 =
      unwrap(besselJZero(1, 1)) / unwrap(besselJZero(0, 1));
  const double heard =
      unwrap(circularMembraneMode(bass.radius, bass.tension, bass.arealDensity, 1, 1))
          .value() /
      unwrap(circularMembraneMode(bass.radius, bass.tension, bass.arealDensity, 0, 1))
          .value();
  CHECK(nearlyEqual(heard, ratio01, 0.0, 1e-12));
  const auto louder = unwrap(tuneMembraneHead(
      kit.bassDrum, frequency * 2.0, densityMin, densityMax, bassTension * 2.0,
      bassTension * 6.0));
  CHECK(nearlyEqual(louder.tension.value(), 4.0 * bassTension, 0.0, 1e-12));
  CHECK(tuneMembraneHead(kit.bassDrum, frequency, densityMin, densityMax,
                         bassTension + 1.0, bassTension + 2.0)
            .error() == QuantityError::RangeError);
  CHECK(tuneMembraneHead(kit.snare55, 0.0, densityMin, densityMax, 1.0, 2.0)
            .error() == QuantityError::InvalidArgument);

  const auto snareShallow = unwrap(tuneMembraneHead(
      kit.snare55, frequency, densityMin, densityMax, requiredTension(kit.snare55) * 0.5,
      requiredTension(kit.snare55) * 1.5));
  const auto snareDeep = unwrap(tuneMembraneHead(
      kit.snare65, frequency, densityMin, densityMax, requiredTension(kit.snare65) * 0.5,
      requiredTension(kit.snare65) * 1.5));
  CHECK(snareShallow.tension.value() == snareDeep.tension.value());
  CHECK(kit.snare55.shellDepth.value() != kit.snare65.shellDepth.value());
  const SurfaceTensionNewtonsPerMeter sharedTension{1000.0};
  const ArealDensityKgPerSquareMeter sharedDensity{density};
  const double bassOpen =
      unwrap(circularMembraneMode(kit.bassDrum.headRadius, sharedTension,
                                  sharedDensity, 0, 1))
          .value();
  const double rackOpen =
      unwrap(circularMembraneMode(kit.rackTom10.headRadius, sharedTension,
                                  sharedDensity, 0, 1))
          .value();
  CHECK(nearlyEqual(rackOpen / bassOpen,
                    kit.bassDrum.headRadius.value() / kit.rackTom10.headRadius.value(),
                    0.0, 1e-12));

  const double momentum = 0.01;
  const MembraneImpulse center{0.0, 0.3, momentum};
  const auto axis =
      unwrap(membraneStrikeCoefficient(bass, center, 0, 1));
  const auto diametral =
      unwrap(membraneStrikeCoefficient(bass, center, 1, 1));
  CHECK(diametral.velocityCoefficient == 0.0);
  const double norm =
      constants::twoPi * 0.5 * bass.radius.value() * bass.radius.value() *
      std::pow(std::cyl_bessel_j(1.0, axis.besselZero), 2.0);
  const double expectedB = momentum / bass.arealDensity.value() * 1.0 / norm;
  CHECK(nearlyEqual(axis.velocityCoefficient, expectedB, 0.0, 1e-12));
  CHECK(unwrap(membraneStrikeDisplacement(bass, center, 0.0, 0.0, 0.0, 2, 2)) ==
        0.0);
  const double time = 1.0e-4;
  const double oneMode =
      (axis.velocityCoefficient / axis.angularFrequency) * std::sin(axis.angularFrequency * time);
  CHECK(nearlyEqual(unwrap(membraneStrikeDisplacement(bass, center, 0.0, 0.0, time, 0, 1)),
                    oneMode, 0.0, 1e-12));
  CHECK(nearlyEqual(unwrap(membraneStrikeVelocity(bass, center, 1.0, 0.0, time, 2, 2)),
                    0.0, 1.0e-9, 1.0e-6));
  CHECK(membraneStrikeCoefficient(bass, MembraneImpulse{1.0, 0.0, momentum}, 0, 1)
            .error() == QuantityError::InvalidArgument);
  const MembraneImpulse off{0.4, 0.2, momentum};
  CHECK(unwrap(membraneStrikeCoefficient(bass, off, 1, 1)).velocityCoefficient !=
        0.0);
  const auto doubled = unwrap(
      membraneStrikeCoefficient(bass, MembraneImpulse{0.0, 0.0, 2.0 * momentum}, 0, 1));
  CHECK(nearlyEqual(doubled.velocityCoefficient, 2.0 * axis.velocityCoefficient, 0.0,
                    1e-12));

  const double compression = 1.0e-4;
  const double stiffness = 1.0e7;
  const double exponent = 2.0;
  const double mass = 0.02;
  const auto contact =
      unwrap(feltContactRate(compression, stiffness, exponent, mass));
  const double felt = stiffness * compression * compression;
  CHECK(nearlyEqual(contact.forceNewtons, felt, 0.0, 1e-12));
  CHECK(nearlyEqual(contact.acceleration, -felt / mass, 0.0, 1e-12));
  CHECK(unwrap(feltContactRate(-compression, stiffness, exponent, mass)).forceNewtons ==
        0.0);
  CHECK(feltContactRate(compression, stiffness, exponent, 0.0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("cymbals are free plates and the XSR set keeps alloy numbers as arguments",
          "[instruments]") {
  const auto set = unwrap(sabianXsrCompleteSet());
  CHECK(set.splash.diameterInches == 10.0);
  CHECK(set.hiHatTop.diameterInches == 14.0);
  CHECK(set.hiHatBottom.diameterInches == 14.0);
  CHECK(set.hiHatTop.radius.value() == set.hiHatBottom.radius.value());
  CHECK(set.crash16.diameterInches == 16.0);
  CHECK(set.crash18.diameterInches == 18.0);
  CHECK(set.china.diameterInches == 18.0);
  CHECK(set.ride.diameterInches == 20.0);
  CHECK(std::strcmp(set.alloyName, "B20") == 0);
  CHECK(nearlyEqual(set.ride.radius.value(), 10.0 * 0.0254, 0.0, 1e-15));
  CHECK(std::strcmp(formula(set).c_str(),
                    "SABIAN XSR Complete Set XSR5006B splash hi-hat top "
                    "hi-hat bottom fast crash fast crash chinese ride") == 0);

  const CymbalMaterial material{0.001, 1.0e11, 7800.0, 0.33};
  const auto fundamental =
      unwrap(freeCircularPlateMode(2, 1, material.poisson));
  const double lambdaSquared = fundamental.lambda * fundamental.lambda;
  CHECK(lambdaSquared > 4.5);
  CHECK(lambdaSquared < 6.5);
  const double below =
      unwrap(freePlateCharacteristic(2, fundamental.lambda * (1.0 - 1.0e-8),
                                    material.poisson));
  const double above =
      unwrap(freePlateCharacteristic(2, fundamental.lambda * (1.0 + 1.0e-8),
                                    material.poisson));
  CHECK(below * above <= 0.0);
  const auto axisymmetric =
      unwrap(freeCircularPlateMode(0, 1, material.poisson));
  CHECK(axisymmetric.lambda > fundamental.lambda);

  const double ride =
      unwrap(cymbalModeFrequency(set.ride, material, 2, 1)).value();
  const double crash =
      unwrap(cymbalModeFrequency(set.crash16, material, 2, 1)).value();
  CHECK(nearlyEqual(crash / ride,
                    (set.ride.radius.value() / set.crash16.radius.value()) *
                        (set.ride.radius.value() / set.crash16.radius.value()),
                    0.0, 1e-9));
  const CymbalMaterial thicker{0.002, material.youngsModulusPa,
                               material.densityKgPerM3, material.poisson};
  CHECK(nearlyEqual(unwrap(cymbalModeFrequency(set.ride, thicker, 2, 1)).value(),
                    2.0 * ride, 0.0, 1e-12));
  const CymbalMaterial stiffer{material.thicknessMeters, 4.0 * material.youngsModulusPa,
                               material.densityKgPerM3, material.poisson};
  CHECK(nearlyEqual(unwrap(cymbalModeFrequency(set.ride, stiffer, 2, 1)).value(),
                    2.0 * ride, 0.0, 1e-12));
  CHECK(cymbalModeFrequency(set.ride, CymbalMaterial{0.001, 1.0e11, 7800.0, 0.5}, 2, 1)
            .error() == QuantityError::InvalidArgument);
  const double hats =
      unwrap(cymbalModeFrequency(set.hiHatTop, material, 2, 1)).value();
  CHECK(unwrap(cymbalModeFrequency(set.hiHatBottom, material, 2, 1)).value() ==
        hats);
  CHECK(unwrap(cymbalModeFrequency(set.china, material, 2, 1)).value() ==
        unwrap(cymbalModeFrequency(set.crash18, material, 2, 1)).value());

  const auto edge = unwrap(freePlateRadialShape(
      fundamental, set.ride.radius.value(), set.ride.radius.value()));
  CHECK(edge != 0.0);
  const CymbalImpulse center{0.0, 0.4, 0.002};
  CHECK(unwrap(cymbalStrikeCoefficient(set.ride, material, center, 2, 1))
            .velocityCoefficient == 0.0);
  const auto driven =
      unwrap(cymbalStrikeCoefficient(set.ride, material, center, 0, 1));
  CHECK(driven.velocityCoefficient != 0.0);
  CHECK(unwrap(cymbalStrikeDisplacement(set.ride, material, center, 0.2, 0.4, 0.0, 2,
                                        1)) == 0.0);
  const double later = 1.0e-5;
  const auto shape = unwrap(freePlateRadialShape(
      unwrap(freeCircularPlateMode(0, 1, material.poisson)), set.ride.radius.value(),
      0.0));
  const double expected = (driven.velocityCoefficient / driven.angularFrequency) *
                          std::sin(driven.angularFrequency * later) * shape;
  CHECK(nearlyEqual(unwrap(cymbalStrikeDisplacement(set.ride, material, center, 0.0, 0.0,
                                                    later, 0, 1)),
                    expected, 0.0, 1e-9));
  const auto heavy = unwrap(cymbalStrikeCoefficient(
      set.ride, material, CymbalImpulse{0.0, 0.0, 0.004}, 0, 1));
  CHECK(nearlyEqual(heavy.velocityCoefficient, 2.0 * driven.velocityCoefficient, 0.0,
                    1e-12));
  const auto felt = unwrap(feltContactRate(2.0e-4, 5.0e6, 1.5, 0.03));
  CHECK(felt.forceNewtons > 0.0);
  CHECK(felt.acceleration < 0.0);
}

TEST_CASE("wound-string stiffness uses a caller core and not an outer diameter",
          "[instruments]") {
  auto courseOf = [](double length, double mu, double tension) {
    return StringCourse{
        StringGeometry{DistanceMeters{length}},
        StringMaterial{LinearDensityKgPerM{mu}},
        StringTuning{"synthetic",
                     FrequencyHz{std::sqrt(tension / mu) / (2.0 * length)}},
        TensionNewtons{tension},
        TabulatedRange{},
        TabulatedRange{},
        0.0,
        false,
        "synthetic"};
  };

  const double diameter = 1.0e-3;
  const double young = 2.0e11;
  const double rho = 8000.0;
  const double length = 0.5;
  const double wave = 200.0;
  const double factor = 4.0;
  const double area = constants::pi * diameter * diameter / 4.0;
  const double mu = rho * factor * area;
  const double tension = mu * wave * wave;
  const StringCourse course = courseOf(length, mu, tension);
  const auto wound = unwrap(makeWoundString(course, diameter, young, rho));
  CHECK(nearlyEqual(wound.windingFactor, factor, 0.0, 1e-12));
  const double kappa = 0.25 * diameter * std::sqrt(young / (rho * factor));
  CHECK(nearlyEqual(wound.stiffness, kappa, 0.0, 1e-12));
  CHECK(nearlyEqual(wound.inharmonicity, inharmonicityB(wound.system), 0.0,
                    1e-15));
  CHECK(wound.system.stiffLeft == StiffEnd::Hinged);
  CHECK(wound.system.stiffRight == StiffEnd::Hinged);
  const double ideal =
      std::sqrt(tension / mu) / (2.0 * length);
  const auto spectrum =
      unwrap(makeStiffSpectrum(FrequencyHz{ideal}, wound.inharmonicity, 6));
  for (int n = 1; n <= 6; ++n) {
    CHECK(nearlyEqual(unwrap(modeFrequency(wound.system, n)).value(),
                      unwrap(partialFrequency(spectrum, n)).value(), 0.0,
                      1e-12));
  }

  const auto doubledModulus =
      unwrap(makeWoundString(course, diameter, 2.0 * young, rho));
  CHECK(nearlyEqual(doubledModulus.inharmonicity, 2.0 * wound.inharmonicity,
                    0.0, 1e-12));

  const StringCourse heavier =
      courseOf(length, 2.0 * mu, 2.0 * tension);
  const auto doubledMass =
      unwrap(makeWoundString(heavier, diameter, young, rho));
  CHECK(nearlyEqual(doubledMass.windingFactor, 2.0 * factor, 0.0, 1e-12));
  CHECK(nearlyEqual(doubledMass.inharmonicity, 0.5 * wound.inharmonicity, 0.0,
                    1e-12));

  const auto wider =
      unwrap(makeWoundString(course, 2.0 * diameter, young, rho));
  CHECK(nearlyEqual(wider.windingFactor, 1.0, 0.0, 1e-12));
  CHECK(nearlyEqual(wider.inharmonicity, 16.0 * wound.inharmonicity, 0.0,
                    1e-12));

  // An outer diameter passed as a core weighs more than the string.
  CHECK(makeWoundString(course, 4.0 * diameter, young, rho).error() ==
        QuantityError::DomainError);
  CHECK(makeWoundString(course, 0.0, young, rho).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeWoundString(course, diameter, -young, rho).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeWoundString(course, diameter, young, 0.0).error() ==
        QuantityError::InvalidArgument);
  StringCourse blank = course;
  blank.tension = TensionNewtons{0.0};
  CHECK(makeWoundString(blank, diameter, young, rho).error() ==
        QuantityError::InvalidArgument);

  // Chabassier's Table III, including its wound rows and its plain F = 1
  // rows. Those cores are not a violin, viola, cello, bass, or harp.
  const auto material = steelPianoWireChabassier2013();
  const auto specs = chabassier2013Strings();
  for (const PianoStringSpec& spec : specs) {
    const auto piano = unwrap(pianoString(spec, material));
    const double d = spec.diameter.value();
    const double specMu = material.densityKgPerM3 * spec.densityFactor *
                          constants::pi * d * d / 4.0;
    const StringCourse built =
        courseOf(spec.length.value(), specMu, spec.tension.value());
    const auto matched = unwrap(makeWoundString(
        built, d, material.youngsModulusPa, material.densityKgPerM3));
    CHECK(nearlyEqual(matched.stiffness, piano.stiffness, 0.0, 1e-12));
    CHECK(nearlyEqual(matched.windingFactor, spec.densityFactor, 0.0, 1e-12));
    if (!spec.plainWire) CHECK(matched.windingFactor > 1.0);
  }
  const PianoStringSpec& lowest = specs.front();
  const double lowestMu = material.densityKgPerM3 * lowest.densityFactor *
                          constants::pi * lowest.diameter.value() *
                          lowest.diameter.value() / 4.0;
  const StringCourse lowestCourse = courseOf(
      lowest.length.value(), lowestMu, lowest.tension.value());
  CHECK(makeWoundString(lowestCourse, 10.0 * lowest.diameter.value(),
                        material.youngsModulusPa, material.densityKgPerM3)
            .error() == QuantityError::DomainError);

  // A bowed course accepts a caller core. Nothing on the violin supplies one.
  // Le Carrou's printed harp diameters are outer diameters and are not read.
  const auto violinInstrument = unwrap(violin());
  const auto thin = unwrap(makeWoundString(violinInstrument.courses[0], 2.0e-4,
                                           1.0e11, 7000.0));
  const auto thick = unwrap(makeWoundString(violinInstrument.courses[0], 4.0e-4,
                                            1.0e11, 7000.0));
  CHECK(thin.windingFactor > 1.0);
  CHECK(thick.windingFactor > 1.0);
  CHECK(thick.inharmonicity > thin.inharmonicity);
  CHECK(std::strstr(thin.source, "caller arguments") != nullptr);
}

TEST_CASE("time-stepped stick-slip uses rigid reflections and caller friction",
          "[instruments]") {
  const auto violinInstrument = unwrap(violin());
  const StringCourse& course = violinInstrument.courses[2];
  const double beta = 1.0 / 9.0;
  const double speed = 0.2;
  const double muS = 0.8;
  const double muD = 0.3;
  const double density = course.material.linearDensity.value();
  const double wave = std::sqrt(course.tension.value() / density);
  const double impedance = density * wave;
  const double twoZ = 2.0 * impedance;
  // Above the at-rest demand 2 Z |v_b| and below the first bridge echo 4 Z |v_b|.
  const double normal = 3.0 * impedance * speed / muS;
  CHECK(muS * normal > twoZ * speed);
  CHECK(muS * normal < 2.0 * twoZ * speed);
  const BowFriction friction{muS, muD};
  const BowStroke stroke{beta, speed, normal, friction};
  const int bridge = 4;
  const int nut = 32;
  const auto capture = unwrap(captureStickSlip(course, stroke, bridge, nut, 2));
  CHECK(capture.samples.size() == 72);
  CHECK(nearlyEqual(capture.timeStep,
                    (2.0 * course.geometry.vibratingLength.value() / wave) /
                        36.0,
                    0.0, 1e-12));
  CHECK(nearlyEqual(capture.impedance, impedance, 0.0, 1e-12));

  std::vector<double> outBridge(capture.samples.size(), 0.0);
  std::vector<double> outNut(capture.samples.size(), 0.0);
  bool wasStuck = true;
  for (std::size_t i = 0; i < capture.samples.size(); ++i) {
    const StickSlipSample& sample = capture.samples[i];
    CHECK(nearlyEqual(sample.historyVelocity,
                      sample.incomingBridge + sample.incomingNut, 0.0, 0.0));
    if (sample.stuck) {
      CHECK(sample.stringVelocity == stroke.speedMetersPerSecond);
      CHECK(std::fabs(sample.frictionForce) <= muS * normal);
    } else {
      CHECK(nearlyEqual(std::fabs(sample.frictionForce), muD * normal, 0.0,
                        1e-12));
      CHECK(sample.frictionForce * (sample.stringVelocity - speed) < 0.0);
      CHECK(nearlyEqual(sample.stringVelocity,
                        sample.historyVelocity + sample.frictionForce / twoZ,
                        0.0, 1e-12));
    }
    const auto again = unwrap(stickSlipJunction(
        sample.historyVelocity, sample.incomingBridge, sample.incomingNut,
        speed, impedance, normal, friction, wasStuck));
    CHECK(again.stuck == sample.stuck);
    CHECK(nearlyEqual(again.frictionForce, sample.frictionForce, 0.0, 1e-12));
    wasStuck = sample.stuck;
    outBridge[i] = sample.incomingNut + sample.frictionForce / twoZ;
    outNut[i] = sample.incomingBridge + sample.frictionForce / twoZ;
    if (i >= static_cast<std::size_t>(bridge)) {
      CHECK(nearlyEqual(sample.incomingBridge,
                        -outBridge[i - static_cast<std::size_t>(bridge)], 0.0,
                        1e-12));
    } else {
      CHECK(sample.incomingBridge == 0.0);
    }
    if (i >= static_cast<std::size_t>(nut)) {
      CHECK(nearlyEqual(sample.incomingNut,
                        -outNut[i - static_cast<std::size_t>(nut)], 0.0,
                        1e-12));
    } else {
      CHECK(sample.incomingNut == 0.0);
    }
  }
  for (int i = 0; i < bridge; ++i) CHECK(capture.samples[static_cast<std::size_t>(i)].stuck);
  const StickSlipSample& released = capture.samples[static_cast<std::size_t>(bridge)];
  CHECK(!released.stuck);
  CHECK(nearlyEqual(released.incomingBridge, -speed, 0.0, 1e-12));
  CHECK(released.incomingNut == 0.0);
  CHECK(nearlyEqual(released.frictionForce, muD * normal, 0.0, 1e-12));

  BowStroke heavy = stroke;
  heavy.normalForceNewtons = 5.0 * impedance * speed / muS;
  CHECK(muS * heavy.normalForceNewtons > 2.0 * twoZ * speed);
  const auto held = unwrap(captureStickSlip(course, heavy, bridge, nut, 1));
  CHECK(held.samples[static_cast<std::size_t>(bridge)].stuck);
  CHECK(held.samples[static_cast<std::size_t>(bridge)].stringVelocity == speed);

  const double maxNormal = twoZ * speed / (beta * (muS - muD));
  BowStroke tooHeavy = stroke;
  tooHeavy.normalForceNewtons = maxNormal * 1.1;
  CHECK(makeHelmholtzStroke(course, tooHeavy).error() ==
        QuantityError::DomainError);
  CHECK(captureStickSlip(course, tooHeavy, bridge, nut, 1).ok());

  BowStroke upBow = stroke;
  upBow.speedMetersPerSecond = -speed;
  const auto upward = unwrap(captureStickSlip(course, upBow, bridge, nut, 1));
  CHECK(upward.samples.front().stringVelocity == -speed);
  CHECK(!upward.samples[static_cast<std::size_t>(bridge)].stuck);
  CHECK(nearlyEqual(
      upward.samples[static_cast<std::size_t>(bridge)].frictionForce,
      -muD * normal, 0.0, 1e-12));

  const double band = 0.5 * (muS + muD) * normal;
  const double bandHistory = speed - band / twoZ;
  CHECK(unwrap(stickSlipJunction(bandHistory, bandHistory, 0.0, speed,
                                 impedance, normal, friction, true))
            .stuck);
  const auto slipping = unwrap(stickSlipJunction(
      bandHistory, bandHistory, 0.0, speed, impedance, normal, friction, false));
  CHECK(!slipping.stuck);
  CHECK(nearlyEqual(slipping.frictionForce, muD * normal, 0.0, 1e-12));
  const double dynamicHistory = speed - (muD * normal) / twoZ;
  CHECK(unwrap(stickSlipJunction(dynamicHistory, dynamicHistory, 0.0, speed,
                                 impedance, normal, friction, false))
            .stuck);
  const double over = muS * normal * (1.0 + 1e-6);
  const double overHistory = speed - over / twoZ;
  CHECK(!unwrap(stickSlipJunction(overHistory, overHistory, 0.0, speed,
                                  impedance, normal, friction, true))
             .stuck);
  const double staticHistory = speed - (muS * normal) / twoZ;
  CHECK(unwrap(stickSlipJunction(staticHistory, staticHistory, 0.0, speed,
                                 impedance, normal, friction, true))
            .stuck);

  BowStroke wrongBeta = stroke;
  wrongBeta.bridgeFraction = 0.2;
  CHECK(captureStickSlip(course, wrongBeta, bridge, nut, 1).error() ==
        QuantityError::InvalidArgument);
  CHECK(captureStickSlip(course, stroke, 0, nut, 1).error() ==
        QuantityError::InvalidArgument);
  CHECK(captureStickSlip(course, stroke, bridge, nut, 0).error() ==
        QuantityError::InvalidArgument);
  CHECK(stickSlipJunction(0.0, 0.0, 0.0, 0.0, impedance, normal, friction, true)
            .error() == QuantityError::InvalidArgument);
  CHECK(stickSlipJunction(0.0, 0.0, 0.0, speed, 0.0, normal, friction, true)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("a moving bridge re-solves the string from a caller resistance or mode",
          "[instruments]") {
  auto courseOf = [](double length, double mu, double tension) {
    return StringCourse{
        StringGeometry{DistanceMeters{length}},
        StringMaterial{LinearDensityKgPerM{mu}},
        StringTuning{"synthetic",
                     FrequencyHz{std::sqrt(tension / mu) / (2.0 * length)}},
        TensionNewtons{tension},
        TabulatedRange{},
        TabulatedRange{},
        0.0,
        false,
        "synthetic"};
  };
  const double length = 0.5;
  const double density = 0.01;
  const double speed = 200.0;
  const double tension = density * speed * speed;
  const StringCourse course = courseOf(length, density, tension);
  const double impedance = density * speed;
  const double open = speed / (2.0 * length);
  const double heavy = 20.0 * impedance;
  const auto fundamental = unwrap(resistiveBridgeMode(course, heavy, 1));
  CHECK(nearlyEqual(fundamental.frequencyHz, open, 0.0, 1e-12));
  CHECK(nearlyEqual(fundamental.decayPerSecond,
                    (speed / length) * std::atanh(impedance / heavy), 0.0,
                    1e-12));
  CHECK(nearlyEqual(fundamental.reflection,
                    (impedance - heavy) / (impedance + heavy), 0.0, 1e-12));
  const auto octave = unwrap(resistiveBridgeMode(course, heavy, 2));
  CHECK(nearlyEqual(octave.frequencyHz, 2.0 * open, 0.0, 1e-12));
  CHECK(nearlyEqual(octave.decayPerSecond, fundamental.decayPerSecond, 0.0,
                    1e-12));
  const auto stifferBridge = unwrap(resistiveBridgeMode(course, 2.0 * heavy, 1));
  CHECK(nearlyEqual(stifferBridge.decayPerSecond,
                    (speed / length) * std::atanh(impedance / (2.0 * heavy)),
                    0.0, 1e-12));
  CHECK(stifferBridge.decayPerSecond < fundamental.decayPerSecond);

  const double light = 0.5 * impedance;
  const auto freed = unwrap(resistiveBridgeMode(course, light, 1));
  CHECK(nearlyEqual(freed.frequencyHz, 0.5 * open, 0.0, 1e-12));
  CHECK(nearlyEqual(freed.decayPerSecond,
                    (speed / length) * std::atanh(light / impedance), 0.0,
                    1e-12));
  CHECK(nearlyEqual(unwrap(resistiveBridgeMode(course, light, 2)).frequencyHz,
                    1.5 * open, 0.0, 1e-12));
  CHECK(resistiveBridgeMode(course, impedance, 1).error() ==
        QuantityError::DomainError);
  CHECK(resistiveBridgeMode(course, 0.0, 1).error() ==
        QuantityError::InvalidArgument);
  CHECK(resistiveBridgeMode(course, -heavy, 1).error() ==
        QuantityError::InvalidArgument);
  CHECK(resistiveBridgeMode(course, heavy, 0).error() ==
        QuantityError::InvalidArgument);

  const StringCourse longer = courseOf(2.0 * length, density, tension);
  const auto stretched = unwrap(resistiveBridgeMode(longer, heavy, 1));
  CHECK(nearlyEqual(stretched.frequencyHz, 0.5 * open, 0.0, 1e-12));
  CHECK(nearlyEqual(stretched.decayPerSecond, 0.5 * fundamental.decayPerSecond,
                    0.0, 1e-12));

  const double bodyHz = 480.0;
  const double mass = 0.002;
  const auto resonator =
      unwrap(makeBodyResonator(BodyModeSpec{bodyHz, 1.0e8, mass}));
  const auto solved = unwrap(movingBridgeModes(course, resonator, 1));
  const auto coupled = unwrap(coupleToBridge(
      course, BridgeOscillator{mass, resonator.stiffness, 0.0}));
  const auto coupledFrequency = unwrap(coupledModeFrequencies(coupled, 1));
  CHECK(std::fabs(solved[0].frequencyHz - open) > 1.0e-3 * open);
  CHECK(nearlyEqual(solved[0].frequencyHz, coupledFrequency[0].value(), 0.0,
                    1.0e-5));
  const auto partner =
      unwrap(makeBodyResonator(BodyModeSpec{900.0, 1.0e8, 0.004}));
  const BodyResonator pair[] = {resonator, partner};
  const auto summed = unwrap(movingBridgeModes(course, pair, 1));
  CHECK(std::fabs(summed[0].frequencyHz - solved[0].frequencyHz) >
        1.0e-4 * open);

  const auto played =
      unwrap(makeBodyResonator(BodyModeSpec{220.0, 25.0, 0.04}));
  const double atResonance =
      unwrap(bridgeResistance(played, played.resonanceHz));
  CHECK(nearlyEqual(atResonance, played.damping, 0.0, 1e-12));
  const auto mobility = unwrap(bodyMobility(played, played.resonanceHz));
  CHECK(mobility.imag() == 0.0);
  CHECK(nearlyEqual(atResonance,
                    mobility.real() /
                        (mobility.real() * mobility.real() +
                         mobility.imag() * mobility.imag()),
                    0.0, 1e-12));
  const double below =
      unwrap(bridgeResistance(played, 0.5 * played.resonanceHz));
  CHECK(below != atResonance);

  const auto violinInstrument = unwrap(violin());
  const StringCourse& violinA = violinInstrument.courses[2];
  const double beta = 1.0 / 9.0;
  const double bowSpeed = 0.2;
  const double muS = 0.8;
  const double muD = 0.3;
  const double violinWave =
      std::sqrt(violinA.tension.value() / violinA.material.linearDensity.value());
  const double violinZ = violinA.material.linearDensity.value() * violinWave;
  const double maxNormal = 2.0 * violinZ * bowSpeed / (beta * (muS - muD));
  const BowStroke stroke{beta, bowSpeed, 0.5 * maxNormal, {muS, muD}};
  const auto motion = unwrap(makeHelmholtzStroke(violinA, stroke));
  const auto minimum = unwrap(schellengMinimumForce(motion, atResonance));
  CHECK(nearlyEqual(minimum,
                    motion.maxNormalForce * motion.impedance /
                        (4.0 * motion.beta * atResonance),
                    0.0, 1e-12));
  CHECK(makeHelmholtzStroke(violinA, stroke, atResonance).ok());
  const auto violinHeavy =
      unwrap(resistiveBridgeMode(violinA, 30.0 * violinZ, 1));
  const auto violinHeavier =
      unwrap(resistiveBridgeMode(violinA, 60.0 * violinZ, 1));
  CHECK(violinHeavier.decayPerSecond < violinHeavy.decayPerSecond);
  CHECK(nearlyEqual(violinHeavy.frequencyHz,
                    unwrap(openStringFrequency(violinA)).value(), 0.0, 1e-12));
  CHECK(30.0 * violinZ != violinInstrument.body.length.value());
  CHECK(bridgeResistance(std::span<const BodyResonator>{}, 220.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(movingBridgeModes(course, std::span<const BodyResonator>{}, 1).error() ==
        QuantityError::InvalidArgument);
}
