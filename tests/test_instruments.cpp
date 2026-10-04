// Instruments — bowed strings, a concert pedal harp, a French double
// harpsichord, Helmholtz bow, and one body mode (docs/48).
//
// The open string is the ideal-string fundamental at the pitch-adapter
// frequency. The nominal density is Guettler's range midpoint, and the
// tension that hits that pitch must sit inside his published tension
// interval. A pluck is the existing modal sum, checked against d'Alembert
// within the truncation bound. The bridge hook is the coupled-string
// limits (rigid end, free end). Hammer and reed are refused. A bow is a
// steady Helmholtz stroke: stick/slip velocity, bridge-force sawtooth,
// and Schelleng's maximum normal force. A body mode filters that force
// with a caller-supplied resonance, Q, and modal mass. A hard hammer is
// the infinite-string contact law until the first reflection returns. A
// reed is the quasi-static Bernoulli slit, not a bore and not a string.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include <mmathen/instruments/body.h>
#include <mmathen/instruments/bow.h>
#include <mmathen/instruments/hammer.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/reed.h>
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
