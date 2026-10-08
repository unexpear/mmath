// Bow, felt hammer, mallet, and bore valve, stepped into samples.
//
// Every force, pressure, loss duration, quality factor, and reflection
// magnitude below is this test's argument. None of them is stored on an
// instrument. Listening files are peak-scaled to 0.5 so a displacement
// in metres or a pressure in pascals is audible. That scale is playback
// only: the assertions read the unscaled samples.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/instruments/bowed_string.h>
#include <mmathen/instruments/cello.h>
#include <mmathen/instruments/cymbal.h>
#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/drum.h>
#include <mmathen/instruments/mallet_voice.h>
#include <mmathen/instruments/piano.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/instruments/struck_string.h>
#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/oscillator.h>
#include <mmathen/sink/wav.h>

using namespace mmathen;

namespace {

template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

constexpr double kFs = 48000.0;
const SampleRateHz kRate{kFs};
constexpr double kAir = 101325.0;
constexpr double kTemp = 293.15;

const InstrumentRenderStatus* statusNamed(const char* name) {
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (std::strcmp(kInstrumentRenderStatus[i].name, name) == 0)
      return &kInstrumentRenderStatus[i];
  }
  return nullptr;
}

std::vector<double> renderOf(IVoice& voice, std::uint64_t frames) {
  std::vector<double> samples(static_cast<std::size_t>(frames));
  REQUIRE(voice.renderBlock(samples.data(), FrameCount{frames}).ok());
  for (double sample : samples) REQUIRE(std::isfinite(sample));
  return samples;
}

double peakOf(const std::vector<double>& samples, std::size_t begin,
              std::size_t end) {
  double peak = 0.0;
  for (std::size_t i = begin; i < end; ++i)
    peak = std::max(peak, std::fabs(samples[i]));
  return peak;
}

// Stick onsets of a Helmholtz corner. The velocity equals the bow speed
// only while stuck; ripple during slip is not a second period.
double stickPitch(const std::vector<double>& samples, std::size_t begin,
                  std::size_t end, double bowSpeed) {
  double previous = -1.0;
  double sum = 0.0;
  int count = 0;
  for (std::size_t i = begin + 1; i < end; ++i) {
    const bool stuck = std::fabs(samples[i] - bowSpeed) < 1.0e-8;
    const bool was = std::fabs(samples[i - 1] - bowSpeed) < 1.0e-8;
    if (stuck && !was) {
      if (previous >= 0.0) {
        sum += static_cast<double>(i) - previous;
        ++count;
      }
      previous = static_cast<double>(i);
    }
  }
  REQUIRE(count >= 8);
  return kFs * static_cast<double>(count) / sum;
}

double pitchNear(const std::vector<double>& samples, std::size_t begin,
                 double lowHz, double highHz) {
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() +
                                 static_cast<std::ptrdiff_t>(begin + 8192));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris,
                                 FrequencyHz{lowHz}, FrequencyHz{highHz}))
      .frequency.value();
}

double amplitudeNear(const std::vector<double>& samples, std::size_t begin,
                     double lowHz, double highHz) {
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() +
                                 static_cast<std::ptrdiff_t>(begin + 4096));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris,
                                 FrequencyHz{lowHz}, FrequencyHz{highHz}))
      .amplitude;
}

void writeListening(const std::vector<double>& samples, const char* path) {
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  const double gain = 0.5 / peak;
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  auto opened =
      WavSink::open(path, kRate, FrameCount{static_cast<std::uint64_t>(samples.size())});
  REQUIRE(opened.ok());
  auto sink = std::move(opened).value();
  REQUIRE(sink.consume(scaled.data(),
                       FrameCount{static_cast<std::uint64_t>(scaled.size())}));
  REQUIRE(sink.close().ok());
}

LossModel courseLoss(const StringCourse& course, double t1, double t2) {
  const auto ideal = unwrap(courseString(course));
  const double frequency = course.tuning.frequency.value();
  return unwrap(lossFromT60(ideal.speed, FrequencyHz{frequency}, DurationSeconds{t1},
                            FrequencyHz{6.0 * frequency}, DurationSeconds{t2}));
}

void checkDecay(const std::vector<double>& samples, std::size_t first,
                double lowHz, double highHz, double decayRate) {
  const double early = amplitudeNear(samples, first, lowHz, highHz);
  const double late = amplitudeNear(samples, first + 4096, lowHz, highHz);
  REQUIRE(early > 0.0);
  REQUIRE(late > 0.0);
  const double dt = 4096.0 / kFs;
  const double expected = std::exp(-decayRate * dt);
  const double ratio = late / early;
  CHECK(std::fabs(ratio - expected) / expected < 0.05);
}

MembraneHead tunedHead(const MembraneShell& shell, double frequencyHz) {
  const double densityMin = 0.20;
  const double densityMax = 0.30;
  const double density = 0.5 * (densityMin + densityMax);
  const double alpha = unwrap(besselJZero(0, 1));
  const double wave =
      constants::twoPi * shell.headRadius.value() * frequencyHz / alpha;
  const double tension = density * wave * wave;
  return unwrap(tuneMembraneHead(shell, frequencyHz, densityMin, densityMax,
                                 tension * 0.5, tension * 1.5));
}

MalletStrike testMallet(double radial, double mass, double speed, double stiffness,
                        double quality) {
  return MalletStrike{radial, 0.0, mass, speed, stiffness, 1.0, 0.0, quality};
}

}  // namespace

TEST_CASE("a constant-force oscillator steps its own decay", "[contact]") {
  const double omega = 2.0 * constants::pi * 440.0;
  const double sigma = 3.0;
  const double dt = 1.0 / kFs;
  OscillatorState state{1.0e-3, 0.0};
  const int steps = 4800;
  for (int i = 0; i < steps; ++i) {
    state = unwrap(stepConstantForceOscillator(state, omega, sigma, 0.0, dt));
  }
  const double time = static_cast<double>(steps) * dt;
  const double envelope = 1.0e-3 * std::exp(-sigma * time);
  CHECK(std::fabs(state.displacement) <= envelope * 1.001);
  CHECK(stepConstantForceOscillator(state, omega, omega, 0.0, dt).error() ==
        QuantityError::DomainError);
}

TEST_CASE("bow, hammer, and mallet still leave the audible bar unmet", "[contact]") {
  REQUIRE(kInstrumentRenderStatusCount == 26);
  CHECK(instrumentsThatRenderAudio() == 11);
  CHECK(std::strstr(statusNamed("violin")->resonator, "waveguide") != nullptr);
  CHECK(std::strstr(statusNamed("violin")->excitation, "bow") != nullptr);
  CHECK_FALSE(statusNamed("piano")->rendersAudio);
  CHECK(std::strstr(statusNamed("piano")->gap, "hammer") != nullptr);
  CHECK(std::strstr(statusNamed("clarinet")->gap, "bore") != nullptr);
  CHECK(statusNamed("clarinet")->rendersAudio);
  CHECK(statusNamed("flute")->rendersAudio);
  CHECK(statusNamed("saxophone")->rendersAudio);
  CHECK(statusNamed("trumpet")->rendersAudio);
  CHECK(std::strstr(statusNamed("trumpet")->gap, "flare") != nullptr);
  CHECK(statusNamed("trombone")->rendersAudio);
  CHECK(std::strstr(statusNamed("saxophone")->resonator, "cone") != nullptr);
  CHECK(std::strstr(statusNamed("bass drum")->gap, "port") != nullptr);
  CHECK_FALSE(statusNamed("splash")->rendersAudio);
}

TEST_CASE("a bow on a lossy waveguide has the loop pitch and decay", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double mu = course.material.linearDensity.value();
  const double tension = course.tension.value();
  const double impedance = mu * std::sqrt(tension / mu);
  const double beta = 0.125;
  const double speed = 0.2;
  const double muS = 0.8;
  const double muD = 0.3;
  const double maximum =
      2.0 * impedance * speed / (beta * (muS - muD));
  const BowStroke stroke{beta, speed, 0.15 * maximum, BowFriction{muS, muD}};
  const LossModel loss = courseLoss(course, 2.0, 0.25);
  auto made = BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate,
                                      12000);
  REQUIRE(made.ok());
  auto voice = std::move(made).value();
  CHECK(std::fabs(voice.fundamentalHz() - 440.0) < 0.05);
  const auto samples = renderOf(voice, 32768);
  CHECK(peakOf(samples, 2000, 10000) > 0.05);
  CHECK(samples[4000] > -1.0);
  double lowest = samples[2000];
  for (std::size_t i = 2000; i < 10000; ++i)
    lowest = std::min(lowest, samples[i]);
  CHECK(lowest < 0.0);
  // The corner's travel is the loss filter's group delay, a few samples
  // longer than the linear mode, so the speaking pitch sits near the loop
  // rather than on it. After the bow lifts, the transparent junction is
  // the tuned loop and the ring is that mode.
  const double speaking = stickPitch(samples, 4000, 11000, speed);
  CHECK(std::fabs(speaking - voice.fundamentalHz()) / voice.fundamentalHz() < 0.05);
  const double heard = pitchNear(samples, 16384, 400.0, 480.0);
  CHECK(std::fabs(heard - voice.fundamentalHz()) < 2.0);
  checkDecay(samples, 16384, 400.0, 480.0, voice.fundamentalDecay());
  writeListening(samples, "/opt/cursor/artifacts/mmath-violin-bow.wav");

  voice.reset();
  auto again = renderOf(voice, 1024);
  std::vector<double> coarse(1024);
  voice.reset();
  for (int i = 0; i < 4; ++i) {
    REQUIRE(voice
                .renderBlock(coarse.data() + static_cast<std::size_t>(i) * 256,
                             FrameCount{256})
                .ok());
  }
  for (int i = 0; i < 1024; ++i) CHECK(coarse[static_cast<std::size_t>(i)] == again[static_cast<std::size_t>(i)]);

  const char* bows[] = {"viola", "cello", "double bass"};
  const StringCourse others[] = {unwrap(viola()).courses[2],
                                 unwrap(cello()).courses[3],
                                 unwrap(doubleBass()).courses[3]};
  const char* paths[] = {"/opt/cursor/artifacts/mmath-viola-bow.wav",
                         "/opt/cursor/artifacts/mmath-cello-bow.wav",
                         "/opt/cursor/artifacts/mmath-double-bass-bow.wav"};
  for (int i = 0; i < 3; ++i) {
    const double otherMu = others[i].material.linearDensity.value();
    const double otherT = others[i].tension.value();
    const double otherZ = otherMu * std::sqrt(otherT / otherMu);
    const double otherMax =
        2.0 * otherZ * speed / (beta * (muS - muD));
    const BowStroke otherStroke{beta, speed, 0.15 * otherMax,
                                BowFriction{muS, muD}};
    auto other = unwrap(BowedStringVoice::create(
        others[i], otherStroke, courseLoss(others[i], 2.0, 0.25), 0.0, 4, 0,
        kRate, 8000));
    auto rendered = renderOf(other, 16384);
    CHECK(peakOf(rendered, 1000, 8000) > 1.0e-3);
    CHECK(std::strstr(statusNamed(bows[i])->excitation, "bow") != nullptr);
    writeListening(rendered, paths[i]);
  }
  CHECK(BowedStringVoice::create(course, BowStroke{0.0, speed, 0.1, {muS, muD}},
                                 loss, 0.0, 4, 0, kRate, 10)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("a felt hammer rings the stiff string after contact", "[contact]") {
  auto bank = unwrap(concertGrandStringBank());
  const PianoStringSpec* spec = findPianoString(bank, "C#5");
  REQUIRE(spec != nullptr);
  const StringCourse course = unwrap(tunePianoString(*spec));
  const WaveSystem1D stiff = unwrap(tunedPianoString(*spec));
  const LossModel loss = courseLoss(course, 4.0, 0.4);
  const HammerStrike strike{0.12, 0.008, 0.8};
  auto elastic = unwrap(FeltHammerVoice::create(
      course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4, kRate, 0.3));
  const double pole = elastic.fundamentalHz();
  auto system = unwrap(pluckedCourseSystem(course, loss, stiff.stiffness));
  const double mode = unwrap(modeFrequency(system, 1)).value();
  CHECK(std::fabs(pole - mode) < 1.0e-6);
  const auto samples = renderOf(elastic, 24576);
  CHECK(peakOf(samples, 0, 2000) > 1.0e-5);
  const double heard = pitchNear(samples, 8192, pole - 40.0, pole + 40.0);
  CHECK(std::fabs(heard - pole) < 2.0);
  checkDecay(samples, 8192, pole - 40.0, pole + 40.0, elastic.fundamentalDecay());
  writeListening(samples, "/opt/cursor/artifacts/mmath-piano-hammer.wav");

  auto viscous = unwrap(FeltHammerVoice::create(
      course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.4, 4, kRate, 0.3));
  const auto damped = renderOf(viscous, 4096);
  CHECK(peakOf(damped, 0, 4096) < peakOf(samples, 0, 4096));
  CHECK(peakOf(damped, 0, 4096) > 0.0);
}

TEST_CASE("a mallet rings a membrane mode and a free-plate mode", "[contact]") {
  const auto kit = unwrap(yamahaTourCustomKit());
  const MembraneHead bass = tunedHead(kit.bassDrum, 90.0);
  const MalletStrike strike = testMallet(0.0, 0.03, 1.5, 8.0e4, 40.0);
  auto voice = unwrap(MembraneMalletVoice::create(bass, strike, 0, 1, kRate, 0.0, 0.0));
  const double omega = constants::twoPi * 90.0;
  const double sigma = omega / (2.0 * 40.0);
  const double damped =
      std::sqrt(omega * omega - sigma * sigma) / constants::twoPi;
  CHECK(std::fabs(voice.fundamentalHz() - damped) < 1.0e-6);
  const auto samples = renderOf(voice, 24576);
  CHECK(peakOf(samples, 0, 4000) > 1.0e-6);
  const double heard = pitchNear(samples, 8192, 70.0, 110.0);
  CHECK(std::fabs(heard - voice.fundamentalHz()) < 1.0);
  checkDecay(samples, 8192, 70.0, 110.0, voice.fundamentalDecay());
  writeListening(samples, "/opt/cursor/artifacts/mmath-bass-drum.wav");

  const MembraneShell* shells[] = {&kit.rackTom10, &kit.rackTom12, &kit.floorTom,
                                   &kit.snare55, &kit.snare65};
  const char* names[] = {"/opt/cursor/artifacts/mmath-rack-tom-10.wav",
                         "/opt/cursor/artifacts/mmath-rack-tom-12.wav",
                         "/opt/cursor/artifacts/mmath-floor-tom.wav",
                         "/opt/cursor/artifacts/mmath-snare-55.wav",
                         "/opt/cursor/artifacts/mmath-snare-65.wav"};
  const double pitches[] = {140.0, 110.0, 80.0, 180.0, 160.0};
  for (int i = 0; i < 5; ++i) {
    const MembraneHead head = tunedHead(*shells[i], pitches[i]);
    auto tom = unwrap(
        MembraneMalletVoice::create(head, strike, 0, 1, kRate, 0.0, 0.0));
    auto rendered = renderOf(tom, 16384);
    CHECK(peakOf(rendered, 0, 4000) > 1.0e-6);
    writeListening(rendered, names[i]);
  }

  const auto set = unwrap(sabianXsrCompleteSet());
  const CymbalMaterial material{0.001, 1.0e11, 7800.0, 0.33};
  const MalletStrike plateStrike = testMallet(0.45, 0.02, 1.2, 4.0e4, 30.0);
  const int nodal = 2;
  const int radial = 1;
  auto splash = unwrap(PlateMalletVoice::create(
      set.splash, material, plateStrike, &nodal, &radial, 1, kRate, 0.45, 0.0));
  const double plateHz =
      unwrap(freeCircularPlateFrequency(set.splash.radius, material.thicknessMeters,
                                        material.youngsModulusPa,
                                        material.densityKgPerM3, material.poisson,
                                        2, 1))
          .value();
  const double plateOmega = constants::twoPi * plateHz;
  const double plateSigma = plateOmega / (2.0 * 30.0);
  const double plateDamped =
      std::sqrt(plateOmega * plateOmega - plateSigma * plateSigma) /
      constants::twoPi;
  CHECK(std::fabs(splash.fundamentalHz() - plateDamped) < 1.0e-4);
  const auto plate = renderOf(splash, 32768);
  CHECK(peakOf(plate, 0, 8000) > 1.0e-8);
  const double plateHeard =
      pitchNear(plate, 8192, plateDamped - 30.0, plateDamped + 30.0);
  CHECK(std::fabs(plateHeard - splash.fundamentalHz()) < 1.5);
  checkDecay(plate, 8192, plateDamped - 30.0, plateDamped + 30.0,
             splash.fundamentalDecay());
  writeListening(plate, "/opt/cursor/artifacts/mmath-splash.wav");

  const CymbalPlate* plates[] = {&set.hiHatTop, &set.crash16, &set.crash18,
                                 &set.china, &set.ride};
  const char* platePaths[] = {"/opt/cursor/artifacts/mmath-hi-hat.wav",
                              "/opt/cursor/artifacts/mmath-crash-16.wav",
                              "/opt/cursor/artifacts/mmath-crash-18.wav",
                              "/opt/cursor/artifacts/mmath-china.wav",
                              "/opt/cursor/artifacts/mmath-ride.wav"};
  for (int i = 0; i < 5; ++i) {
    auto cymbal = unwrap(PlateMalletVoice::create(
        *plates[i], material, plateStrike, &nodal, &radial, 1, kRate, 0.45, 0.0));
    auto rendered = renderOf(cymbal, 16384);
    CHECK(peakOf(rendered, 0, 8000) > 1.0e-8);
    writeListening(rendered, platePaths[i]);
  }
  CHECK(MembraneMalletVoice::create(bass, testMallet(0.0, 0.03, 1.5, 8.0e4, 0.4),
                                    0, 1, kRate, 0.0, 0.0)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("a reed, a lip, and a jet step a bore delay", "[contact]") {
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  ClarinetSpec clarinetSpec{0.45, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                            1.0e-6, 1.0e-4, 1.0e-3, 0.01};
  const auto clarinet = unwrap(makeClarinet(clarinetSpec));
  const ValveBlow reedBlow{4000.0, 16000, 0.97, 2.0e-5};
  auto reed = unwrap(BoreValveVoice::fromClarinet(clarinet, reedBlow, kRate));
  CHECK(std::fabs(reed.fundamentalHz() - speed / (4.0 * 0.45)) /
            (speed / (4.0 * 0.45)) <
        0.02);
  const auto reedSamples = renderOf(reed, 32768);
  CHECK(peakOf(reedSamples, 4000, 14000) > 100.0);
  const double reedHeard = pitchNear(reedSamples, 4096, 150.0, 230.0);
  CHECK(std::fabs(reedHeard - reed.fundamentalHz()) < 3.0);
  const double reedRing = pitchNear(reedSamples, 18000, 150.0, 230.0);
  CHECK(std::fabs(reedRing - reed.fundamentalHz()) < 0.5);
  checkDecay(reedSamples, 18432, 150.0, 230.0, reed.reflectionDecay());
  writeListening(reedSamples, "/opt/cursor/artifacts/mmath-clarinet.wav");

  SaxophoneSpec saxSpec{0.45, 0.12, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                        1.0e-6, 1.0e-4, 1.0e-3, 0.01};
  const auto saxophone = unwrap(makeSaxophone(saxSpec));
  auto saxVoice =
      unwrap(BoreValveVoice::fromSaxophoneCylinder(saxophone, reedBlow, kRate));
  CHECK(std::fabs(saxVoice.fundamentalHz() - reed.fundamentalHz()) < 0.05);
  CHECK(std::fabs(saxophone.fundamentalHz - saxVoice.fundamentalHz()) > 5.0);
  const auto saxSamples = renderOf(saxVoice, 32768);
  const double saxRing = pitchNear(saxSamples, 18000, 150.0, 230.0);
  CHECK(std::fabs(saxRing - saxVoice.fundamentalHz()) < 0.5);
  checkDecay(saxSamples, 18432, 150.0, 230.0, saxVoice.reflectionDecay());
  writeListening(saxSamples, "/opt/cursor/artifacts/mmath-saxophone.wav");

  const double length = 0.55;
  const double pipe = speed / (2.0 * length);
  const double density =
      unwrap(airDensity(PressurePascals{kAir}, TemperatureKelvin{kTemp})).value();
  const double span = 8.0e-3;
  const double sigma = 0.4;
  const double jetSpeed = span / (sigma * (0.5 / pipe));
  const double overpressure = 0.5 * density * jetSpeed * jetSpeed;
  const double thickness = 1.2e-3;
  FluteSpec fluteSpec{length, 2.8e-4, kAir, kTemp, 1.2e-5, thickness, span, sigma};
  const auto flute = unwrap(makeFlute(fluteSpec));
  const ValveBlow jetBlow{overpressure, 16000, 0.99, 0.25 * thickness};
  auto jet = unwrap(BoreValveVoice::fromFlute(flute, jetBlow, kRate));
  CHECK(jet.outputsFlow());
  CHECK(std::fabs(jet.fundamentalHz() - pipe) / pipe < 0.02);
  const auto jetSamples = renderOf(jet, 32768);
  CHECK(peakOf(jetSamples, 4000, 14000) > 1.0e-4);
  const double jetHeard = pitchNear(jetSamples, 4096, pipe - 40.0, pipe + 40.0);
  CHECK(std::fabs(jetHeard - jet.fundamentalHz()) < 1.0);
  const double jetRing = pitchNear(jetSamples, 18432, pipe - 40.0, pipe + 40.0);
  CHECK(std::fabs(jetRing - jet.fundamentalHz()) < 1.0);
  checkDecay(jetSamples, 18432, pipe - 40.0, pipe + 40.0, jet.reflectionDecay());
  writeListening(jetSamples, "/opt/cursor/artifacts/mmath-flute.wav");

  const double lipMass = 2.0e-5;
  const double lipOmega = 2.0 * constants::pi * (speed / (4.0 * 0.45));
  const double lipStiffness = lipOmega * lipOmega * lipMass;
  TrumpetSpec trumpetSpec{0.45, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5,
                          8.0e-3, lipMass, lipStiffness, 1.5e-5, 0.002};
  const auto trumpet = unwrap(makeTrumpet(trumpetSpec));
  const ValveBlow lipBlow{2500.0, 16000, 0.98, 0.0};
  auto lip = unwrap(BoreValveVoice::fromTrumpet(trumpet, lipBlow, kRate));
  const auto lipSamples = renderOf(lip, 32768);
  CHECK(peakOf(lipSamples, 4000, 14000) > 100.0);
  const double speaking = pitchNear(lipSamples, 4096, 140.0, 360.0);
  const double speakingLater = pitchNear(lipSamples, 8192, 140.0, 360.0);
  CHECK(std::fabs(speaking - speakingLater) < 1.0);
  CHECK(speaking > 0.7 * lip.fundamentalHz());
  CHECK(speaking < 2.2 * lip.fundamentalHz());
  const double lipRing = pitchNear(lipSamples, 18000, 150.0, 230.0);
  CHECK(std::fabs(lipRing - lip.fundamentalHz()) < 0.5);
  checkDecay(lipSamples, 18432, 150.0, 230.0, lip.reflectionDecay());
  writeListening(lipSamples, "/opt/cursor/artifacts/mmath-trumpet.wav");

  TromboneSpec tromboneSpec{0.45, 0.15, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6,
                            4.0e-5, 8.0e-3, lipMass, lipStiffness, 1.5e-5, 0.002};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  CHECK(std::fabs(trombone.bore.lengthMeters - 0.75) < 1.0e-12);
  auto slide = unwrap(BoreValveVoice::fromTrombone(trombone, lipBlow, kRate));
  CHECK(slide.fundamentalHz() < lip.fundamentalHz());
  const auto slideSamples = renderOf(slide, 32768);
  CHECK(peakOf(slideSamples, 4000, 14000) > 50.0);
  const double slideRing = pitchNear(slideSamples, 18000, 80.0, 160.0);
  CHECK(std::fabs(slideRing - slide.fundamentalHz()) < 0.5);
  checkDecay(slideSamples, 18432, 80.0, 160.0, slide.reflectionDecay());
  writeListening(slideSamples, "/opt/cursor/artifacts/mmath-trombone.wav");

  CHECK(BoreValveVoice::fromClarinet(
            clarinet, ValveBlow{4000.0, 100, 0.0, 0.0}, kRate)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("an unflanged open end lengthens the bore and radiates", "[contact]") {
  const double wave = constants::twoPi * 180.0 / 343.0;
  const auto ratio = unwrap(openEndImpedanceRatio(wave, 0.02, 0.012));
  CHECK(std::fabs(ratio.real - 0.25 * wave * wave * 0.02 * 0.02) < 1.0e-12);
  CHECK(std::fabs(ratio.imag - wave * 0.012) < 1.0e-12);
  const auto reflected = unwrap(openEndPressureReflection(ratio));
  const double magnitude = std::hypot(reflected.real, reflected.imag);
  CHECK(magnitude < 1.0);
  CHECK(reflected.real < 0.0);
  const auto atZero = unwrap(openEndPressureReflection(
      unwrap(openEndImpedanceRatio(0.0, 0.02, 0.012))));
  CHECK(std::fabs(atZero.real + 1.0) < 1.0e-12);
  CHECK(std::fabs(atZero.imag) < 1.0e-12);
  CHECK(openEndImpedanceRatio(wave, 0.0, 0.012).error() ==
        QuantityError::InvalidArgument);

  ClarinetSpec clarinetSpec{0.45, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                            1.0e-6, 1.0e-4, 1.0e-3, 0.01};
  const auto clarinet = unwrap(makeClarinet(clarinetSpec));
  const ValveBlow plain{4000.0, 16000, 1.0, 2.0e-5};
  auto closed = unwrap(BoreValveVoice::fromClarinet(clarinet, plain, kRate));
  ValveBlow loaded = plain;
  loaded.radiusMeters = 0.02;
  loaded.endCorrectionMeters = 0.012;
  auto voice = unwrap(BoreValveVoice::fromClarinet(clarinet, loaded, kRate));
  CHECK(voice.fundamentalHz() < closed.fundamentalHz());
  CHECK(voice.reflectionDecay() > closed.reflectionDecay());
  const auto samples = renderOf(voice, 48000);
  CHECK(peakOf(samples, 4000, 14000) > 100.0);
  const double ring = pitchNear(samples, 20000, voice.fundamentalHz() - 25.0,
                                voice.fundamentalHz() + 25.0);
  CHECK(std::fabs(ring - voice.fundamentalHz()) < 1.0);
  checkDecay(samples, 20000, voice.fundamentalHz() - 25.0,
             voice.fundamentalHz() + 25.0, voice.reflectionDecay());
  writeListening(samples, "/opt/cursor/artifacts/mmath-clarinet-open-end.wav");

  ValveBlow wider = loaded;
  wider.radiusMeters = 0.03;
  auto wide = unwrap(BoreValveVoice::fromClarinet(clarinet, wider, kRate));
  CHECK(wide.reflectionDecay() > voice.reflectionDecay());

  const double length = 0.55;
  const double density =
      unwrap(airDensity(PressurePascals{kAir}, TemperatureKelvin{kTemp})).value();
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double area = 2.8e-4;
  FluteSpec fluteSpec{length, area, kAir, kTemp, 1.2e-5, 1.2e-3, 8.0e-3, 0.4};
  const auto flute = unwrap(makeFlute(fluteSpec));
  const double jetSpeed = (8.0e-3 / 0.4) / (length / speed);
  const double jetPressure = 0.5 * density * jetSpeed * jetSpeed;
  ValveBlow jetBlow{jetPressure, 16000, 1.0, 0.25 * 1.2e-3, 0.015, 0.008};
  auto jet = unwrap(BoreValveVoice::fromFlute(flute, jetBlow, kRate));
  CHECK(jet.fundamentalHz() < speed / (2.0 * length));
  const auto jetSamples = renderOf(jet, 48000);
  CHECK(peakOf(jetSamples, 4000, 14000) > 1.0e-6);
  const double jetRing = pitchNear(jetSamples, 20000, jet.fundamentalHz() - 30.0,
                                   jet.fundamentalHz() + 30.0);
  CHECK(std::fabs(jetRing - jet.fundamentalHz()) < 1.5);
  checkDecay(jetSamples, 20000, jet.fundamentalHz() - 30.0,
             jet.fundamentalHz() + 30.0, jet.reflectionDecay());
  writeListening(jetSamples, "/opt/cursor/artifacts/mmath-flute-open-end.wav");
}

TEST_CASE("a reed on a truncated cone is not a cylinder", "[contact]") {
  SaxophoneSpec saxSpec{0.45, 0.12, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                        1.0e-6, 1.0e-4, 1.0e-3, 0.01};
  const auto saxophone = unwrap(makeSaxophone(saxSpec));
  const ValveBlow blow{4000.0, 16000, 1.0, 2.0e-5};
  auto ideal = unwrap(ConeValveVoice::fromSaxophone(saxophone, blow, kRate));
  CHECK(std::fabs(ideal.fundamentalHz() - saxophone.fundamentalHz) < 1.0e-9);
  const double cylinder = unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value() /
                          (4.0 * 0.45);
  CHECK(std::fabs(ideal.fundamentalHz() - cylinder) > 5.0);
  const auto samples = renderOf(ideal, 48000);
  CHECK(peakOf(samples, 4000, 14000) > 100.0);
  const double heard = pitchNear(samples, 20000, ideal.fundamentalHz() - 40.0,
                                 ideal.fundamentalHz() + 40.0);
  CHECK(std::fabs(heard - ideal.fundamentalHz()) < 1.0);
  const double early = amplitudeNear(samples, 24000, ideal.fundamentalHz() - 40.0,
                                     ideal.fundamentalHz() + 40.0);
  const double late = amplitudeNear(samples, 32000, ideal.fundamentalHz() - 40.0,
                                    ideal.fundamentalHz() + 40.0);
  CHECK(std::fabs(late / early - 1.0) < 0.05);
  writeListening(samples, "/opt/cursor/artifacts/mmath-saxophone-cone.wav");

  ValveBlow radiating = blow;
  radiating.radiusMeters = 0.03;
  radiating.endCorrectionMeters = 0.01;
  auto bell = unwrap(ConeValveVoice::fromSaxophone(saxophone, radiating, kRate));
  const auto flared = renderOf(bell, 48000);
  const double flaredHeard =
      pitchNear(flared, 18000, 250.0, 340.0);
  CHECK(flaredHeard < ideal.fundamentalHz() - 5.0);
  const double a1 = amplitudeNear(flared, 16000, 250.0, 340.0);
  const double a2 = amplitudeNear(flared, 24192, 250.0, 340.0);
  const double a3 = amplitudeNear(flared, 32384, 250.0, 340.0);
  const double ratio1 = a2 / a1;
  const double ratio2 = a3 / a2;
  CHECK(ratio1 < 0.9);
  CHECK(std::fabs(ratio1 - ratio2) / ratio1 < 0.08);
  ValveBlow wider = radiating;
  wider.radiusMeters = 0.045;
  auto wide = unwrap(ConeValveVoice::fromSaxophone(saxophone, wider, kRate));
  const auto wideSamples = renderOf(wide, 40000);
  const double wideRatio = amplitudeNear(wideSamples, 24192, 220.0, 340.0) /
                           amplitudeNear(wideSamples, 16000, 220.0, 340.0);
  CHECK(wideRatio < ratio1);
  writeListening(flared, "/opt/cursor/artifacts/mmath-saxophone-bell.wav");

  const double lipMass = 2.0e-5;
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double lipOmega = 2.0 * constants::pi * (speed / (4.0 * 0.45));
  TrumpetSpec trumpetSpec{0.45, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5, 8.0e-3,
                          lipMass, lipOmega * lipOmega * lipMass, 1.5e-5, 0.002};
  const auto trumpet = unwrap(makeTrumpet(trumpetSpec));
  const ValveBlow lipBlow{2500.0, 16000, 1.0, 0.0, 0.03, 0.01};
  auto lip = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.2, lipBlow, kRate));
  const auto lipSamples = renderOf(lip, 48000);
  CHECK(peakOf(lipSamples, 18000, 28000) > 50.0);
  const double lipHeard = pitchNear(lipSamples, 20000, 200.0, 340.0);
  CHECK(lipHeard < lip.fundamentalHz() - 2.0);
  const double lipLater = pitchNear(lipSamples, 28000, 200.0, 340.0);
  CHECK(std::fabs(lipHeard - lipLater) < 1.0);
  writeListening(lipSamples, "/opt/cursor/artifacts/mmath-trumpet-cone.wav");

  TromboneSpec tromboneSpec{0.45, 0.15, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5,
                            8.0e-3, lipMass, lipOmega * lipOmega * lipMass, 1.5e-5,
                            0.002};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  auto slide = unwrap(ConeValveVoice::fromTrombone(trombone, 0.2, lipBlow, kRate));
  CHECK(slide.fundamentalHz() < lip.fundamentalHz());
  const auto slideSamples = renderOf(slide, 48000);
  CHECK(peakOf(slideSamples, 18000, 28000) > 20.0);
  const double slideHeard = pitchNear(slideSamples, 20000, 80.0, 260.0);
  CHECK(slideHeard < lipHeard);
  writeListening(slideSamples, "/opt/cursor/artifacts/mmath-trombone-cone.wav");
}

TEST_CASE("a positive cup compliance lowers the brass ring", "[contact]") {
  const double lipMass = 2.0e-5;
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double lipOmega = 2.0 * constants::pi * (speed / (4.0 * 0.45));
  auto brass = [&](double cupVolume) {
    TrumpetSpec spec{0.45, 1.2e-4, kAir, kTemp, cupVolume, 8.0e-6, 4.0e-5, 8.0e-3,
                     lipMass, lipOmega * lipOmega * lipMass, 1.5e-5, 0.002};
    return unwrap(makeTrumpet(spec));
  };
  const ValveBlow lipBlow{2500.0, 16000, 1.0, 0.0, 0.03, 0.01};
  auto bare = unwrap(ConeValveVoice::fromTrumpet(brass(0.0), 0.2, lipBlow, kRate));
  auto small = unwrap(ConeValveVoice::fromTrumpet(brass(2.0e-6), 0.2, lipBlow, kRate));
  auto large = unwrap(ConeValveVoice::fromTrumpet(brass(8.0e-6), 0.2, lipBlow, kRate));
  const auto bareSamples = renderOf(bare, 48000);
  const auto smallSamples = renderOf(small, 48000);
  const auto largeSamples = renderOf(large, 48000);
  const double barePitch = pitchNear(bareSamples, 20000, 200.0, 340.0);
  const double smallPitch = pitchNear(smallSamples, 20000, 200.0, 340.0);
  const double largePitch = pitchNear(largeSamples, 20000, 180.0, 340.0);
  CHECK(smallPitch < barePitch - 2.0);
  CHECK(largePitch < smallPitch - 5.0);
  CHECK(std::fabs(smallPitch - pitchNear(smallSamples, 28000, 200.0, 340.0)) < 1.0);
  CHECK(peakOf(smallSamples, 18000, 28000) > 50.0);
  const double early = amplitudeNear(smallSamples, 20000, 200.0, 340.0);
  const double middle = amplitudeNear(smallSamples, 28000, 200.0, 340.0);
  const double late = amplitudeNear(smallSamples, 36000, 200.0, 340.0);
  const double ratio1 = middle / early;
  const double ratio2 = late / middle;
  CHECK(ratio1 < 0.9);
  CHECK(std::fabs(ratio1 - ratio2) / ratio1 < 0.08);
  writeListening(smallSamples, "/opt/cursor/artifacts/mmath-trumpet-cup.wav");

  Trumpet negative = brass(0.0);
  negative.acousticCompliance = -1.0e-12;
  CHECK(ConeValveVoice::fromTrumpet(negative, 0.2, lipBlow, kRate).error() ==
        QuantityError::InvalidArgument);

  auto slide = [&](double cupVolume) {
    TromboneSpec spec{0.45, 0.15, 1.2e-4, kAir, kTemp, cupVolume, 8.0e-6, 4.0e-5,
                      8.0e-3, lipMass, lipOmega * lipOmega * lipMass, 1.5e-5, 0.002};
    return unwrap(ConeValveVoice::fromTrombone(unwrap(makeTrombone(spec)), 0.2, lipBlow,
                                              kRate));
  };
  auto slideBare = slide(0.0);
  auto slideCup = slide(8.0e-6);
  const auto slideBareSamples = renderOf(slideBare, 48000);
  const auto slideCupSamples = renderOf(slideCup, 48000);
  const double slideBarePitch = pitchNear(slideBareSamples, 20000, 80.0, 260.0);
  const double slideCupPitch = pitchNear(slideCupSamples, 20000, 80.0, 260.0);
  CHECK(slideCupPitch < slideBarePitch - 2.0);
  CHECK(peakOf(slideCupSamples, 18000, 28000) > 20.0);
  writeListening(slideCupSamples, "/opt/cursor/artifacts/mmath-trombone-cup.wav");
}
