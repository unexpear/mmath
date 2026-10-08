// Plucked courses step a lossy tuned waveguide into a WAV.
//
// Loss durations and the stiffness coefficient below are this test's
// arguments. They are not stored on any instrument. The electric-guitar
// listen fraction is likewise an argument: the chart prints no pickup
// position.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/cello.h>
#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/electric_guitar.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/wave1d.h>
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
constexpr double kPluckFraction = 0.5;
constexpr double kListenFraction = 0.23;
constexpr double kHeight = 1.0e-4;

const InstrumentRenderStatus* statusNamed(const char* name) {
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (std::strcmp(kInstrumentRenderStatus[i].name, name) == 0)
      return &kInstrumentRenderStatus[i];
  }
  return nullptr;
}

LossModel testLoss(const StringCourse& course) {
  const auto ideal = unwrap(courseString(course));
  const double f = course.tuning.frequency.value();
  return unwrap(lossFromT60(ideal.speed, FrequencyHz{f}, DurationSeconds{2.0},
                            FrequencyHz{6.0 * f}, DurationSeconds{0.25}));
}

PluckedStringVoice makePluck(const StringCourse& course, StringReadout readout,
                             double listenFraction, double pluckFraction) {
  const Excitation pluck{ExcitationKind::Pluck, pluckFraction, kHeight};
  return unwrap(PluckedStringVoice::create(
      course, pluck, testLoss(course), 0.0, 4, 0, kRate, listenFraction,
      readout));
}

double zeroCrossingHz(const std::vector<double>& x, double fs) {
  std::vector<double> marks;
  for (std::size_t i = 1; i < x.size(); ++i) {
    if (x[i - 1] <= 0.0 && x[i] > 0.0)
      marks.push_back(static_cast<double>(i));
  }
  REQUIRE(marks.size() >= 4);
  double period = 0.0;
  for (std::size_t i = 1; i < marks.size(); ++i)
    period += marks[i] - marks[i - 1];
  period /= static_cast<double>(marks.size() - 1);
  return fs / period;
}

void checkAudible(PluckedStringVoice& voice, const char* path) {
  const double pole = voice.fundamentalHz();
  const double decay = voice.fundamentalDecay();
  REQUIRE(pole > 0.0);
  REQUIRE(decay > 0.0);
  constexpr std::uint64_t kFrames = 24576;
  MemorySink memory;
  const auto stats = unwrap(renderVoice(
      voice, VoiceRenderConfig{FrameCount{kFrames}, 512}, memory));
  CHECK(stats.framesRendered == kFrames);
  CHECK(stats.isClean());
  CHECK(stats.peakAbs > 1.0e-5);
  const std::vector<double>& samples = memory.samples();
  REQUIRE(samples.size() == kFrames);
  for (double v : samples) CHECK(std::isfinite(v));

  std::vector<double> early(samples.begin(), samples.begin() + 8192);
  const auto est = unwrap(estimateSinusoid(
      early, kRate, Window::BlackmanHarris, FrequencyHz{pole - 40.0},
      FrequencyHz{pole + 40.0}));
  CHECK(std::fabs(est.frequency.value() - pole) < 0.5);

  std::vector<double> late(samples.begin() + 16384, samples.begin() + 24576);
  CHECK(std::fabs(zeroCrossingHz(late, kFs) - pole) / pole < 0.02);

  auto amplitude = [&](std::size_t start) {
    std::vector<double> w(samples.begin() + static_cast<std::ptrdiff_t>(start),
                          samples.begin() +
                              static_cast<std::ptrdiff_t>(start + 4096));
    return unwrap(estimateSinusoid(
                      w, kRate, Window::BlackmanHarris,
                      FrequencyHz{pole - 40.0}, FrequencyHz{pole + 40.0}))
        .amplitude;
  };
  const double a0 = amplitude(0);
  const double a1 = amplitude(8192);
  CHECK(a1 < a0);
  const double measured = std::log(a0 / a1) / (8192.0 / kFs);
  CHECK(nearlyEqual(measured, decay, 0.0, 3.0e-2));

  voice.reset();
  auto wav = unwrap(WavSink::open(path, kRate, FrameCount{kFrames}));
  const auto filed = unwrap(renderVoice(
      voice, VoiceRenderConfig{FrameCount{kFrames}, 128}, wav));
  CHECK(filed.isClean());
  CHECK(filed.peakAbs == stats.peakAbs);
  CHECK(unwrap(wav.close()).value() == kFrames);

  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  in.seekg(44);
  std::int16_t pcm = 0;
  int nonzero = 0;
  for (std::uint64_t i = 0; i < kFrames; ++i) {
    in.read(reinterpret_cast<char*>(&pcm), 2);
    REQUIRE(in.good());
    if (pcm != 0) ++nonzero;
  }
  CHECK(nonzero > 100);
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
  const HarpsichordSpeakingLength* eight = findSpeakingLength(
      instrument, HarpsichordChoir::LongEightFoot, "C4");
  REQUIRE(eight != nullptr);
  REQUIRE(eight->pluckingPointMm.has_value());
  const double frequency = unwrap(makePitch("C4")).frequency.value();
  const double length = eight->vibratingLengthMm * 1.0e-3;
  const double tension =
      1.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return unwrap(tuneHarpsichordString(*eight, 0.5, 1.5, tension * 0.5,
                                      tension * 1.5));
}

StringCourse classicalCourse() {
  const ClassicalGuitar guitar = unwrap(classicalGuitar());
  const ClassicalGuitarString* high = findClassicalGuitarString(guitar, "E4");
  REQUIRE(high != nullptr);
  const double length = guitar.vibratingLength.value();
  const double frequency = unwrap(makePitch("E4")).frequency.value();
  const double tension =
      2.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return unwrap(tuneClassicalGuitarString(guitar, *high, 1.0, 3.0,
                                          tension * 0.5, tension * 1.5));
}

StringCourse electricCourse() {
  const ElectricGuitar guitar = unwrap(electricGuitar());
  const ElectricGuitarString* high = findElectricGuitarString(guitar, "E4");
  REQUIRE(high != nullptr);
  const double length = guitar.vibratingLength.value();
  const double frequency = unwrap(makePitch("E4")).frequency.value();
  const double newtons = high->tensionPounds * kPoundForceNewtons;
  const double grams =
      newtons / ((2.0 * length * frequency) * (2.0 * length * frequency)) *
      1.0e3;
  return unwrap(tuneElectricGuitarString(guitar, *high, grams));
}
}  // namespace

TEST_CASE("plucked courses still leave the audible bar unmet", "[pluck]") {
  REQUIRE(kInstrumentRenderStatusCount == 26);
  CHECK(instrumentsThatRenderAudio() == 0);
  const char* stepped[] = {"violin", "viola", "cello", "double bass",
                           "harp", "harpsichord", "classical guitar",
                           "electric guitar"};
  for (const char* name : stepped) {
    const auto* row = statusNamed(name);
    REQUIRE(row != nullptr);
    CHECK_FALSE(row->rendersAudio);
    CHECK(std::strstr(row->resonator, "waveguide") != nullptr);
  }
  const auto* electric = statusNamed("electric guitar");
  REQUIRE(electric != nullptr);
  CHECK(std::strstr(electric->radiation, "coil") != nullptr);
  const auto* piano = statusNamed("piano");
  REQUIRE(piano != nullptr);
  CHECK_FALSE(piano->rendersAudio);
  CHECK(std::strstr(piano->gap, "hammer") != nullptr);
  CHECK(std::strstr(statusNamed("clarinet")->gap, "bore") != nullptr);
}

TEST_CASE("plucked instrument courses render pitch and decay", "[pluck]") {
  const StringCourse violinA = unwrap(violin()).courses[2];
  const StringCourse violaD = unwrap(viola()).courses[2];
  const StringCourse celloA = unwrap(cello()).courses[3];
  const StringCourse bassG = unwrap(doubleBass()).courses[3];
  REQUIRE(std::strcmp(violinA.tuning.label, "A4") == 0);
  REQUIRE(std::strcmp(violaD.tuning.label, "D4") == 0);
  REQUIRE(std::strcmp(celloA.tuning.label, "A3") == 0);
  REQUIRE(std::strcmp(bassG.tuning.label, "G2") == 0);

  struct Row {
    const char* path;
    StringCourse course;
    StringReadout readout;
  };
  const Row rows[] = {
      {"/tmp/mmath-pluck-violin.wav", violinA, StringReadout::Displacement},
      {"/tmp/mmath-pluck-viola.wav", violaD, StringReadout::Displacement},
      {"/tmp/mmath-pluck-cello.wav", celloA, StringReadout::Displacement},
      {"/tmp/mmath-pluck-double-bass.wav", bassG, StringReadout::Displacement},
      {"/tmp/mmath-pluck-harp.wav", harpCourse(), StringReadout::Displacement},
      {"/tmp/mmath-pluck-harpsichord.wav", harpsichordCourse(),
       StringReadout::Displacement},
      {"/tmp/mmath-pluck-classical-guitar.wav", classicalCourse(),
       StringReadout::Displacement},
      {"/tmp/mmath-pluck-electric-guitar.wav", electricCourse(),
       StringReadout::CoilVelocity},
  };
  for (const Row& row : rows) {
    INFO(row.path);
    auto voice = makePluck(row.course, row.readout, kListenFraction,
                           kPluckFraction);
    // The loop is tuned to the damped mode, sqrt(w^2 - sigma^2), which
    // is the pitch the samples ring at. The course label is the
    // undamped ideal-string frequency.
    const auto model = unwrap(modeFrequency(
        unwrap(pluckedCourseSystem(row.course, testLoss(row.course), 0.0)),
        1));
    CHECK(std::fabs(voice.fundamentalHz() - model.value()) < 1.0e-4);
    CHECK(std::fabs(voice.fundamentalHz() -
                    row.course.tuning.frequency.value()) < 0.01);
    checkAudible(voice, row.path);
  }
}

TEST_CASE("electric pickup is a position comb and a coil velocity",
          "[pluck]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const Excitation pluck{ExcitationKind::Pluck, 0.13, kHeight};
  const LossModel loss = testLoss(course);
  auto at = [&](double listen, StringReadout readout) {
    return unwrap(PluckedStringVoice::create(course, pluck, loss, 0.0, 4, 0,
                                             kRate, listen, readout));
  };
  auto displacementNode = at(0.5, StringReadout::Displacement);
  auto displacementOff = at(0.25, StringReadout::Displacement);
  auto coilNode = at(0.5, StringReadout::CoilVelocity);
  auto coilOff = at(0.25, StringReadout::CoilVelocity);

  constexpr std::uint64_t kFrames = 8192;
  auto render = [&](PluckedStringVoice& voice) {
    MemorySink memory;
    CHECK(renderVoice(voice, VoiceRenderConfig{FrameCount{kFrames}, 256},
                      memory)
              .ok());
    return memory.samples();
  };
  const auto node = render(displacementNode);
  const auto off = render(displacementOff);
  const auto coil = render(coilNode);
  const auto coilOpen = render(coilOff);
  const double second =
      unwrap(loopPole(displacementNode.filters(), kRate, 2)).frequency.value();
  auto partial2 = [&](const std::vector<double>& samples) {
    return unwrap(estimateSinusoid(
                      samples, kRate, Window::BlackmanHarris,
                      FrequencyHz{second - 50.0}, FrequencyHz{second + 50.0}))
        .amplitude;
  };
  CHECK(partial2(node) < 0.25 * partial2(off));
  CHECK(partial2(coil) < 0.25 * partial2(coilOpen));

  displacementNode.reset();
  coilNode.reset();
  const auto again = render(displacementNode);
  const auto coilAgain = render(coilNode);
  REQUIRE(coilAgain.size() == again.size());
  CHECK(coilAgain.front() == 0.0);
  for (std::size_t i = 1; i < again.size(); ++i) {
    CHECK(coilAgain[i] == (again[i] - again[i - 1]) * kFs);
  }
}

TEST_CASE("a stiff plucked course tracks the stretched partial", "[pluck]") {
  const StringCourse course = unwrap(violin()).courses[2];
  auto system = unwrap(pluckedCourseSystem(course, testLoss(course), 0.0));
  const double kappa = unwrap(stiffnessFromB(system, 4.0e-4));
  const Excitation pluck{ExcitationKind::Pluck, 0.17, kHeight};
  auto voice = unwrap(PluckedStringVoice::create(
      course, pluck, testLoss(course), kappa, 4, 2, kRate, 0.29,
      StringReadout::Displacement));
  const double fundamental = course.tuning.frequency.value();
  CHECK(voice.fundamentalHz() > fundamental);
  const auto fourth = unwrap(loopPole(voice.filters(), kRate, 4));
  const double harmonic = 4.0 * fundamental;
  CHECK(fourth.frequency.value() > harmonic + 3.0);

  constexpr std::uint64_t kFrames = 16384;
  MemorySink memory;
  const auto stats = unwrap(renderVoice(
      voice, VoiceRenderConfig{FrameCount{kFrames}, 512}, memory));
  CHECK(stats.isClean());
  CHECK(stats.peakAbs > 1.0e-5);
  std::vector<double> window(memory.samples().begin(),
                             memory.samples().begin() + 8192);
  const auto est = unwrap(estimateSinusoid(
      window, kRate, Window::BlackmanHarris,
      FrequencyHz{fourth.frequency.value() - 30.0},
      FrequencyHz{fourth.frequency.value() + 30.0}));
  CHECK(std::fabs(est.frequency.value() - fourth.frequency.value()) < 2.0);
  CHECK(est.frequency.value() > harmonic + 2.0);
}

TEST_CASE("the same pluck steps on the FDTD grid", "[pluck]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const Excitation pluck{ExcitationKind::Pluck, kPluckFraction, kHeight};
  auto grid = unwrap(
      pluckedCourseFdtd(course, pluck, testLoss(course), 0.0, 128, 1.0));
  const double dt = grid.timeStep().value();
  REQUIRE(dt > 0.0);
  const SampleRateHz rate{1.0 / dt};
  constexpr std::size_t kFrames = 8192;
  const std::size_t node = 30;
  std::vector<double> samples(kFrames);
  double peak = 0.0;
  for (std::size_t i = 0; i < kFrames; ++i) {
    samples[i] = unwrap(grid.valueAt(node));
    CHECK(std::isfinite(samples[i]));
    peak = (std::max)(peak, std::fabs(samples[i]));
    grid.step();
  }
  CHECK(peak > 1.0e-6);
  const double f0 = course.tuning.frequency.value();
  const auto est = unwrap(estimateSinusoid(
      samples, rate, Window::BlackmanHarris, FrequencyHz{f0 * 0.8},
      FrequencyHz{f0 * 1.2}));
  CHECK(std::fabs(est.frequency.value() - f0) / f0 < 0.02);
}

TEST_CASE("a plucked voice rejects a bow and a bad listen point", "[pluck]") {
  const StringCourse course = unwrap(violin()).courses[2];
  const LossModel loss = testLoss(course);
  const Excitation bow{ExcitationKind::Bow, 0.2, kHeight};
  CHECK(PluckedStringVoice::create(course, bow, loss, 0.0, 4, 0, kRate, 0.2,
                                   StringReadout::Displacement)
            .error() == QuantityError::DomainError);
  const Excitation pluck{ExcitationKind::Pluck, 0.2, kHeight};
  CHECK(PluckedStringVoice::create(course, pluck, loss, 0.0, 4, 0, kRate, 0.0,
                                   StringReadout::Displacement)
            .error() == QuantityError::InvalidArgument);
  CHECK(PluckedStringVoice::create(course, pluck, loss, 0.0, 4, 0, kRate, 1.0,
                                   StringReadout::Displacement)
            .error() == QuantityError::InvalidArgument);
  auto system = unwrap(pluckedCourseSystem(course, loss, 0.0));
  const double kappa = unwrap(stiffnessFromB(system, 1.0e-4));
  CHECK(PluckedStringVoice::create(course, pluck, loss, kappa, 4, 0, kRate, 0.2,
                                   StringReadout::Displacement)
            .error() == QuantityError::InvalidArgument);
  CHECK(PluckedStringVoice::create(course, pluck, loss, 0.0, 4, 1, kRate, 0.2,
                                   StringReadout::Displacement)
            .error() == QuantityError::InvalidArgument);

  auto voice = makePluck(course, StringReadout::Displacement, 0.2, 0.2);
  CHECK(voice.renderBlock(nullptr, FrameCount{4}).error() ==
        QuantityError::InvalidArgument);
  MemorySink one;
  MemorySink other;
  auto second = makePluck(course, StringReadout::Displacement, 0.2, 0.2);
  CHECK(renderVoice(voice, VoiceRenderConfig{FrameCount{300}, 64}, one).ok());
  CHECK(renderVoice(second, VoiceRenderConfig{FrameCount{300}, 127}, other).ok());
  REQUIRE(one.samples().size() == other.samples().size());
  for (std::size_t i = 0; i < one.samples().size(); ++i)
    CHECK(one.samples()[i] == other.samples()[i]);
}
