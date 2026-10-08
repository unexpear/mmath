// A plucked course closed on a modal soundboard, radiated as a monopole.
//
// Mass, Q, resonance, radiating area, and distance below are this
// test's arguments. They are not stored on any instrument. The heavy
// board is nearly rigid, so the radiated tone is the string. A lighter
// board at the same Q decays faster, and a detuned board pulls the
// pitch: both are the bridge moving the string.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/tuned_waveguide.h>
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

LossModel courseLoss(const StringCourse& course) {
  const auto ideal = unwrap(courseString(course));
  const double frequency = course.tuning.frequency.value();
  return unwrap(lossFromT60(ideal.speed, FrequencyHz{frequency}, DurationSeconds{2.0},
                            FrequencyHz{6.0 * frequency}, DurationSeconds{0.25}));
}

SoundboardVoice makeBoard(const StringCourse& course, std::span<const BodyModeSpec> modes) {
  const Excitation pluck{ExcitationKind::Pluck, 0.3, 1.0e-4};
  return unwrap(SoundboardVoice::create(course, pluck, courseLoss(course), modes, 0.02,
                                        1.0, 101325.0, 293.15, kRate));
}

std::vector<double> renderOf(SoundboardVoice& voice, std::uint64_t frames) {
  std::vector<double> samples(static_cast<std::size_t>(frames));
  REQUIRE(voice.renderBlock(samples.data(), FrameCount{frames}).ok());
  return samples;
}

double pitchNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                 double highHz) {
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() +
                                 static_cast<std::ptrdiff_t>(begin + 8192));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris,
                                 FrequencyHz{lowHz}, FrequencyHz{highHz}))
      .frequency.value();
}

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

double amplitudeNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                     double highHz) {
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

void requireFinite(const std::vector<double>& samples) {
  for (double sample : samples) REQUIRE(std::isfinite(sample));
}

StringCourse harpCourse() {
  const PedalHarp harp = unwrap(concertPedalHarp());
  const HarpStringGeometry* cb4 = findHarpString(harp, "Cb4");
  REQUIRE(cb4 != nullptr);
  const double frequency = unwrap(makePitch("Cb4")).frequency.value();
  const double length = cb4->vibratingLengthCm * 1.0e-2;
  const double tension = 2.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return unwrap(tuneHarpString(*cb4, 1.0, 3.0, tension * 0.5, tension * 1.5));
}

StringCourse harpsichordCourse() {
  const FrenchDouble instrument = unwrap(frenchDouble());
  const HarpsichordSpeakingLength* eight =
      findSpeakingLength(instrument, HarpsichordChoir::LongEightFoot, "C4");
  REQUIRE(eight != nullptr);
  const double frequency = unwrap(makePitch("C4")).frequency.value();
  const double length = eight->vibratingLengthMm * 1.0e-3;
  const double tension =
      1.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return unwrap(tuneHarpsichordString(*eight, 0.5, 1.5, tension * 0.5, tension * 1.5));
}

StringCourse classicalCourse() {
  const ClassicalGuitar guitar = unwrap(classicalGuitar());
  const ClassicalGuitarString* high = findClassicalGuitarString(guitar, "E4");
  REQUIRE(high != nullptr);
  const double length = guitar.vibratingLength.value();
  const double frequency = unwrap(makePitch("E4")).frequency.value();
  const double tension =
      2.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return unwrap(tuneClassicalGuitarString(guitar, *high, 1.0, 3.0, tension * 0.5,
                                          tension * 1.5));
}

}  // namespace

TEST_CASE("a zero bridge displacement is the rigid reflection", "[soundboard]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const Excitation pluck{ExcitationKind::Pluck, 0.3, 1.0e-4};
  const auto system = unwrap(pluckedCourseSystem(course, courseLoss(course), 0.0));
  const auto shape = unwrap(pluckedCourseShape(system, pluck));
  const auto design = unwrap(designTunedLoop(system, kRate, 4, 0));
  auto rigid = unwrap(TunedWaveguide::create(shape, design.filters));
  auto yielding = unwrap(TunedWaveguide::create(shape, design.filters));
  const std::size_t node = design.filters.intervals / 3;
  for (int i = 0; i < 400; ++i) {
    rigid.step();
    const double moved = yielding.stepYieldingBridge(0.0);
    CHECK(std::isfinite(moved));
    CHECK(unwrap(rigid.valueAt(node)) == unwrap(yielding.valueAt(node)));
  }
}

TEST_CASE("a heavy soundboard radiates the string", "[soundboard]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double frequency = course.tuning.frequency.value();
  const BodyModeSpec mode{frequency, 200.0, 20.0};
  auto voice = makeBoard(course, std::span<const BodyModeSpec>(&mode, 1));
  CHECK(nearlyEqual(voice.fundamentalHz(), frequency, 0.01, 0.0));
  const auto samples = renderOf(voice, 32768);
  requireFinite(samples);
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  CHECK(peak > 1.0e-4);
  const double heard = pitchNear(samples, 16384, 400.0, 480.0);
  CHECK(std::fabs(heard - voice.fundamentalHz()) < 1.0);
  const double early = amplitudeNear(samples, 16384, 400.0, 480.0);
  const double late = amplitudeNear(samples, 24576, 400.0, 480.0);
  REQUIRE(early > 0.0);
  const double expected = std::exp(-voice.stringDecay() * (8192.0 / kFs));
  CHECK(std::fabs(late / early - expected) / expected < 0.05);
  writeListening(samples, "mmath-violin-soundboard.wav");

  voice.reset();
  std::vector<double> once(1024);
  std::vector<double> blocked(1024);
  REQUIRE(voice.renderBlock(once.data(), FrameCount{1024}).ok());
  voice.reset();
  for (std::size_t i = 0; i < blocked.size(); i += 128) {
    REQUIRE(voice.renderBlock(blocked.data() + i, FrameCount{128}).ok());
  }
  for (std::size_t i = 0; i < once.size(); ++i) CHECK(once[i] == blocked[i]);
}

TEST_CASE("a yielding bridge changes the radiated tone", "[soundboard]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double frequency = course.tuning.frequency.value();
  const BodyModeSpec light{frequency, 40.0, 0.5};
  const BodyModeSpec heavy{frequency, 40.0, 20.0};
  auto lightVoice = makeBoard(course, std::span<const BodyModeSpec>(&light, 1));
  auto heavyVoice = makeBoard(course, std::span<const BodyModeSpec>(&heavy, 1));
  const auto lightSamples = renderOf(lightVoice, 32768);
  const auto heavySamples = renderOf(heavyVoice, 32768);
  requireFinite(lightSamples);
  requireFinite(heavySamples);
  const double lightEarly = amplitudeNear(lightSamples, 8192, 400.0, 480.0);
  const double lightLate = amplitudeNear(lightSamples, 16384, 400.0, 480.0);
  const double heavyEarly = amplitudeNear(heavySamples, 8192, 400.0, 480.0);
  const double heavyLate = amplitudeNear(heavySamples, 16384, 400.0, 480.0);
  REQUIRE(lightEarly > 0.0);
  REQUIRE(heavyEarly > 0.0);
  CHECK(lightLate / lightEarly < 0.40);
  CHECK(heavyLate / heavyEarly > 0.45);
  double lightPeak = 0.0;
  double heavyPeak = 0.0;
  for (double sample : lightSamples) lightPeak = std::max(lightPeak, std::fabs(sample));
  for (double sample : heavySamples) heavyPeak = std::max(heavyPeak, std::fabs(sample));
  CHECK(lightPeak > 5.0 * heavyPeak);

  const BodyModeSpec detuned{300.0, 80.0, 0.2};
  auto pulled = makeBoard(course, std::span<const BodyModeSpec>(&detuned, 1));
  const auto pulledSamples = renderOf(pulled, 32768);
  requireFinite(pulledSamples);
  const double pulledPitch = pitchNear(pulledSamples, 8192, 400.0, 480.0);
  const double heavyPitch = pitchNear(heavySamples, 8192, 400.0, 480.0);
  CHECK(pulledPitch > heavyPitch + 0.05);
  CHECK(std::fabs(pitchNear(pulledSamples, 16384, 400.0, 480.0) - pulledPitch) < 0.02);
  writeListening(lightSamples, "mmath-violin-soundboard-light.wav");
}

TEST_CASE("a mode bank still radiates the plucked fundamental", "[soundboard]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const double frequency = course.tuning.frequency.value();
  const BodyModeSpec bank[] = {{frequency, 200.0, 20.0}, {2.7 * frequency, 60.0, 8.0}};
  auto voice = makeBoard(course, bank);
  const auto samples = renderOf(voice, 32768);
  requireFinite(samples);
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  CHECK(peak > 1.0e-4);
  CHECK(std::fabs(pitchNear(samples, 16384, 400.0, 480.0) - voice.fundamentalHz()) < 1.0);
  const BodyModeSpec none[] = {{frequency, 200.0, 20.0}};
  CHECK(SoundboardVoice::create(course, Excitation{ExcitationKind::Pluck, 0.3, 1.0e-4},
                                courseLoss(course), std::span<const BodyModeSpec>{}, 0.02, 1.0,
                                101325.0, 293.15, kRate)
            .error() == QuantityError::InvalidArgument);
  CHECK(SoundboardVoice::create(course, Excitation{ExcitationKind::Pluck, 0.3, 1.0e-4},
                                courseLoss(course), none, -1.0, 1.0, 101325.0, 293.15, kRate)
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("harp, harpsichord, and classical guitar radiate through a soundboard",
          "[soundboard]") {
  struct Row {
    const char* name;
    StringCourse course;
    const char* path;
  };
  const Row rows[] = {
      {"harp", harpCourse(), "mmath-harp-soundboard.wav"},
      {"harpsichord", harpsichordCourse(),
       "mmath-harpsichord-soundboard.wav"},
      {"classical guitar", classicalCourse(),
       "mmath-classical-guitar-soundboard.wav"},
  };
  for (const Row& row : rows) {
    INFO(row.name);
    const double frequency = row.course.tuning.frequency.value();
    const BodyModeSpec mode{frequency, 200.0, 20.0};
    auto voice = makeBoard(row.course, std::span<const BodyModeSpec>(&mode, 1));
    const auto samples = renderOf(voice, 32768);
    requireFinite(samples);
    double peak = 0.0;
    for (double sample : samples) peak = std::max(peak, std::fabs(sample));
    CHECK(peak > 1.0e-5);
    const double low = frequency - 40.0;
    const double high = frequency + 40.0;
    CHECK(std::fabs(pitchNear(samples, 16384, low, high) - voice.fundamentalHz()) < 1.5);
    CHECK(energyAboveGuard(samples, 16384) < 0.01);
    CHECK(autocorrelationAt(samples, 16384, voice.fundamentalHz()) > 0.8);
    const double early = amplitudeNear(samples, 16384, low, high);
    const double late = amplitudeNear(samples, 24576, low, high);
    REQUIRE(early > late);
    REQUIRE(early > 0.0);
    writeListening(samples, row.path);
  }
  REQUIRE(kInstrumentRenderStatusCount == 26);
  CHECK(instrumentsThatRenderAudio() == 16);
  for (const char* name : {"harp", "harpsichord", "classical guitar"}) {
    bool found = false;
    for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
      if (std::strcmp(kInstrumentRenderStatus[i].name, name) == 0) {
        found = true;
        CHECK(kInstrumentRenderStatus[i].rendersAudio);
        CHECK(std::strstr(kInstrumentRenderStatus[i].body, "feedback") != nullptr);
        CHECK(std::strstr(kInstrumentRenderStatus[i].radiation, "monopole") != nullptr);
      }
    }
    CHECK(found);
  }
  for (const char* name : {"electric guitar"}) {
    for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
      if (std::strcmp(kInstrumentRenderStatus[i].name, name) == 0)
        CHECK_FALSE(kInstrumentRenderStatus[i].rendersAudio);
    }
  }
}
