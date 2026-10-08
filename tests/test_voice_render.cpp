// Voice render path: a stateful stepper fills blocks, WavSink stores them.
//
// The waveguide voice is the stepper already in tuned_waveguide.h. Its
// decay is the loop's loss filter, not an envelope. Plucked courses have
// their own voice. Clarinet, flute, and saxophone are the three that
// render audio.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/math/comparison.h>
#include <mmathen/physics/tuned_waveguide.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/render/voice.h>
#include <mmathen/render/waveguide_voice.h>
#include <mmathen/sink/sink.h>
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

WaveSystem1D lossyString() {
  auto base = unwrap(idealString(DistanceMeters{0.6523}, TensionNewtons{97.344},
                                 LinearDensityKgPerM{0.001}));
  auto loss = unwrap(lossFromT60(base.speed, FrequencyHz{240.0},
                                 DurationSeconds{3.0}, FrequencyHz{2400.0},
                                 DurationSeconds{0.4}));
  return unwrap(withLoss(base, loss));
}

WaveguideVoice makeVoice() {
  const auto system = lossyString();
  const auto design = unwrap(designTunedLoop(system, kRate, 6, 0));
  const auto shape = unwrap(InitialShape::mode(system, 1, 1.0e-3));
  return unwrap(WaveguideVoice::create(shape, design.filters, kRate, 0.2));
}

double zeroCrossingHz(const std::vector<double>& x, double fs) {
  std::vector<double> marks;
  for (std::size_t i = 1; i < x.size(); ++i) {
    if (x[i - 1] <= 0.0 && x[i] > 0.0) marks.push_back(static_cast<double>(i));
  }
  REQUIRE(marks.size() >= 4);
  double period = 0.0;
  for (std::size_t i = 1; i < marks.size(); ++i)
    period += marks[i] - marks[i - 1];
  period /= static_cast<double>(marks.size() - 1);
  return fs / period;
}

class FailingVoice final : public IVoice {
 public:
  explicit FailingVoice(int succeed) : succeed_(succeed) {}
  SampleRateHz sampleRate() const override { return kRate; }
  Result<FrameCount, QuantityError> renderBlock(double* out,
                                                FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    if (calls_ >= succeed_) return R::failure(QuantityError::DomainError);
    ++calls_;
    const auto count = static_cast<std::uint64_t>(n.value());
    for (std::uint64_t i = 0; i < count; ++i) out[i] = 0.1;
    return R::success(n);
  }
  void reset() override { calls_ = 0; }

 private:
  int succeed_;
  int calls_ = 0;
};
}  // namespace

TEST_CASE("three winds meet the audible-physics bar", "[voice]") {
  REQUIRE(kInstrumentRenderStatusCount == 26);
  CHECK(instrumentsThatRenderAudio() == 3);
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    const char* name = kInstrumentRenderStatus[i].name;
    const bool renders = std::strcmp(name, "clarinet") == 0 ||
                         std::strcmp(name, "flute") == 0 ||
                         std::strcmp(name, "saxophone") == 0;
    CHECK(kInstrumentRenderStatus[i].rendersAudio == renders);
    CHECK(name != nullptr);
    CHECK(kInstrumentRenderStatus[i].gap != nullptr);
  }
}

TEST_CASE("a waveguide voice renders pitch and physical decay to WAV",
          "[voice]") {
  const auto system = lossyString();
  const auto design = unwrap(designTunedLoop(system, kRate, 6, 0));
  const double pole = design.partials[0].frequency.value();
  const double decay = design.partials[0].decayRate;
  REQUIRE(decay > 0.0);
  const auto shape = unwrap(InitialShape::mode(system, 1, 1.0e-3));
  auto voice = unwrap(WaveguideVoice::create(shape, design.filters, kRate, 0.2));

  constexpr std::uint64_t kFrames = 16384;
  MemorySink memory;
  const auto stats = unwrap(renderVoice(
      voice, VoiceRenderConfig{FrameCount{kFrames}, 512}, memory));
  CHECK(stats.framesRendered == kFrames);
  CHECK(stats.isClean());
  CHECK(stats.peakAbs > 1.0e-4);
  CHECK(stats.nanCount == 0);
  CHECK(stats.infCount == 0);
  const std::vector<double>& samples = memory.samples();
  REQUIRE(samples.size() == kFrames);
  for (double v : samples) CHECK(std::isfinite(v));

  std::vector<double> window(samples.begin(), samples.begin() + 8192);
  const auto est = unwrap(estimateSinusoid(
      window, kRate, Window::BlackmanHarris, FrequencyHz{pole - 30.0},
      FrequencyHz{pole + 30.0}));
  CHECK(std::fabs(est.frequency.value() - pole) < 0.2);
  CHECK(std::fabs(zeroCrossingHz(window, kFs) - pole) / pole < 0.02);

  auto amplitude = [&](std::size_t start) {
    std::vector<double> w(samples.begin() + static_cast<std::ptrdiff_t>(start),
                          samples.begin() +
                              static_cast<std::ptrdiff_t>(start + 4096));
    return unwrap(estimateSinusoid(
                      w, kRate, Window::BlackmanHarris,
                      FrequencyHz{pole - 30.0}, FrequencyHz{pole + 30.0}))
        .amplitude;
  };
  const double early = amplitude(0);
  const double later = amplitude(8192);
  CHECK(later < early);
  const double measured = std::log(early / later) / (8192.0 / kFs);
  CHECK(nearlyEqual(measured, decay, 0.0, 2.0e-2));

  voice.reset();
  const char* path = "/tmp/mmath-voice-waveguide.wav";
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

TEST_CASE("voice block size does not change the samples", "[voice]") {
  auto a = makeVoice();
  auto b = makeVoice();
  constexpr std::uint64_t kFrames = 300;
  MemorySink left;
  MemorySink right;
  CHECK(renderVoice(a, VoiceRenderConfig{FrameCount{kFrames}, 64}, left).ok());
  CHECK(renderVoice(b, VoiceRenderConfig{FrameCount{kFrames}, 127}, right).ok());
  REQUIRE(left.samples().size() == right.samples().size());
  for (std::size_t i = 0; i < left.samples().size(); ++i)
    CHECK(left.samples()[i] == right.samples()[i]);

  a.reset();
  MemorySink again;
  CHECK(renderVoice(a, VoiceRenderConfig{FrameCount{kFrames}, 64}, again).ok());
  for (std::size_t i = 0; i < again.samples().size(); ++i)
    CHECK(again.samples()[i] == left.samples()[i]);
}

TEST_CASE("voice render refuses a bad block and a failing stepper",
          "[voice]") {
  auto voice = makeVoice();
  MemorySink memory;
  CHECK(renderVoice(voice, VoiceRenderConfig{FrameCount{32}, 0}, memory)
            .error() == RenderError::InvalidConfig);
  CHECK(memory.samples().empty());

  FailingVoice failing(1);
  MemorySink kept;
  CHECK(renderVoice(failing, VoiceRenderConfig{FrameCount{40}, 16}, kept)
            .error() == RenderError::VoiceFailure);
  CHECK(kept.samples().size() == 16);

  WaveguideVoice bad = voice;
  CHECK(bad.renderBlock(nullptr, FrameCount{4}).error() ==
        QuantityError::InvalidArgument);
}
