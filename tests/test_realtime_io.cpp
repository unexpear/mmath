// D22 — the app's testable core: WAV reader, SPSC ring, band analyzer.
//
// WAV: byte-exact round trip through wav.h's writer, and hand-built files
// for every accepted encoding and every error path. SPSC ring: FIFO order,
// wrap-around, full/empty behaviour, and a two-thread stress test (a
// counting sequence must arrive complete and in order). Analyzer: a tone's
// band level, exact linear release, peak hold timing, and block-size
// invariance (identical results however the samples are pushed).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numbers>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <mmathen/analysis/analyzer.h>
#include <mmathen/analysis/spectrum.h>
#include <mmathen/core/spsc_ring.h>
#include <mmathen/math/comparison.h>
#include <mmathen/sink/wav.h>
#include <mmathen/sink/wav_reader.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

void put16(std::vector<unsigned char>& b, std::uint32_t v) {
  b.push_back(static_cast<unsigned char>(v & 0xFF));
  b.push_back(static_cast<unsigned char>((v >> 8) & 0xFF));
}
void put32(std::vector<unsigned char>& b, std::uint32_t v) {
  put16(b, v & 0xFFFF);
  put16(b, v >> 16);
}
void putTag(std::vector<unsigned char>& b, const char* t) {
  b.insert(b.end(), t, t + 4);
}

// A WAV file with the given fmt fields and raw data bytes. `extensible`
// wraps the tag in WAVE_FORMAT_EXTENSIBLE; `listChunk` inserts an odd-sized
// LIST chunk (with its pad byte) before the data.
std::vector<unsigned char> makeWav(std::uint16_t tag, std::uint16_t channels,
                                   std::uint32_t rate, std::uint16_t bits,
                                   const std::vector<unsigned char>& data,
                                   bool extensible = false,
                                   bool listChunk = false,
                                   std::uint32_t declaredData = 0) {
  std::vector<unsigned char> b;
  putTag(b, "RIFF");
  put32(b, 0);  // RIFF size (not checked by the reader)
  putTag(b, "WAVE");
  putTag(b, "fmt ");
  put32(b, extensible ? 40 : 16);
  put16(b, extensible ? 0xFFFE : tag);
  put16(b, channels);
  put32(b, rate);
  put32(b, rate * channels * bits / 8);
  put16(b, static_cast<std::uint16_t>(channels * bits / 8));
  put16(b, bits);
  if (extensible) {
    put16(b, 22);    // cbSize
    put16(b, bits);  // valid bits
    put32(b, 0);     // channel mask
    put16(b, tag);   // SubFormat GUID begins with the format tag
    for (int i = 0; i < 14; ++i) b.push_back(0);
  }
  if (listChunk) {
    putTag(b, "LIST");
    put32(b, 3);
    b.push_back('a');
    b.push_back('b');
    b.push_back('c');
    b.push_back(0);  // RIFF pad byte
  }
  putTag(b, "data");
  put32(b, declaredData != 0 ? declaredData
                             : static_cast<std::uint32_t>(data.size()));
  b.insert(b.end(), data.begin(), data.end());
  return b;
}

void put24(std::vector<unsigned char>& b, std::int32_t v) {
  const auto u = static_cast<std::uint32_t>(v);
  b.push_back(static_cast<unsigned char>(u & 0xFF));
  b.push_back(static_cast<unsigned char>((u >> 8) & 0xFF));
  b.push_back(static_cast<unsigned char>((u >> 16) & 0xFF));
}
void putF32(std::vector<unsigned char>& b, float f) {
  std::uint32_t u;
  std::memcpy(&u, &f, 4);
  put32(b, u);
}
}  // namespace

TEST_CASE("WAV round trip through the writer is exact", "[d22core]") {
  const std::string path = "d22_roundtrip.wav";
  std::vector<double> x = {0.0, 0.5, -0.5, 1.0, -1.0, 1.7, -2.0, 0.123456789};
  for (int i = 0; i < 992; ++i)
    x.push_back(0.7 * std::sin(0.013 * i));
  {
    auto sink = unwrap(WavSink::open(path, SampleRateHz{44100.0},
                                     FrameCount{x.size()}));
    REQUIRE(sink.consume(x.data(), FrameCount{x.size()}));
    REQUIRE(sink.close().ok());
  }
  const auto w = unwrap(readWavFile(path));
  std::remove(path.c_str());
  CHECK(w.rate == SampleRateHz{44100.0});
  CHECK(w.channels == 1);
  CHECK(w.bitsPerSample == 16);
  CHECK(w.frames == x.size());
  CHECK(w.outOfRange == 0);
  bool exact = true;
  for (std::size_t i = 0; i < x.size(); ++i)
    exact = exact && w.samples[i] == WavSink::encodeSample(x[i]) / 32767.0;
  CHECK(exact);
  CHECK(readWavFile("no_such_file_d22.wav").error() == QuantityError::RangeError);
}

TEST_CASE("WAV reader handles 24-bit, float, extensible, extra chunks",
          "[d22core]") {
  // 24-bit stereo, two frames, odd LIST chunk before data.
  std::vector<unsigned char> d24;
  put24(d24, 8388607);   // L +1
  put24(d24, -8388607);  // R -1
  put24(d24, -8388608);  // L just below -1 (reported, not clamped)
  put24(d24, 0);         // R 0
  const auto w24 = unwrap(parseWav(makeWav(1, 2, 96000, 24, d24, false, true)));
  CHECK(w24.channels == 2);
  CHECK(w24.frames == 2);
  CHECK(w24.samples[0] == 1.0);
  CHECK(w24.samples[1] == -1.0);
  CHECK(w24.samples[2] == -8388608.0 / 8388607.0);
  CHECK(w24.outOfRange == 1);
  CHECK(w24.channel(1) == std::vector<double>{-1.0, 0.0});
  CHECK(w24.mono()[0] == 0.0);
  // 32-bit float, plain and extensible.
  std::vector<unsigned char> df;
  putF32(df, 0.25f);
  putF32(df, -0.5f);
  for (const bool ext : {false, true}) {
    const auto wf = unwrap(parseWav(makeWav(3, 1, 48000, 32, df, ext)));
    CHECK(wf.isFloat);
    CHECK(wf.samples == std::vector<double>{0.25, -0.5});
  }
  // 32-bit integer PCM.
  std::vector<unsigned char> d32;
  put32(d32, 2147483647U);
  const auto w32 = unwrap(parseWav(makeWav(1, 1, 8000, 32, d32)));
  CHECK(w32.samples[0] == 1.0);
}

TEST_CASE("WAV reader rejects what it cannot read faithfully", "[d22core]") {
  std::vector<unsigned char> d16(8, 0);
  CHECK(parseWav(makeWav(2, 1, 44100, 16, d16)).error() ==
        QuantityError::DomainError);  // ADPCM
  CHECK(parseWav(makeWav(1, 1, 44100, 8, d16)).error() ==
        QuantityError::DomainError);  // 8-bit
  CHECK(parseWav(makeWav(3, 1, 44100, 64, d16)).error() ==
        QuantityError::DomainError);  // 64-bit float
  CHECK(parseWav(makeWav(1, 1, 44100, 16, d16, false, false, 100)).error() ==
        QuantityError::RangeError);  // declared data beyond the file
  std::vector<unsigned char> nan;
  putF32(nan, std::numeric_limits<float>::quiet_NaN());
  CHECK(parseWav(makeWav(3, 1, 44100, 32, nan)).error() ==
        QuantityError::InvalidArgument);
  std::vector<unsigned char> junk = {'R', 'I', 'F', 'X', 0, 0, 0, 0,
                                     'W', 'A', 'V', 'E'};
  CHECK(parseWav(junk).error() == QuantityError::InvalidArgument);
  auto noData = makeWav(1, 1, 44100, 16, d16);
  noData.resize(12 + 8 + 16);  // RIFF header + fmt chunk only
  CHECK(parseWav(noData).error() == QuantityError::InvalidArgument);
}

TEST_CASE("SPSC ring: FIFO order, wrap-around, full and empty", "[d22core]") {
  CHECK(SpscRing::create(6).error() == QuantityError::InvalidArgument);
  CHECK(SpscRing::create(1).error() == QuantityError::InvalidArgument);
  auto ring = unwrap(SpscRing::create(8));
  double in[16], out[16];
  for (int i = 0; i < 16; ++i) in[i] = i;
  CHECK(ring->write(in, 5) == 5);
  CHECK(ring->read(out, 3) == 3);
  CHECK(out[0] == 0.0);
  CHECK(out[2] == 2.0);
  CHECK(ring->write(in + 5, 10) == 6);  // only 6 free: never overwrites
  CHECK(ring->available() == 8);
  CHECK(ring->read(out, 16) == 8);      // wraps the physical buffer
  for (int i = 0; i < 8; ++i) CHECK(out[i] == 3.0 + i);
  CHECK(ring->read(out, 4) == 0);       // empty
}

TEST_CASE("SPSC ring: two threads stream a counting sequence intact",
          "[d22core]") {
  auto ring = unwrap(SpscRing::create(1024));
  constexpr std::size_t total = 1000000;
  std::thread producer([&] {
    std::vector<double> chunk(37);
    std::size_t next = 0;
    while (next < total) {
      const std::size_t n = (std::min)(chunk.size(), total - next);
      for (std::size_t i = 0; i < n; ++i)
        chunk[i] = static_cast<double>(next + i);
      std::size_t done = 0;
      while (done < n) done += ring->write(chunk.data() + done, n - done);
      next += n;
    }
  });
  std::vector<double> got(53);
  std::size_t expect = 0;
  bool inOrder = true;
  while (expect < total) {
    const std::size_t n = ring->read(got.data(), got.size());
    for (std::size_t i = 0; i < n; ++i) {
      inOrder = inOrder && got[i] == static_cast<double>(expect);
      ++expect;
    }
  }
  producer.join();
  CHECK(inOrder);
  CHECK(expect == total);
  CHECK(ring->available() == 0);
}

TEST_CASE("band analyzer: level, exact release, peak hold", "[d22core]") {
  const SampleRateHz rate{48000.0};
  MeterBallistics bal;
  bal.releaseDbPerSecond = 30.0;
  bal.peakHoldSeconds = 0.25;
  auto an = unwrap(BandAnalyzer::create(rate, 4096, 1024, 3, FrequencyHz{100.0},
                                        FrequencyHz{10000.0}, Window::Hann,
                                        bal));
  std::size_t k1 = 0;
  for (std::size_t i = 0; i < an.bands().size(); ++i)
    if (an.bands()[i].center.value() == 1000.0) k1 = i;
  // 1 kHz tone, amplitude 0.5, for 1 s.
  std::vector<double> tone(48000);
  for (std::size_t i = 0; i < tone.size(); ++i)
    tone[i] = 0.5 * std::sin(2.0 * std::numbers::pi * 1000.0 *
                             static_cast<double>(i) / 48000.0);
  const std::size_t frames = an.push(tone.data(), tone.size());
  CHECK(frames == (48000 / 1024) - 3);  // first full frame after 4096
  CHECK(nearlyEqual(an.raw()[k1].levelDb, 10.0 * std::log10(0.125), 0.05, 0.0));
  CHECK(an.display()[k1] == an.raw()[k1].levelDb);
  const double start = an.display()[k1];
  const double peak0 = an.peaks()[k1];
  // Silence: once the window holds no tone, the display falls exactly
  // release * dt per frame; the peak holds 0.25 s, then falls.
  std::vector<double> silence(1024, 0.0);
  const double dt = an.frameStepSeconds();  // 1024/48000
  for (int f = 0; f < 4; ++f) an.push(silence.data(), silence.size());
  // Frames 1..3 still see part of the tone; from frame 4 raw is the floor.
  const double afterFour = an.display()[k1];
  for (int f = 1; f <= 5; ++f) {
    an.push(silence.data(), silence.size());
    CHECK(nearlyEqual(an.display()[k1], afterFour - 30.0 * dt * f, 1e-9, 0.0));
  }
  CHECK(an.display()[k1] < start);
  // Hold: 0.25 s / 21.3 ms = 11.7 frames. After 9 silent frames the peak is
  // still at its value; after 30 it has fallen.
  CHECK(an.peaks()[k1] == peak0);
  for (int f = 0; f < 21; ++f) an.push(silence.data(), silence.size());
  CHECK(an.peaks()[k1] < peak0);
  CHECK(an.peaks()[k1] >= an.display()[k1]);
}

TEST_CASE("band analyzer is block-size invariant", "[d22core]") {
  const SampleRateHz rate{48000.0};
  std::vector<double> x(30000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = 0.3 * std::sin(0.05 * static_cast<double>(i)) +
           0.1 * std::sin(0.31 * static_cast<double>(i));
  std::vector<std::vector<double>> results;
  for (const std::size_t block : {1u, 7u, 1024u, 30000u}) {
    auto an = unwrap(BandAnalyzer::create(rate, 2048, 512, 1,
                                          FrequencyHz{30.0},
                                          FrequencyHz{17000.0},
                                          Window::BlackmanHarris,
                                          MeterBallistics{}));
    for (std::size_t i = 0; i < x.size(); i += block)
      an.push(x.data() + i, (std::min)(block, x.size() - i));
    results.push_back(an.display());
  }
  CHECK(results[0] == results[1]);
  CHECK(results[1] == results[2]);
  CHECK(results[2] == results[3]);
  CHECK(BandAnalyzer::create(rate, 1000, 512, 3, FrequencyHz{30.0},
                             FrequencyHz{17000.0}, Window::Hann,
                             MeterBallistics{})
            .error() == QuantityError::InvalidArgument);
}
