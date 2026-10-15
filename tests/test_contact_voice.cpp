// Bow, felt hammer, mallet, and bore valve, stepped into samples.
//
// Every force, pressure, loss duration, quality factor, and reflection
// magnitude below is this test's argument. None of them is stored on an
// instrument. Listening files are peak-scaled to 0.5 so a displacement
// in metres or a pressure in pascals is audible. That scale is playback
// only: the assertions read the unscaled samples.

#include <mmathen/render/room_convolution.h>
#include <mmathen/music/expressive_timing.h>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <span>
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
#include <mmathen/sink/wav_path.h>

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

// Fraction of Hann-window energy above 0.4 fs. A Nyquist rail lives there.
double energyAboveGuard(const std::vector<double>& samples, std::size_t origin) {
  constexpr int kN = 8192;
  REQUIRE(origin + static_cast<std::size_t>(kN) <= samples.size());
  std::vector<std::complex<double>> buf(static_cast<std::size_t>(kN));
  for (int i = 0; i < kN; ++i) {
    const double window =
        0.5 * (1.0 - std::cos(6.283185307179586 * static_cast<double>(i) / kN));
    buf[static_cast<std::size_t>(i)] =
        samples[origin + static_cast<std::size_t>(i)] * window;
  }
  auto spec = fft(std::move(buf));
  REQUIRE(spec.ok());
  const auto& bins = spec.value();
  double total = 0.0;
  double high = 0.0;
  for (int k = 0; k < kN; ++k) {
    const double power = std::norm(bins[static_cast<std::size_t>(k)]);
    total += power;
    const int folded = std::min(k, kN - k);
    if (static_cast<double>(folded) / static_cast<double>(kN) > 0.4) high += power;
  }
  return total > 0.0 ? high / total : 1.0;
}

double autocorrelationAt(const std::vector<double>& samples, std::size_t origin,
                         double frequencyHz) {
  const int lag = std::max(8, static_cast<int>(std::llround(kFs / frequencyHz)));
  REQUIRE(origin + static_cast<std::size_t>(lag + 4000) <= samples.size());
  double energy0 = 0.0;
  double energy1 = 0.0;
  double cross = 0.0;
  for (int i = 0; i < 4000; ++i) {
    const double u = samples[origin + static_cast<std::size_t>(i)];
    const double v = samples[origin + static_cast<std::size_t>(i + lag)];
    energy0 += u * u;
    energy1 += v * v;
    cross += u * v;
  }
  return cross / std::sqrt(energy0 * energy1);
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

void writeListening(const std::vector<double>& samples, const char* filename) {
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  const double gain = 0.5 / peak;
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  const auto path = wavOutputPath(filename);
  auto opened = WavSink::open(path.string(), kRate,
                              FrameCount{static_cast<std::uint64_t>(samples.size())});
  REQUIRE(opened.ok());
  auto sink = std::move(opened).value();
  REQUIRE(sink.consume(scaled.data(),
                       FrameCount{static_cast<std::uint64_t>(scaled.size())}));
  REQUIRE(sink.close().ok());
  REQUIRE(copyWavToArtifacts(path, filename));
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

TEST_CASE("bow, hammer, and mallet record which instruments render", "[contact]") {
  REQUIRE(kInstrumentRenderStatusCount == 26);
  CHECK(instrumentsThatRenderAudio() == 16);
  CHECK(std::strstr(statusNamed("violin")->resonator, "waveguide") != nullptr);
  CHECK(std::strstr(statusNamed("violin")->excitation, "bow") != nullptr);
  CHECK(statusNamed("violin")->rendersAudio);
  CHECK(std::strstr(statusNamed("violin")->gap, "wolf") != nullptr);
  CHECK(statusNamed("piano")->rendersAudio);
  CHECK(std::strstr(statusNamed("piano")->radiation, "monopole") != nullptr);
  CHECK(std::strstr(statusNamed("piano")->gap, "four") != nullptr);
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
  CHECK(energyAboveGuard(samples, 16384) < 0.01);
  CHECK(autocorrelationAt(samples, 16384, voice.fundamentalHz()) > 0.8);
  checkDecay(samples, 16384, 400.0, 480.0, voice.fundamentalDecay());
  writeListening(samples, "mmath-violin-bow.wav");

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
  const char* paths[] = {"mmath-viola-bow.wav",
                         "mmath-cello-bow.wav",
                         "mmath-double-bass-bow.wav"};
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
    CHECK(energyAboveGuard(rendered, 4096) < 0.01);
    // The bow's speaking period sits a few samples off the tuned loop.
    // The best lag in that neighborhood is the heard pitch.
    {
      const double expect = other.fundamentalHz();
      const int center = std::max(8, static_cast<int>(std::llround(kFs / expect)));
      double best = -2.0;
      int lag = center;
      for (int candidate = std::max(8, center - 12); candidate <= center + 12;
           ++candidate) {
        const double value = autocorrelationAt(rendered, 4096, kFs / candidate);
        if (value > best) {
          best = value;
          lag = candidate;
        }
      }
      CHECK(best > 0.85);
      CHECK(std::fabs(kFs / static_cast<double>(lag) - expect) / expect < 0.04);
    }
    CHECK(std::strstr(statusNamed(bows[i])->excitation, "bow") != nullptr);
    writeListening(rendered, paths[i]);
  }
  CHECK(BowedStringVoice::create(course, BowStroke{0.0, speed, 0.1, {muS, muD}},
                                 loss, 0.0, 4, 0, kRate, 10)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("a bowed bridge radiates through the soundboard", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double frequency = course.tuning.frequency.value();
  const double mu = course.material.linearDensity.value();
  const double tension = course.tension.value();
  const double impedance = mu * std::sqrt(tension / mu);
  const double beta = 0.125;
  const double speed = 0.2;
  const double muS = 0.8;
  const double muD = 0.3;
  const double maximum = 2.0 * impedance * speed / (beta * (muS - muD));
  const BowStroke stroke{beta, speed, 0.15 * maximum, BowFriction{muS, muD}};
  const LossModel loss = courseLoss(course, 2.0, 0.25);
  const BodyModeSpec heavyMode{frequency, 200.0, 20.0};
  const BowBodyDrive heavy{std::span<const BodyModeSpec>(&heavyMode, 1), 0.02, 1.0,
                           kAir, kTemp};
  auto voice = unwrap(BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate,
                                              12000, &heavy));
  CHECK(voice.radiating());
  CHECK(std::fabs(voice.fundamentalHz() - frequency) < 0.05);
  const auto samples = renderOf(voice, 49152);
  CHECK(peakOf(samples, 16000, 24000) > 1.0e-5);
  CHECK(std::fabs(pitchNear(samples, 24576, 400.0, 480.0) - voice.fundamentalHz()) < 1.0);
  const double early = amplitudeNear(samples, 32768, 400.0, 480.0);
  const double late = amplitudeNear(samples, 40960, 400.0, 480.0);
  REQUIRE(early > 0.0);
  const double expected = std::exp(-voice.fundamentalDecay() * (8192.0 / kFs));
  CHECK(std::fabs(late / early - expected) / expected < 0.05);
  writeListening(samples, "mmath-violin-bow-body.wav");

  voice.reset();
  std::vector<double> once(512);
  std::vector<double> blocked(512);
  REQUIRE(voice.renderBlock(once.data(), FrameCount{512}).ok());
  voice.reset();
  for (std::size_t i = 0; i < blocked.size(); i += 128) {
    REQUIRE(voice.renderBlock(blocked.data() + i, FrameCount{128}).ok());
  }
  for (std::size_t i = 0; i < once.size(); ++i)
    CHECK(once[i] == blocked[i]);

  const BodyModeSpec lightMode{frequency, 40.0, 0.5};
  const BowBodyDrive light{std::span<const BodyModeSpec>(&lightMode, 1), 0.02, 1.0, kAir,
                           kTemp};
  const BodyModeSpec heavySameQ{frequency, 40.0, 20.0};
  const BowBodyDrive heavyDrive{std::span<const BodyModeSpec>(&heavySameQ, 1), 0.02, 1.0,
                                kAir, kTemp};
  auto lightVoice =
      unwrap(BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate, 12000, &light));
  auto heavyVoice = unwrap(
      BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate, 12000, &heavyDrive));
  const auto lightSamples = renderOf(lightVoice, 32768);
  const auto heavySamples = renderOf(heavyVoice, 32768);
  const double lightEarly = amplitudeNear(lightSamples, 16384, 400.0, 480.0);
  const double lightLate = amplitudeNear(lightSamples, 24576, 400.0, 480.0);
  const double heavyEarly = amplitudeNear(heavySamples, 16384, 400.0, 480.0);
  const double heavyLate = amplitudeNear(heavySamples, 24576, 400.0, 480.0);
  CHECK(lightLate / lightEarly < 0.40);
  CHECK(heavyLate / heavyEarly > 0.45);
  CHECK(peakOf(lightSamples, 0, 32768) > 5.0 * peakOf(heavySamples, 0, 32768));
  writeListening(lightSamples, "mmath-violin-bow-body-light.wav");

  const char* names[] = {"viola", "cello", "double bass"};
  const StringCourse others[] = {unwrap(viola()).courses[2], unwrap(cello()).courses[3],
                                 unwrap(doubleBass()).courses[3]};
  const char* paths[] = {"mmath-viola-bow-body.wav",
                         "mmath-cello-bow-body.wav",
                         "mmath-double-bass-bow-body.wav"};
  for (int i = 0; i < 3; ++i) {
    const double otherF = others[i].tuning.frequency.value();
    const BodyModeSpec mode{otherF, 200.0, 20.0};
    const BowBodyDrive drive{std::span<const BodyModeSpec>(&mode, 1), 0.02, 1.0, kAir, kTemp};
    const double otherMu = others[i].material.linearDensity.value();
    const double otherT = others[i].tension.value();
    const double otherZ = otherMu * std::sqrt(otherT / otherMu);
    const BowStroke otherStroke{beta, speed, 0.15 * (2.0 * otherZ * speed / (beta * (muS - muD))),
                                BowFriction{muS, muD}};
    auto other = unwrap(BowedStringVoice::create(others[i], otherStroke,
                                                 courseLoss(others[i], 2.0, 0.25), 0.0, 4, 0,
                                                 kRate, 12000, &drive));
    const auto rendered = renderOf(other, 32768);
    CHECK(peakOf(rendered, 16000, 28000) > 1.0e-5);
    CHECK(std::fabs(pitchNear(rendered, 24576, otherF - 40.0, otherF + 40.0) -
                    other.fundamentalHz()) < 1.5);
    CHECK(statusNamed(names[i])->rendersAudio);
    writeListening(rendered, paths[i]);
  }

  const BowBodyDrive empty{std::span<const BodyModeSpec>(), 0.02, 1.0, kAir, kTemp};
  CHECK(BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate, 100, &empty).error() ==
        QuantityError::InvalidArgument);
  const BowBodyDrive negative{std::span<const BodyModeSpec>(&heavyMode, 1), -0.02, 1.0, kAir,
                             kTemp};
  CHECK(BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate, 100, &negative).error() ==
        QuantityError::InvalidArgument);
  const BodyModeSpec overdamped{frequency, 0.4, 20.0};
  const BowBodyDrive soft{std::span<const BodyModeSpec>(&overdamped, 1), 0.02, 1.0, kAir, kTemp};
  CHECK(BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, kRate, 100, &soft).error() ==
        QuantityError::DomainError);
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
  CHECK(energyAboveGuard(samples, 8192) < 0.01);
  CHECK(autocorrelationAt(samples, 8192, pole) > 0.8);
  checkDecay(samples, 8192, pole - 40.0, pole + 40.0, elastic.fundamentalDecay());
  writeListening(samples, "mmath-piano-hammer.wav");

  auto viscous = unwrap(FeltHammerVoice::create(
      course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.4, 4, kRate, 0.3));
  const auto damped = renderOf(viscous, 4096);
  CHECK(peakOf(damped, 0, 4096) < peakOf(samples, 0, 4096));
  CHECK(peakOf(damped, 0, 4096) > 0.0);
}

TEST_CASE("a hammered bridge radiates through the soundboard", "[contact]") {
  auto bank = unwrap(concertGrandStringBank());
  const PianoStringSpec* spec = findPianoString(bank, "C#5");
  REQUIRE(spec != nullptr);
  const StringCourse course = unwrap(tunePianoString(*spec));
  const WaveSystem1D stiff = unwrap(tunedPianoString(*spec));
  const LossModel loss = courseLoss(course, 4.0, 0.4);
  const HammerStrike strike{0.12, 0.008, 0.8};
  const double frequency = course.tuning.frequency.value();
  const BodyModeSpec heavyMode{frequency, 200.0, 1000.0};
  const HammerBodyDrive heavy{std::span<const BodyModeSpec>(&heavyMode, 1), 0.02, 1.0, kAir,
                              kTemp};
  auto voice = unwrap(HammeredStringVoice::create(
      course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4, 4, kRate, heavy));
  CHECK(voice.radiating());
  auto system = unwrap(pluckedCourseSystem(course, loss, stiff.stiffness));
  auto design = unwrap(designTunedLoop(system, kRate, 4, 4));
  const double pole = unwrap(loopPole(design.filters, kRate, 1)).frequency.value();
  CHECK(std::fabs(voice.fundamentalHz() - pole) < 1.0e-9);
  const auto samples = renderOf(voice, 49152);
  CHECK(peakOf(samples, 8000, 16000) > 1.0e-5);
  CHECK(std::fabs(pitchNear(samples, 24576, frequency - 40.0, frequency + 40.0) -
                  voice.fundamentalHz()) < 1.0);
  const double early = amplitudeNear(samples, 32768, frequency - 40.0, frequency + 40.0);
  const double late = amplitudeNear(samples, 40960, frequency - 40.0, frequency + 40.0);
  REQUIRE(early > 0.0);
  const double expected = std::exp(-voice.fundamentalDecay() * (8192.0 / kFs));
  CHECK(std::fabs(late / early - expected) / expected < 0.05);
  writeListening(samples, "mmath-piano-soundboard.wav");

  voice.reset();
  std::vector<double> once(512);
  std::vector<double> blocked(512);
  REQUIRE(voice.renderBlock(once.data(), FrameCount{512}).ok());
  voice.reset();
  for (std::size_t i = 0; i < blocked.size(); i += 128) {
    REQUIRE(voice.renderBlock(blocked.data() + i, FrameCount{128}).ok());
  }
  for (std::size_t i = 0; i < once.size(); ++i) CHECK(once[i] == blocked[i]);

  const BodyModeSpec lightMode{frequency, 40.0, 5.0};
  const HammerBodyDrive light{std::span<const BodyModeSpec>(&lightMode, 1), 0.02, 1.0, kAir,
                              kTemp};
  const BodyModeSpec heavySameQ{frequency, 40.0, 1000.0};
  const HammerBodyDrive heavyQuiet{std::span<const BodyModeSpec>(&heavySameQ, 1), 0.02, 1.0,
                                   kAir, kTemp};
  auto lightVoice = unwrap(HammeredStringVoice::create(
      course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4, 4, kRate, light));
  auto heavyVoice = unwrap(HammeredStringVoice::create(
      course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4, 4, kRate, heavyQuiet));
  const auto lightSamples = renderOf(lightVoice, 49152);
  const auto heavySamples = renderOf(heavyVoice, 49152);
  const double lightEarly =
      amplitudeNear(lightSamples, 32768, frequency - 40.0, frequency + 40.0);
  const double lightLate =
      amplitudeNear(lightSamples, 40960, frequency - 40.0, frequency + 40.0);
  const double heavyEarly =
      amplitudeNear(heavySamples, 32768, frequency - 40.0, frequency + 40.0);
  const double heavyLate =
      amplitudeNear(heavySamples, 40960, frequency - 40.0, frequency + 40.0);
  REQUIRE(lightEarly > 0.0);
  REQUIRE(heavyEarly > 0.0);
  CHECK(lightLate / lightEarly < 0.45);
  CHECK(heavyLate / heavyEarly > 0.65);
  CHECK(peakOf(lightSamples, 0, 8192) > 5.0 * peakOf(heavySamples, 0, 8192));
  CHECK(std::fabs(pitchNear(lightSamples, 24576, frequency - 40.0, frequency + 40.0) -
                  lightVoice.fundamentalHz()) < 1.0);
  writeListening(lightSamples, "mmath-piano-soundboard-light.wav");

  const HammerBodyDrive empty{std::span<const BodyModeSpec>(), 0.02, 1.0, kAir, kTemp};
  CHECK(HammeredStringVoice::create(course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4,
                                    4, kRate, empty)
            .error() == QuantityError::InvalidArgument);
  const HammerBodyDrive negative{std::span<const BodyModeSpec>(&heavyMode, 1), -0.02, 1.0, kAir,
                                 kTemp};
  CHECK(HammeredStringVoice::create(course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4,
                                    4, kRate, negative)
            .error() == QuantityError::InvalidArgument);
  const BodyModeSpec overdamped{frequency, 0.4, 1000.0};
  const HammerBodyDrive soft{std::span<const BodyModeSpec>(&overdamped, 1), 0.02, 1.0, kAir,
                             kTemp};
  CHECK(HammeredStringVoice::create(course, strike, loss, stiff.stiffness, 3200.0, 1.0, 0.0, 4,
                                    4, kRate, soft)
            .error() == QuantityError::DomainError);
  CHECK(statusNamed("piano")->rendersAudio);
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
  writeListening(samples, "mmath-bass-drum.wav");

  const MembraneShell* shells[] = {&kit.rackTom10, &kit.rackTom12, &kit.floorTom,
                                   &kit.snare55, &kit.snare65};
  const char* names[] = {"mmath-rack-tom-10.wav",
                         "mmath-rack-tom-12.wav",
                         "mmath-floor-tom.wav",
                         "mmath-snare-55.wav",
                         "mmath-snare-65.wav"};
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
  writeListening(plate, "mmath-splash.wav");

  const CymbalPlate* plates[] = {&set.hiHatTop, &set.crash16, &set.crash18,
                                 &set.china, &set.ride};
  const char* platePaths[] = {"mmath-hi-hat.wav",
                              "mmath-crash-16.wav",
                              "mmath-crash-18.wav",
                              "mmath-china.wav",
                              "mmath-ride.wav"};
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
  CHECK(peakOf(reedSamples, 4000, 14000) > 1.0e-3);
  const double reedHeard = pitchNear(reedSamples, 4096, 150.0, 230.0);
  CHECK(std::fabs(reedHeard - reed.fundamentalHz()) < 3.0);
  const double reedRing = pitchNear(reedSamples, 18000, 150.0, 230.0);
  CHECK(std::fabs(reedRing - reed.fundamentalHz()) < 0.5);
  checkDecay(reedSamples, 18432, 150.0, 230.0, reed.reflectionDecay());
  writeListening(reedSamples, "mmath-clarinet-delay.wav");

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
  writeListening(saxSamples, "mmath-saxophone-delay.wav");

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
  CHECK_FALSE(jet.outputsFlow());
  CHECK(std::fabs(jet.fundamentalHz() - pipe) / pipe < 0.02);
  const auto jetSamples = renderOf(jet, 32768);
  CHECK(peakOf(jetSamples, 4000, 14000) > 1.0e-4);
  const double jetHeard = pitchNear(jetSamples, 4096, pipe - 40.0, pipe + 40.0);
  CHECK(std::fabs(jetHeard - jet.fundamentalHz()) < 1.0);
  const double jetRing = pitchNear(jetSamples, 18432, pipe - 40.0, pipe + 40.0);
  CHECK(std::fabs(jetRing - jet.fundamentalHz()) < 1.0);
  checkDecay(jetSamples, 18432, pipe - 40.0, pipe + 40.0, jet.reflectionDecay());
  writeListening(jetSamples, "mmath-flute-delay.wav");

  const double lipMass = 2.0e-5;
  const double lipOmega = 2.0 * constants::pi * (speed / (4.0 * 0.45));
  const double lipStiffness = lipOmega * lipOmega * lipMass;
  TrumpetSpec trumpetSpec{0.45, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5,
                          8.0e-3, lipMass, lipStiffness, 1.5e-5, 0.002};
  const auto trumpet = unwrap(makeTrumpet(trumpetSpec));
  const ValveBlow lipBlow{2500.0, 16000, 0.98, 0.0};
  auto lip = unwrap(BoreValveVoice::fromTrumpet(trumpet, lipBlow, kRate));
  const auto lipSamples = renderOf(lip, 32768);
  CHECK(peakOf(lipSamples, 4000, 14000) > 1.0e-3);
  const double speaking = pitchNear(lipSamples, 4096, 140.0, 360.0);
  const double speakingLater = pitchNear(lipSamples, 8192, 140.0, 360.0);
  CHECK(std::fabs(speaking - speakingLater) < 1.0);
  CHECK(speaking > 0.7 * lip.fundamentalHz());
  CHECK(speaking < 2.2 * lip.fundamentalHz());
  const double lipRing = pitchNear(lipSamples, 18000, 150.0, 230.0);
  CHECK(std::fabs(lipRing - lip.fundamentalHz()) < 0.5);
  checkDecay(lipSamples, 18432, 150.0, 230.0, lip.reflectionDecay());
  writeListening(lipSamples, "mmath-trumpet-delay.wav");

  TromboneSpec tromboneSpec{0.45, 0.15, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6,
                            4.0e-5, 8.0e-3, lipMass, lipStiffness, 1.5e-5, 0.002};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  CHECK(std::fabs(trombone.bore.lengthMeters - 0.75) < 1.0e-12);
  auto slide = unwrap(BoreValveVoice::fromTrombone(trombone, lipBlow, kRate));
  CHECK(slide.fundamentalHz() < lip.fundamentalHz());
  const auto slideSamples = renderOf(slide, 32768);
  CHECK(peakOf(slideSamples, 4000, 14000) > 1.0e-3);
  const double slideRing = pitchNear(slideSamples, 18000, 80.0, 160.0);
  CHECK(std::fabs(slideRing - slide.fundamentalHz()) < 0.5);
  checkDecay(slideSamples, 18432, 80.0, 160.0, slide.reflectionDecay());
  writeListening(slideSamples, "mmath-trombone-delay.wav");

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
  CHECK(peakOf(samples, 4000, 14000) > 1.0e-3);
  const double ring = pitchNear(samples, 20000, voice.fundamentalHz() - 25.0,
                                voice.fundamentalHz() + 25.0);
  CHECK(std::fabs(ring - voice.fundamentalHz()) < 1.0);
  checkDecay(samples, 20000, voice.fundamentalHz() - 25.0,
             voice.fundamentalHz() + 25.0, voice.reflectionDecay());
  writeListening(samples, "mmath-clarinet-open-end.wav");

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
  writeListening(jetSamples, "mmath-flute-open-end.wav");
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
  CHECK(peakOf(samples, 4000, 14000) > 1.0e-3);
  const double heard = pitchNear(samples, 20000, ideal.fundamentalHz() - 40.0,
                                 ideal.fundamentalHz() + 40.0);
  CHECK(std::fabs(heard - ideal.fundamentalHz()) < 1.0);
  const double early = amplitudeNear(samples, 24000, ideal.fundamentalHz() - 40.0,
                                     ideal.fundamentalHz() + 40.0);
  const double late = amplitudeNear(samples, 32000, ideal.fundamentalHz() - 40.0,
                                    ideal.fundamentalHz() + 40.0);
  CHECK(std::fabs(late / early - 1.0) < 0.05);
  writeListening(samples, "mmath-saxophone-cone.wav");

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
  writeListening(flared, "mmath-saxophone-bell.wav");

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
  CHECK(peakOf(lipSamples, 18000, 28000) > 1.0e-4);
  const double lipHeard = pitchNear(lipSamples, 20000, 200.0, 340.0);
  CHECK(lipHeard < lip.fundamentalHz() - 2.0);
  const double lipLater = pitchNear(lipSamples, 28000, 200.0, 340.0);
  CHECK(std::fabs(lipHeard - lipLater) < 1.0);
  writeListening(lipSamples, "mmath-trumpet-cone.wav");

  TromboneSpec tromboneSpec{0.45, 0.15, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5,
                            8.0e-3, lipMass, lipOmega * lipOmega * lipMass, 1.5e-5,
                            0.002};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  auto slide = unwrap(ConeValveVoice::fromTrombone(trombone, 0.2, lipBlow, kRate));
  CHECK(slide.fundamentalHz() < lip.fundamentalHz());
  const auto slideSamples = renderOf(slide, 48000);
  CHECK(peakOf(slideSamples, 18000, 28000) > 1.0e-4);
  const double slideHeard = pitchNear(slideSamples, 20000, 80.0, 260.0);
  CHECK(slideHeard < lipHeard);
  writeListening(slideSamples, "mmath-trombone-cone.wav");
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
  CHECK(peakOf(smallSamples, 18000, 28000) > 1.0e-4);
  const double early = amplitudeNear(smallSamples, 20000, 200.0, 340.0);
  const double middle = amplitudeNear(smallSamples, 28000, 200.0, 340.0);
  const double late = amplitudeNear(smallSamples, 36000, 200.0, 340.0);
  const double ratio1 = middle / early;
  const double ratio2 = late / middle;
  CHECK(ratio1 < 0.9);
  CHECK(std::fabs(ratio1 - ratio2) / ratio1 < 0.08);
  writeListening(smallSamples, "mmath-trumpet-cup.wav");

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
  CHECK(peakOf(slideCupSamples, 18000, 28000) > 1.0e-4);
  writeListening(slideCupSamples, "mmath-trombone-cup.wav");
}

// Held bowed notes at a moderate force sit on pitch. Harness renders at
// 0.15 of Schelleng's maximum read A4 -34, C5 -24, D5 -27 cents; at 0.05
// they read within 0.3 cents. The 0.05 fraction is this test's argument
// (UNSOURCED as a player value); the flattening at higher force is the
// bow-force flattening McIntyre, Schumacher and Woodhouse describe
// (JASA 69, 1981), not a delay-line error.
TEST_CASE("held bowed violin notes sit on pitch at moderate force", "[contact]") {
  const StringCourse open = unwrap(violin()).courses[2];
  const double f0 = open.tuning.frequency.value();
  for (double target : {f0, f0 * std::pow(2.0, 3.0 / 12.0), f0 * std::pow(2.0, 5.0 / 12.0)}) {
    const StringCourse course =
        target / f0 < 1.0005 ? open : unwrap(fingerStop(open, 1.0 - f0 / target));
    const double mu = course.material.linearDensity.value();
    const double impedance = mu * std::sqrt(course.tension.value() / mu);
    const double beta = 0.125, speed = 0.2, muS = 0.8, muD = 0.3;
    const double maximum = 2.0 * impedance * speed / (beta * (muS - muD));
    const BowStroke stroke{beta, speed, 0.05 * maximum, BowFriction{muS, muD}};
    auto voice = unwrap(BowedStringVoice::create(course, stroke, courseLoss(course, 2.0, 0.25),
                                                0.0, 4, 0, kRate, 96000));
    const auto samples = renderOf(voice, 72000);
    const double heard = pitchNear(samples, 48000, target * 0.97, target * 1.03);
    const double cents = 1200.0 * std::log2(heard / target);
    INFO("held bow " << target << " Hz: " << cents << " cents");
    WARN("held bow " << target << " Hz: " << cents << " cents");
    CHECK(std::fabs(cents) < 5.0);
  }
}

// Bow expression: Seashore-range vibrato (6 Hz, +/-15 cents, delayed) keeps
// the mean pitch within 5 cents, and a soft raised-cosine bow onset lowers
// the attack peak. Values are this test's arguments (see BowExpression).
TEST_CASE("bow expression keeps mean pitch and softens the attack", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double target = course.tuning.frequency.value();
  const double mu = course.material.linearDensity.value();
  const double impedance = mu * std::sqrt(course.tension.value() / mu);
  const double maximum = 2.0 * impedance * 0.2 / (0.125 * 0.5);
  const BowStroke stroke{0.125, 0.2, 0.05 * maximum, BowFriction{0.8, 0.3}};
  auto make = [&](const BowExpression* e) {
    auto voice = unwrap(BowedStringVoice::create(course, stroke, courseLoss(course, 2.0, 0.25),
                                                0.0, 4, 0, kRate, 96000));
    if (e != nullptr) REQUIRE(voice.setExpression(*e));
    return renderOf(voice, 96000);
  };
  const BowExpression vib{6.0, 15.0, 0.3, 0.3, 0.0, 1.0, 0.0};
  const BowExpression soft{0.0, 0.0, 0.0, 0.0, 0.08, 0.2, 0.0};
  const auto plain = make(nullptr);
  const auto wobble = make(&vib);
  const auto gentle = make(&soft);
  // Mean over 1.0-2.0 s, six vibrato cycles.
  // Mean period: autocorrelation over 1.0-2.0 s (six vibrato cycles),
  // lag searched in 0.01-sample steps by linear interpolation.
  double bestLag = 0.0, best = -2.0;
  for (double lag = kFs / (target * 1.03); lag <= kFs / (target * 0.97); lag += 0.01) {
    const auto whole = static_cast<std::size_t>(lag);
    const double frac = lag - static_cast<double>(whole);
    double xy = 0.0, xx = 0.0, yy = 0.0;
    for (std::size_t n = 48000; n < 96000 - 200; ++n) {
      const double y = (1.0 - frac) * wobble[n + whole] + frac * wobble[n + whole + 1];
      xy += wobble[n] * y; xx += wobble[n] * wobble[n]; yy += y * y;
    }
    const double r = xy / std::sqrt(xx * yy + 1e-300);
    if (r > best) { best = r; bestLag = lag; }
  }
  const double heard = kFs / bestLag;
  const double cents = 1200.0 * std::log2(heard / target);
  WARN("vibrato mean " << cents << " cents");
  CHECK(std::fabs(cents) < 5.0);
  double diff = 0.0;
  for (std::size_t i = 48000; i < 60000; ++i) diff = std::max(diff, std::fabs(wobble[i] - plain[i]));
  CHECK(diff > 1.0e-3 * peakOf(plain, 48000, 60000));
  const double hard = peakOf(plain, 0, 2400);
  const double softPeak = peakOf(gentle, 0, 2400);
  WARN("attack peak " << hard << " -> " << softPeak);
  CHECK(softPeak < hard);
  auto voice = unwrap(BowedStringVoice::create(course, stroke, courseLoss(course, 2.0, 0.25),
                                              0.0, 4, 0, kRate, 96000));
  BowExpression bad = vib;
  bad.vibratoCents = -1.0;
  CHECK_FALSE(voice.setExpression(bad));
  BowStroke louder = stroke;
  louder.normalForceNewtons *= 1.5;
  CHECK(voice.setStroke(louder));
  louder.bridgeFraction = 0.2;
  CHECK_FALSE(voice.setStroke(louder));
}

// Bowed sustain, bow changes, lift, and finger change on one string.
namespace {
double rmsSpan(const std::vector<double>& x, std::size_t a, std::size_t b) {
  double e = 0.0;
  for (std::size_t i = a; i < b; ++i) e += x[i] * x[i];
  return std::sqrt(e / static_cast<double>(b - a));
}
double meanPitch(const std::vector<double>& x, std::size_t a, std::size_t b, double target) {
  double bestLag = 0.0, best = -2.0;
  for (double lag = kFs / (target * 1.03); lag <= kFs / (target * 0.97); lag += 0.01) {
    const auto w = static_cast<std::size_t>(lag);
    const double fr = lag - static_cast<double>(w);
    double xy = 0.0, xx = 0.0, yy = 0.0;
    for (std::size_t n = a; n < b; ++n) {
      const double y = (1.0 - fr) * x[n + w] + fr * x[n + w + 1];
      xy += x[n] * y; xx += x[n] * x[n]; yy += y * y;
    }
    const double r = xy / std::sqrt(xx * yy + 1e-300);
    if (r > best) { best = r; bestLag = lag; }
  }
  return kFs / bestLag;
}
}  // namespace

TEST_CASE("a bowed note sustains, survives bow changes, and rings after the lift",
          "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double f0 = course.tuning.frequency.value();
  // Bow 0.1 of the string from the bridge at 0.2 m/s, in-range force (violinBowStroke).
  const BowStroke stroke = violinBowStroke(course, 0.1, 0.2);
  auto voice = unwrap(OversampledBowedStringVoice::create(
      course, stroke, courseLoss(course, 2.0, 0.25), 0.0, 4, 0, kRate, 480000, 4));  // 10 s of bow
  // 0.65 m of hair at 0.2 m/s: a change every 3.25 s.
  std::vector<double> x(576000);
  std::size_t done = 0;
  for (std::size_t at : {156000u, 312000u, 468000u}) {
    REQUIRE(voice.renderBlock(x.data() + done, FrameCount{at - done}).ok());
    done = at;
    REQUIRE(voice.changeBow(0.1));
  }
  REQUIRE(voice.renderBlock(x.data() + done, FrameCount{x.size() - done}).ok());
  const double early = rmsSpan(x, 96000, 144000);
  const double late = rmsSpan(x, 420000, 466000);
  WARN("sustain rms " << early << " -> " << late << " after changes " << rmsSpan(x, 160800, 165600) << " " << rmsSpan(x, 200000, 240000) << " " << rmsSpan(x, 316800, 321600));
  CHECK(std::fabs(20.0 * std::log10(late / early)) < 3.0);  // measured 1.15 dB over 2-9.7 s
  for (std::size_t at : {156000u, 312000u, 468000u}) {
    // In full Helmholtz motion the stroke after a reversal is within 3 dB
    // (the old +5 dB was the weak regime at 0.075 of the maximum).
    const double after = rmsSpan(x, at + 9600, at + 14400);
    CHECK(std::fabs(20.0 * std::log10(after / early)) < 3.0);
  }
  const double lifted = rmsSpan(x, 504000, 528000);
  const double later = rmsSpan(x, 552000, 576000);
  WARN("after lift " << lifted << " -> " << later);
  CHECK(lifted < early);
  CHECK(later < 0.5 * lifted);

  // Finger change A4 -> C5 on the sounding string: on pitch, no click.
  auto legato = unwrap(OversampledBowedStringVoice::create(
      course, stroke, courseLoss(course, 2.0, 0.25), 0.0, 4, 0, kRate, 96000, 4));
  std::vector<double> y(96000);
  REQUIRE(legato.renderBlock(y.data(), FrameCount{48000}).ok());
  const double c5 = f0 * std::pow(2.0, 3.0 / 12.0);
  REQUIRE(legato.setStoppedFrequency(c5));
  REQUIRE(legato.renderBlock(y.data() + 48000, FrameCount{48000}).ok());
  const double heard = meanPitch(y, 72000, 90000, c5);
  const double cents = 1200.0 * std::log2(heard / c5);
  WARN("finger change C5 " << cents << " cents");
  CHECK(std::fabs(cents) < 10.0);  // measured flattening about 6 cents, see OversampledBowedStringVoice
  double steadyStep = 0.0, joinStep = 0.0;
  for (std::size_t i = 40000; i < 47000; ++i) steadyStep = std::max(steadyStep, std::fabs(y[i] - y[i - 1]));
  for (std::size_t i = 47990; i < 48500; ++i) joinStep = std::max(joinStep, std::fabs(y[i] - y[i - 1]));
  WARN("join step " << joinStep << " steady " << steadyStep);
  CHECK(joinStep < 2.0 * steadyStep);
  CHECK_FALSE(legato.setStoppedFrequency(f0 * 0.5));
}

TEST_CASE("articulation: spiccato lifts the bow, accents push the onset", "[contact]") {
  const BowArticulation spic = articulationForNote(0.08, 100);
  CHECK(std::fabs(spic.contactSeconds - 0.04) < 1e-12);
  CHECK(spic.accentGain > 1.0);
  const BowArticulation det = articulationForNote(0.5, 60);
  CHECK(det.contactSeconds == 0.0);
  CHECK(det.accentGain == 1.0);
  BowArticulation bad;
  bad.accentGain = 0.0;
  const StringCourse course = unwrap(violin()).courses[2];
  auto run = [&](const BowArticulation& a) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, kViolinPlayingBeta, 0.2), unwrap(violinStringLoss(course)),
        0.0, 4, 0, kRate, 96000, 4));
    std::vector<double> x(96000);
    REQUIRE(voice.renderBlock(x.data(), FrameCount{48000}).ok());
    REQUIRE(voice.articulate(a));
    CHECK_FALSE(voice.articulate(bad) );
    REQUIRE(voice.renderBlock(x.data() + 48000, FrameCount{48000}).ok());
    return rmsSpan(x, 72000, 96000);
  };
  BowArticulation lift;
  lift.contactSeconds = 0.05;
  const double held = run(BowArticulation{});
  const double lifted = run(lift);
  CHECK(held > 0.05);
  // Lifted 0.45 s earlier: the string rings down (T60 2 s at f0, faster above).
  CHECK(lifted < 0.8 * held);
}

TEST_CASE("expressive timing: tempo scale, mean-1 phrase arch, articulation ratio", "[contact]") {
  std::vector<ScoreNote> notes;
  for (int i = 0; i < 16; ++i) notes.push_back({0.25 * i, 0.25, 69, 90});
  TimingRules r;
  r.tempoScale = 1.5;
  r.phraseScoreSeconds = 4.0;
  r.phraseArchDepth = 0.15;
  r.detachedRatio = 0.85;
  const auto p = applyExpressiveTiming(notes, r);
  REQUIRE(p.size() == 16);
  const double total = p.back().onsetSeconds + p.back().ioiSeconds;
  CHECK(std::fabs(total - 6.0) < 0.02);            // arch keeps the phrase length
  CHECK(p.front().ioiSeconds > p[8].ioiSeconds);   // ends slower than the middle
  CHECK(std::fabs(p[3].soundSeconds - 0.85 * p[3].ioiSeconds) < 1e-12);
  BowArticulation a = articulationForNote(0.08, 100);
  CHECK(a.bounce);
  CHECK(a.restDampT60 > 0.0);
}
TEST_CASE("room convolution is exact against a direct sum", "[contact]") {
  const std::vector<double> dry{1.0, -0.5, 0.25, 0.0, 2.0};
  const std::vector<double> ir{0.5, 0.0, -0.25};
  const auto out = convolveRoom(dry, ir);
  REQUIRE(out.size() == 7);
  for (std::size_t n = 0; n < out.size(); ++n) {
    double want = 0.0;
    for (std::size_t k = 0; k < ir.size(); ++k)
      if (n >= k && n - k < dry.size()) want += ir[k] * dry[n - k];
    CHECK(std::fabs(out[n] - want) < 1e-12);
  }
}
TEST_CASE("expressive dynamics and seeded variability", "[contact]") {
  std::vector<ScoreNote> notes;
  for (int i = 0; i < 16; ++i) notes.push_back({0.25 * i, 0.25, 69, i % 2 ? 101 : 81});
  TimingRules r;
  r.phraseScoreSeconds = 4.0;
  r.levelArchDb = 4.0;
  r.velocityDbPer64 = 6.0;
  r.timingJitterFraction = 0.02;
  r.intonationSdCents = 4.0;
  const auto a = applyExpressiveTiming(notes, r);
  const auto b = applyExpressiveTiming(notes, r);
  REQUIRE(a.size() == b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    CHECK(a[i].ioiSeconds == b[i].ioiSeconds);   // seeded: reproducible
    CHECK(a[i].centsOffset == b[i].centsOffset);
  }
  CHECK(a[7].level > a[0].level);  // phrase middle louder than its start
  BowArticulation bad;
  bad.level = 0.0;
  const StringCourse course = unwrap(violin()).courses[2];
  auto voice = unwrap(OversampledBowedStringVoice::create(
      course, violinBowStroke(course, kViolinPlayingBeta, 0.2), unwrap(violinStringLoss(course)),
      0.0, 4, 0, kRate, 4800, 4));
  CHECK_FALSE(voice.articulate(bad));
  const auto wet = mixRoom({1.0, 0.0, 0.0}, {0.0, 1.0}, 0.5);
  REQUIRE(wet.size() == 3);
  CHECK(std::fabs(wet[0] - 0.5) < 1e-12);
  CHECK(std::fabs(wet[1] - 0.5) < 1e-12);
}

TEST_CASE("vibrato depth stays in cents on a stopped note", "[contact]") {
  // Fails the old behaviour twice over: depth was set in open-string
  // samples (about 30 cents at 1.5x), and the vibrato ring wrapped for
  // deep vibrato (a +14 cent sharp offset).
  const StringCourse course = unwrap(violin()).courses[2];
  const double open = course.tuning.frequency.value();
  const double f = open * 1.5;
  auto measure = [&](double cents, double& meanCents, double& halfCents) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, kViolinPlayingBeta, 0.2), unwrap(violinStringLoss(course)),
        0.0, 4, 0, kRate, 192000, 4));
    if (cents > 0.0) REQUIRE(voice.setExpression(BowExpression{6.5, cents, 0.1, 0.15, 0.0, 1.0, 0.0}));
    REQUIRE(voice.setStoppedFrequency(f));
    std::vector<double> x(192000);
    REQUIRE(voice.renderBlock(x.data(), FrameCount{x.size()}).ok());
    std::vector<double> t;
    for (std::size_t i = 96001; i < x.size(); ++i)
      if (x[i - 1] < 0.0 && x[i] >= 0.0) t.push_back((i - 1) + x[i - 1] / (x[i - 1] - x[i]));
    REQUIRE(t.size() > 200);
    meanCents = 1200.0 * std::log2(kRate.value() * (t.size() - 1) / (t.back() - t.front()) / f);
    std::vector<double> p;
    for (std::size_t i = 1; i < t.size(); ++i) p.push_back(t[i] - t[i - 1]);
    double lo = 1e9, hi = -1e9;
    for (std::size_t i = 2; i + 2 < p.size(); ++i) {
      const double s5 = (p[i - 2] + p[i - 1] + p[i] + p[i + 1] + p[i + 2]) / 5.0;
      lo = std::min(lo, s5);
      hi = std::max(hi, s5);
    }
    halfCents = 0.5 * 1200.0 * std::log2(hi / lo);
  };
  double m0 = 0.0, h0 = 0.0, m1 = 0.0, h1 = 0.0;
  measure(0.0, m0, h0);
  measure(20.0, m1, h1);
  CHECK(std::fabs(m1 - m0) < 3.0);  // vibrato does not move the centre
  CHECK(h1 > 14.0);
  CHECK(h1 < 26.0);                  // ~20 cents, not the ~30 of open-string samples
}
TEST_CASE("bow position moves keep the loop and reject bad input", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto voice = unwrap(OversampledBowedStringVoice::create(
      course, violinBowStroke(course, kViolinPlayingBeta, 0.2), unwrap(violinStringLoss(course)),
      0.0, 4, 0, kRate, 48000, 4));
  const double before = voice.fundamentalHz();
  CHECK_FALSE(voice.setBowPosition(0.0, 1.0));
  CHECK_FALSE(voice.setBowPosition(0.1, -1.0));
  CHECK(voice.setBowPosition(violinBetaForLevelDb(6.0), 1.0));
  CHECK(voice.fundamentalHz() == before);
  CHECK(violinBetaForLevelDb(12.0) < violinBetaForLevelDb(-12.0));
}

TEST_CASE("bridge-force output scales with bow speed and 1/beta", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto level = [&](double beta, double speed) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, beta, speed), unwrap(violinStringLoss(course)), 0.0, 4, 0,
        kRate, 96000, 4));
    voice.setBridgeForceOutput(true);
    std::vector<double> x(96000);
    REQUIRE(voice.renderBlock(x.data(), FrameCount{x.size()}).ok());
    return rmsSpan(x, 48000, 96000);
  };
  const double a = level(0.15, 0.1), b = level(0.15, 0.4), c = level(0.08, 0.4);
  CHECK(20.0 * std::log10(b / a) > 10.0);  // ~12 dB for 4x speed (Helmholtz amplitude ~ v/beta)
  CHECK(c > b);                            // nearer the bridge: louder
}

TEST_CASE("level smoothing turns velocity steps into gradual swells", "[contact]") {
  using namespace mmathen;
  std::vector<ScoreNote> sc;
  for (int i = 0; i < 40; ++i) sc.push_back({0.125 * i, 0.125, 69, i < 20 ? 32 : 96});
  TimingRules r; r.velocityDbPer64 = 6.0;
  const auto hard = applyExpressiveTiming(sc, r);
  r.levelSmoothingSeconds = 0.5;
  const auto soft = applyExpressiveTiming(sc, r);
  REQUIRE(soft.size() == hard.size());
  double maxStepHard = 0, maxStepSoft = 0;
  for (size_t i = 1; i < soft.size(); ++i) {
    maxStepHard = std::max(maxStepHard, std::abs(20 * std::log10(hard[i].level / hard[i - 1].level)));
    maxStepSoft = std::max(maxStepSoft, std::abs(20 * std::log10(soft[i].level / soft[i - 1].level)));
    CHECK(soft[i].level >= soft[i - 1].level - 1e-12);  // monotone swell, no overshoot
  }
  CHECK(std::abs(maxStepHard - 6.0) < 1e-9);
  CHECK(maxStepSoft < 1.5);                             // 1-exp(-0.125/0.5) = 22 % of the 6 dB step
  CHECK(std::abs(20 * std::log10(soft.back().level) - 3.0) < 0.1);  // converges to target
  CHECK(std::abs(soft.front().level - hard.front().level) < 1e-12);
}

TEST_CASE("level flattening removes section steps but keeps accents", "[contact]") {
  using namespace mmathen;
  std::vector<ScoreNote> sc;  // 10 s soft section then 10 s loud section, every 4th note accented
  for (int i = 0; i < 160; ++i) sc.push_back({0.125 * i, 0.125, 69, (i < 80 ? 40 : 100) + (i % 4 == 0 ? 20 : 0)});
  TimingRules r; r.velocityDbPer64 = 6.0;
  auto meanDb = [](const std::vector<PerformedNote>& v, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += 20 * std::log10(v[i].level); return s / double(b - a); };
  const auto hard = applyExpressiveTiming(sc, r);
  r.levelFlattenSeconds = 2.0;
  const auto flat = applyExpressiveTiming(sc, r);
  const double stepHard = meanDb(hard, 100, 160) - meanDb(hard, 0, 60);
  const double stepFlat = meanDb(flat, 100, 160) - meanDb(flat, 0, 60);
  CHECK(stepHard > 5.0);
  CHECK(std::abs(stepFlat) < 1.0);                 // section step removed away from the boundary
  const double accHard = 20 * std::log10(hard[120].level / hard[121].level);
  const double accFlat = 20 * std::log10(flat[120].level / flat[121].level);
  CHECK(std::abs(accFlat - accHard) < 0.3);         // accent contrast kept
}

TEST_CASE("bow taper lowers the end of a note and rejects bad fields", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto render = [&](double taperLevel) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, 0.12, 0.3), unwrap(violinStringLoss(course)), 0.0, 4, 0,
        kRate, 96000, 4));
    voice.setBridgeForceOutput(true);
    BowArticulation a; a.noteSeconds = 1.5; a.taperSeconds = 0.5; a.taperLevel = taperLevel;
    REQUIRE(voice.articulate(a));
    std::vector<double> x(72000);
    REQUIRE(voice.renderBlock(x.data(), FrameCount{x.size()}).ok());
    return std::pair{rmsSpan(x, 24000, 48000), rmsSpan(x, 64800, 72000)};
  };
  const auto flat = render(1.0), tap = render(0.5);
  CHECK(std::abs(20 * std::log10(tap.first / flat.first)) < 0.5);  // body of the note unchanged
  CHECK(20 * std::log10(tap.second / flat.second) < -3.0);         // last 0.15 s ~ half bow speed
  auto voice = unwrap(OversampledBowedStringVoice::create(
      course, violinBowStroke(course, 0.12, 0.3), unwrap(violinStringLoss(course)), 0.0, 4, 0, kRate, 96000, 4));
  BowArticulation bad; bad.taperLevel = 0.0;
  CHECK_FALSE(voice.articulate(bad));
  bad.taperLevel = 1.0; bad.taperSeconds = -1.0;
  CHECK_FALSE(voice.articulate(bad));
}

TEST_CASE("ringing articulation lets short notes ring through the gap", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto gapLevel = [&](BowArticulation a) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, 0.12, 0.3), unwrap(violinStringLoss(course)), 0.0, 4, 0,
        kRate, 96000, 4));
    voice.setBridgeForceOutput(true);
    REQUIRE(voice.articulate(a));
    std::vector<double> x(9600);  // 0.2 s; note 0.12 s, bow lifts at 0.06 s
    REQUIRE(voice.renderBlock(x.data(), FrameCount{x.size()}).ok());
    return 20 * std::log10(rmsSpan(x, 7200, 9600) / rmsSpan(x, 1440, 2880));
  };
  const BowArticulation spic = articulationForNote(0.12, 64), ring = ringingArticulationForNote(0.12, 64);
  CHECK(spic.bounce); CHECK_FALSE(ring.bounce); CHECK(ring.restDampT60 == 0.0);
  CHECK(ring.contactSeconds == spic.contactSeconds);
  CHECK(gapLevel(ring) > gapLevel(spic) + 6.0);  // the gap stays filled by the free string
}

TEST_CASE("level floor caps quiet-side attenuation with a soft knee", "[contact]") {
  using namespace mmathen;
  std::vector<ScoreNote> sc;
  for (int i = 0; i < 128; ++i) sc.push_back({0.125 * i, 0.125, 69, i});
  TimingRules r; r.velocityDbPer64 = 12.0;
  const auto raw = applyExpressiveTiming(sc, r);
  r.levelFloorDb = 4.0;
  const auto cap = applyExpressiveTiming(sc, r);
  double prev = -1e9;
  for (size_t i = 0; i < cap.size(); ++i) {
    const double d = 20 * std::log10(cap[i].level);
    CHECK(d >= -4.0 - 1e-9);
    CHECK(d >= prev - 1e-9);  // still monotone in velocity
    prev = d;
    if (20 * std::log10(raw[i].level) > -2.0) CHECK(std::abs(cap[i].level - raw[i].level) < 1e-12);
  }
}

TEST_CASE("bow beta follows the stopped length: level flat up the A string", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto level = [&](double hz, bool keep) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, 0.12, 0.2), unwrap(violinStringLoss(course)), 0.0, 4, 0,
        kRate, 96000, 4));
    voice.setBridgeForceOutput(true);
    voice.setKeepBetaOnStop(keep);
    REQUIRE(voice.setStoppedFrequency(hz));
    std::vector<double> x(72000);
    REQUIRE(voice.renderBlock(x.data(), FrameCount{x.size()}).ok());
    return 20 * std::log10(rmsSpan(x, 36000, 72000));
  };
  const double open = level(440.0, true), octave = level(880.0, true);
  CHECK(std::abs(octave - open) < 1.5);                        // flat-ish up an octave
  CHECK(level(880.0, false) < level(880.0, true) - 3.0);       // fixed contact sample drops (~6 dB)
}

TEST_CASE("finger changes settle: 0.12 s notes in an up/down A-string run", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto voice = unwrap(OversampledBowedStringVoice::create(
      course, violinBowStroke(course, 0.12, 0.3), unwrap(violinStringLoss(course)), 0.0, 4, 0,
      kRate, 960000, 4));
  voice.setBridgeForceOutput(true);
  REQUIRE(voice.setExpression(BowExpression{kViolinVibratoRateHz, kViolinVibratoCents, kViolinVibratoOnsetSeconds, kViolinVibratoRampSeconds, 0.0, 1.0, 0.0}));
  const int run[8] = {69, 71, 73, 74, 76, 74, 73, 71};
  const std::size_t note = 5760;  // 0.12 s at 48 kHz
  std::vector<double> x(note * 40);
  double worst = 0.0;
  for (int i = 0; i < 40; ++i) {
    const double f = 440.0 * std::pow(2.0, (run[i % 8] - 69) / 12.0);
    REQUIRE(voice.setStoppedFrequency(f));
    REQUIRE(voice.renderBlock(x.data() + note * i, FrameCount{note}).ok());
    if (i < 8) continue;  // let the Helmholtz motion establish
    // pitch over the middle 50 %: autocorrelation peak near the expected period, parabolic refinement
    const double* s = x.data() + note * i + note / 4;
    const std::size_t w = note / 2;
    const double T = 48000.0 / f;
    auto ac = [&](int lag) { double a = 0; for (std::size_t k = 0; k + lag < w; ++k) a += s[k] * s[k + lag]; return a; };
    int best = static_cast<int>(T * 0.9); double bv = -1e300;
    for (int lag = static_cast<int>(T * 0.9); lag <= static_cast<int>(T * 1.1) + 1; ++lag) { const double v = ac(lag); if (v > bv) { bv = v; best = lag; } }
    const double y0 = ac(best - 1), y1 = bv, y2 = ac(best + 1);
    const double den = y0 - 2 * y1 + y2;
    const double lag = best + (std::abs(den) > 0 ? 0.5 * (y0 - y2) / den : 0.0);
    worst = std::max(worst, std::abs(1200.0 * std::log2(T / lag)));
  }
  CHECK(worst < 10.0);
}

#include <mmathen/instruments/violin_body.h>
TEST_CASE("violin bridge hill: peak near rocking frequency, -12 dB/oct above", "[contact]") {
  auto mag = [](double f) {
    BridgeHillFilter h(48000.0);
    double re = 0, im = 0;
    const int n = 48000;
    std::vector<double> y(n);
    for (int i = 0; i < n; ++i) y[i] = h.process(std::sin(2 * 3.141592653589793 * f * i / 48000.0));
    for (int i = n / 2; i < n; ++i) { re += y[i] * std::sin(2 * 3.141592653589793 * f * i / 48000.0); im += y[i] * std::cos(2 * 3.141592653589793 * f * i / 48000.0); }
    return 20 * std::log10(2.0 * std::hypot(re, im) / (n / 2));
  };
  CHECK(std::abs(mag(100.0)) < 0.1);
  CHECK(std::abs(kViolinBridgeHillQ - 1.28) < 0.02);  // Woodhouse 2005 eq. 19
  CHECK(std::abs(mag(2900.0) - 20 * std::log10(kViolinBridgeHillQ)) < 0.5);
  const double d = mag(8000.0) - mag(16000.0);
  CHECK(d > 9.0);                               // ~12 dB/octave rolloff
  // Rayleigh A0 for a 2 l cavity, 2 x 6 cm^2 holes, 2.5 cm effective length -> a few hundred Hz
  const double a0 = helmholtzFrequencyHz(2.0e-3, 12e-4, 0.025);
  CHECK(a0 > 200.0); CHECK(a0 < 350.0);  // measured A0 253-282 Hz (Bissinger 2008)
}

TEST_CASE("signature modes boost 400-550 Hz, not low or high", "[contact]") {
  auto gainAt = [](double f) {
    SignatureModeFilter h(48000.0);
    const int n = 96000; double re = 0, im = 0;
    for (int i = 0; i < n; ++i) { const double ph = 2 * 3.141592653589793 * f * i / 48000.0; const double y = h.process(std::sin(ph)); if (i >= n / 2) { re += y * std::sin(ph); im += y * std::cos(ph); } }
    return 20 * std::log10(2.0 * std::hypot(re, im) / (n / 2));
  };
  CHECK(gainAt(475.0) > 6.0);
  CHECK(std::abs(gainAt(60.0)) < 1.0);
  CHECK(std::abs(gainAt(4000.0)) < 1.0);
}

TEST_CASE("radiation high-pass falls below A0 and passes above", "[contact]") {
  auto g = [](double f) { RadiationHighpass h(48000.0); double e = 0, ei = 0; for (int i = 0; i < 96000; ++i) { const double x = std::sin(2 * 3.141592653589793 * f * i / 48000.0); const double y = h.process(x); if (i >= 48000) { e += y * y; ei += x * x; } } return 10 * std::log10(e / ei); };
  CHECK(g(100.0) < -15.0);
  CHECK(std::abs(g(2000.0)) < 0.5);
}

TEST_CASE("bow force jitter adds noise, keeps the Helmholtz level", "[contact]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto render = [&](double j) {
    auto voice = unwrap(OversampledBowedStringVoice::create(
        course, violinBowStroke(course, 0.12, 0.2), unwrap(violinStringLoss(course)), 0.0, 4, 0,
        kRate, 96000, 4));
    voice.setBridgeForceOutput(true);
    voice.setForceJitter(j);
    std::vector<double> x(72000);
    REQUIRE(voice.renderBlock(x.data(), FrameCount{x.size()}).ok());
    return x;
  };
  const auto a = render(0.0), b = render(0.3), c = render(0.0);
  double diff = 0, same = 0;
  for (size_t i = 36000; i < 72000; ++i) { diff += std::abs(a[i] - b[i]); same += std::abs(a[i] - c[i]); }
  CHECK(same == 0.0);
  CHECK(diff > 0.0);
  CHECK(std::abs(20 * std::log10(rmsSpan(b, 36000, 72000) / rmsSpan(a, 36000, 72000))) < 1.5);
}