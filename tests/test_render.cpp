// Phase 2.5 — offline rendering and WAV certification tests (Catch2).
//
// The golden fixture: 440 Hz sine at peak 0.25, 48000 frames @ 48 kHz.
// Float-buffer statistics are checked pre-quantization; PCM conversion is
// checked separately against known fixtures plus an independent byte-level
// decode of the written file (a co-developed reader sharing the writer's
// assumptions would prove nothing).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numbers>
#include <string>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

Signal<SignedAudioValue> goldenSignal() {
  return sine(FrequencyHz{440.0}, constant(AmplitudeLinear{0.25}));
}

RenderConfig goldenConfig(std::uint64_t block = 512) {
  return RenderConfig{SampleRateHz{48000.0}, FrameCount{48000},
                      TimePointSeconds{0.0}, block};
}

std::uint16_t readU16LE(const std::vector<char>& b, std::size_t o) {
  return static_cast<std::uint16_t>(
      (static_cast<unsigned char>(b[o])) |
      (static_cast<unsigned char>(b[o + 1]) << 8));
}

std::uint32_t readU32LE(const std::vector<char>& b, std::size_t o) {
  return static_cast<std::uint32_t>(readU16LE(b, o)) |
         (static_cast<std::uint32_t>(readU16LE(b, o + 2)) << 16);
}
}  // namespace

TEST_CASE("block size changes batching, never values", "[render]") {
  std::vector<std::vector<double>> runs;
  for (const std::uint64_t bs : {64ULL, 127ULL, 512ULL}) {
    MemorySink mem;
    REQUIRE(renderOffline(goldenSignal(), goldenConfig(bs), mem).ok());
    runs.push_back(mem.samples());
  }
  // Exact sequence equality, including the final partial block of the
  // 127-frame run. Any phase reset or state leak would show up here.
  CHECK(runs[0] == runs[1]);
  CHECK(runs[1] == runs[2]);
  CHECK(runs[0].size() == 48000);
}

TEST_CASE("null and memory sinks consume the same buffers", "[render]") {
  NullSink nullSink;
  MemorySink mem;
  const auto rsNull = unwrap(renderOffline(goldenSignal(), goldenConfig(), nullSink));
  const auto rsMem = unwrap(renderOffline(goldenSignal(), goldenConfig(), mem));
  CHECK(rsNull.framesRendered == 48000);
  CHECK(rsNull.framesRendered == rsMem.framesRendered);
  CHECK(rsNull.peakAbs == mem.stats().peakAbs);
  CHECK(rsNull.nanCount == mem.stats().nanCount);
  CHECK(rsNull.infCount == mem.stats().infCount);
  CHECK(rsNull.overloadCount == mem.stats().overloadCount);
  CHECK(rsNull.isClean());
}

TEST_CASE("golden signal statistics, pre-quantization", "[render]") {
  MemorySink mem;
  const auto stats = unwrap(renderOffline(goldenSignal(), goldenConfig(), mem));
  REQUIRE(stats.framesRendered == 48000);
  CHECK(stats.isClean());

  const auto& s = mem.samples();
  double sum = 0.0;
  double sumSq = 0.0;
  for (double x : s) {
    sum += x;
    sumSq += x * x;
  }
  CHECK(nearlyEqual(sum / s.size(), 0.0, 1e-6, 0.0));
  CHECK(nearlyEqual(std::sqrt(sumSq / s.size()), 0.25 / std::numbers::sqrt2,
                    1e-6, 1e-3));

  // Zero-crossing frequency estimate: rising crossings * fs / N.
  std::uint64_t crossings = 0;
  for (std::size_t i = 1; i < s.size(); ++i) {
    if (s[i - 1] <= 0.0 && s[i] > 0.0) ++crossings;
  }
  const double estFreq =
      static_cast<double>(crossings) * 48000.0 / static_cast<double>(s.size());
  CHECK(nearlyEqual(estFreq, 440.0, 1.0, 0.0));
}

TEST_CASE("encoder fixtures pin the PCM mapping", "[render]") {
  CHECK(WavSink::encodeSample(0.0) == 0);
  CHECK(WavSink::encodeSample(1.0) == 32767);
  CHECK(WavSink::encodeSample(-1.0) == -32767);
  CHECK(WavSink::encodeSample(0.5) == 16384);  // 16383.5 rounds half away
  CHECK(WavSink::encodeSample(2.0) == 32767);  // clamped, counted upstream
  CHECK(WavSink::encodeSample(-2.0) == -32767);
  CHECK(WavSink::encodeSample(
            std::numeric_limits<double>::quiet_NaN()) == 0);
  CHECK(WavSink::encodeSample(
            std::numeric_limits<double>::infinity()) == 0);
}

TEST_CASE("golden WAV file: metadata, length, independent decode", "[render]") {
  const std::string path = "a4_golden_test.wav";
  {
    auto wav = unwrap(
        WavSink::open(path, SampleRateHz{48000.0}, FrameCount{48000}));
    REQUIRE(renderOffline(goldenSignal(), goldenConfig(), wav).ok());
    CHECK(unwrap(wav.close()) == FrameCount{48000});
  }

  std::ifstream in(path, std::ios::binary);
  REQUIRE(in.good());
  std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
  REQUIRE(bytes.size() == 44 + 48000 * 2);
  CHECK(std::string(bytes.data(), 4) == "RIFF");
  CHECK(readU32LE(bytes, 4) == 36 + 48000 * 2);
  CHECK(std::string(bytes.data() + 8, 4) == "WAVE");
  CHECK(std::string(bytes.data() + 12, 4) == "fmt ");
  CHECK(readU32LE(bytes, 16) == 16);
  CHECK(readU16LE(bytes, 20) == 1);      // PCM
  CHECK(readU16LE(bytes, 22) == 1);      // mono
  CHECK(readU32LE(bytes, 24) == 48000);  // sample rate
  CHECK(readU32LE(bytes, 28) == 96000);  // byte rate
  CHECK(readU16LE(bytes, 32) == 2);      // block align
  CHECK(readU16LE(bytes, 34) == 16);     // bits per sample
  CHECK(std::string(bytes.data() + 36, 4) == "data");
  CHECK(readU32LE(bytes, 40) == 96000);

  // Independent decode: int16 LE -> float, compared against the float render
  // within 1 LSB (quantized output is never compared to the unquantized
  // identity exactly).
  MemorySink mem;
  REQUIRE(renderOffline(goldenSignal(), goldenConfig(), mem).ok());
  REQUIRE(mem.samples().size() == 48000);
  for (std::size_t i = 0; i < 48000; ++i) {
    const auto raw = readU16LE(bytes, 44 + i * 2);
    const double decoded =
        static_cast<double>(static_cast<std::int16_t>(raw)) / 32767.0;
    CHECK(nearlyEqual(decoded, mem.samples()[i], 1.0 / 32767.0 + 1e-12, 0.0));
  }
}

TEST_CASE("WAV open validates rate and length", "[render]") {
  // Non-integer rate has no exact header field.
  CHECK(WavSink::open("x.wav", SampleRateHz{48000.5}, FrameCount{8}).error() ==
        RenderError::InvalidConfig);
  // 2*rate must fit the 32-bit byte-rate field.
  CHECK(WavSink::open("x.wav", SampleRateHz{3000000000.0}, FrameCount{8})
            .error() == RenderError::InvalidConfig);
  CHECK(WavSink::open("x.wav", SampleRateHz{0.0}, FrameCount{8}).error() ==
        RenderError::InvalidConfig);
}

TEST_CASE("file failures are failures, never partial successes", "[render]") {
  // Unwritable path: open itself fails.
  CHECK(WavSink::open("nonexistent_dir_xyz/a.wav", SampleRateHz{48000.0},
                      FrameCount{48000})
            .error() == RenderError::SinkFailure);
  // Short write: close verifies declared length and fails.
  {
    auto wav = unwrap(
        WavSink::open("a4_short_test.wav", SampleRateHz{48000.0},
                      FrameCount{48000}));
    MemorySink mem;
    REQUIRE(renderOffline(goldenSignal(), goldenConfig(), mem).ok());
    const double* first =
        mem.samples().data();  // only first 100 frames consumed
    REQUIRE(wav.consume(first, FrameCount{100}));
    CHECK(wav.close().error() == RenderError::SinkFailure);
  }
}

TEST_CASE("render config validation", "[render]") {
  MemorySink mem;
  CHECK(renderOffline(goldenSignal(),
                      RenderConfig{SampleRateHz{0.0}, FrameCount{48000},
                                   TimePointSeconds{0.0}, 512},
                      mem)
            .error() == RenderError::InvalidConfig);
  CHECK(renderOffline(goldenSignal(), goldenConfig(0), mem).error() ==
        RenderError::InvalidConfig);
  // Absurd block sizes report InvalidConfig instead of dying in the buffer
  // allocation (audit F2). Small frame count: rejected before any alloc.
  CHECK(renderOffline(goldenSignal(),
                      RenderConfig{SampleRateHz{48000.0}, FrameCount{8},
                                   TimePointSeconds{0.0},
                                   kMaxRenderFrames + 2},
                      mem)
            .error() == RenderError::InvalidConfig);
  CHECK(renderOffline(goldenSignal(),
                      RenderConfig{SampleRateHz{48000.0},
                                   FrameCount{kMaxRenderFrames + 1},
                                   TimePointSeconds{0.0}, 512},
                      mem)
            .error() == RenderError::InvalidConfig);
  // Zero frames is valid: no consume calls, zero stats, success.
  CHECK(unwrap(renderOffline(goldenSignal(),
                             RenderConfig{SampleRateHz{48000.0}, FrameCount{0},
                                          TimePointSeconds{0.0}, 512},
                             mem))
            .framesRendered == 0);
}

TEST_CASE("non-finite input completes flagged, never hidden", "[render]") {
  auto nanSig = Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [](TimePointSeconds t) {
        return SignedAudioValue{t.value() >= 0.005
                                    ? std::numeric_limits<double>::quiet_NaN()
                                    : 0.0};
      },
      "nan-after-0.005")};
  MemorySink mem;
  const auto stats = unwrap(renderOffline(
      nanSig,
      RenderConfig{SampleRateHz{48000.0}, FrameCount{480}, TimePointSeconds{0.0},
                   512},
      mem));
  CHECK(stats.framesRendered == 480);
  CHECK(stats.nanCount > 0);
  CHECK_FALSE(stats.isClean());
}
