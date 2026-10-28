// D32 — multichannel WAV writer (Catch2).
//
// Mono 16-bit output is byte-identical to the certified WavSink; stereo
// 16- and 24-bit files round-trip exactly through parseWav; overloads and
// non-finite samples are counted; a short write fails close(); invalid
// configurations are refused before any file is created.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <mmathen/sink/wav.h>
#include <mmathen/sink/wav_path.h>
#include <mmathen/sink/wav_reader.h>
#include <mmathen/sink/wav_writer.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
std::vector<unsigned char> bytesOf(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}
std::vector<double> signal(std::size_t n, double phase) {
  std::vector<double> x = {0.0, 1.0, -1.0, 0.5, -0.5, 0.123456789};
  for (std::size_t i = x.size(); i < n; ++i)
    x.push_back(0.8 * std::sin(0.0137 * static_cast<double>(i) + phase));
  return x;
}
}  // namespace

TEST_CASE("mono 16-bit is byte-identical to WavSink", "[d32wav]") {
  const auto x = signal(1000, 0.0);
  {
    auto a = unwrap(WavSink::open("d32_a.wav", SampleRateHz{48000.0},
                                  FrameCount{x.size()}));
    REQUIRE(a.consume(x.data(), FrameCount{x.size()}));
    REQUIRE(a.close().ok());
    auto b = unwrap(WavWriter::open("d32_b.wav", SampleRateHz{48000.0},
                                    FrameCount{x.size()}, 1, 16));
    REQUIRE(b.write(x.data(), x.size()));
    REQUIRE(b.close().ok());
  }
  const auto ba = bytesOf("d32_a.wav"), bb = bytesOf("d32_b.wav");
  std::remove("d32_a.wav");
  std::remove("d32_b.wav");
  CHECK(ba.size() == 44 + 2 * x.size());
  CHECK(ba == bb);
}

TEST_CASE("stereo 16- and 24-bit round-trip exactly", "[d32wav]") {
  for (const int bits : {16, 24}) {
    const auto l = signal(3000, 0.0), r = signal(3000, 1.3);
    std::vector<double> inter;
    for (std::size_t i = 0; i < l.size(); ++i) {
      inter.push_back(l[i]);
      inter.push_back(r[i]);
    }
    {
      auto w = unwrap(WavWriter::open("d32_st.wav", SampleRateHz{44100.0},
                                      FrameCount{l.size()}, 2, bits));
      REQUIRE(w.write(inter.data(), 1000));  // in two blocks
      REQUIRE(w.write(inter.data() + 2000, l.size() - 1000));
      CHECK(w.overloads() == 0);
      REQUIRE(w.close().ok());
    }
    const auto d = unwrap(readWavFile("d32_st.wav"));
    std::remove("d32_st.wav");
    CHECK(d.channels == 2);
    CHECK(d.bitsPerSample == bits);
    CHECK(d.frames == l.size());
    const double fs = bits == 24 ? 8388607.0 : 32767.0;
    bool exact = true;
    for (std::size_t i = 0; i < inter.size(); ++i)
      exact = exact &&
              d.samples[i] == static_cast<double>(WavWriter::encode(inter[i], bits)) / fs;
    CHECK(exact);
  }
}

TEST_CASE("overloads are counted, short writes fail, bad configs refused",
          "[d32wav]") {
  {
    const double v[4] = {1.5, -3.0, std::numeric_limits<double>::quiet_NaN(), 0.2};
    auto w = unwrap(WavWriter::open("d32_o.wav", SampleRateHz{48000.0},
                                    FrameCount{2}, 2, 24));
    REQUIRE(w.write(v, 2));
    CHECK(w.overloads() == 3);
    REQUIRE(w.close().ok());
    std::remove("d32_o.wav");
  }
  {
    auto w = unwrap(WavWriter::open("d32_s.wav", SampleRateHz{48000.0},
                                    FrameCount{10}, 1, 16));
    const double z[5] = {};
    REQUIRE(w.write(z, 5));
    CHECK(w.close().error() == RenderError::SinkFailure);  // 5 of 10
    std::remove("d32_s.wav");
  }
  CHECK(WavWriter::open("d32_x.wav", SampleRateHz{48000.0}, FrameCount{1}, 0)
            .error() == RenderError::InvalidConfig);
  CHECK(WavWriter::open("d32_x.wav", SampleRateHz{48000.0}, FrameCount{1}, 2, 20)
            .error() == RenderError::InvalidConfig);
  CHECK(WavWriter::open("d32_x.wav", SampleRateHz{44100.5}, FrameCount{1}, 2)
            .error() == RenderError::InvalidConfig);
  CHECK(WavWriter::encode(1.0, 24) == 8388607);
  CHECK(WavWriter::encode(-1.0, 16) == -32767);
}

namespace {
std::string currentWavDir() {
#if defined(_MSC_VER)
  char* buffer = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&buffer, &length, "MMATH_WAV_DIR") != 0 || buffer == nullptr)
    return {};
  std::string value(buffer);
  std::free(buffer);
  return value;
#else
  const char* value = std::getenv("MMATH_WAV_DIR");
  if (value == nullptr) return {};
  return std::string(value);
#endif
}

void writeWavDir(const std::string& value) {
#if defined(_MSC_VER)
  _putenv_s("MMATH_WAV_DIR", value.c_str());
#else
  if (value.empty())
    unsetenv("MMATH_WAV_DIR");
  else
    setenv("MMATH_WAV_DIR", value.c_str(), 1);
#endif
}

struct WavDirGuard {
  std::string previous;
  explicit WavDirGuard(const std::string& next) : previous(currentWavDir()) {
    writeWavDir(next);
  }
  ~WavDirGuard() { writeWavDir(previous); }
};
}  // namespace

TEST_CASE("wav output follows the temp directory and MMATH_WAV_DIR", "[d32wav]") {
  const auto fallback = std::filesystem::temp_directory_path();
  {
    WavDirGuard cleared("");
    CHECK(wavOutputDirectory().lexically_normal() == fallback.lexically_normal());
  }
  CHECK(copyWavFile(std::filesystem::path("missing.wav"), {}));
  const auto absent = artifactWavPath("mmath-path-probe.wav");
  if (!absent.empty()) CHECK(absent.filename() == "mmath-path-probe.wav");

  const auto overrideDir = fallback / "mmath-wav-dir-1201";
  std::error_code error;
  std::filesystem::create_directories(overrideDir, error);
  REQUIRE(!error);
  {
    WavDirGuard overridden(overrideDir.string());
    CHECK(wavOutputDirectory().lexically_normal() == overrideDir.lexically_normal());
    const auto file = wavOutputPath("mmath-path-probe.wav");
    CHECK(file.filename() == "mmath-path-probe.wav");
    const double sample = 0.25;
    auto opened =
        unwrap(WavSink::open(file.string(), SampleRateHz{48000.0}, FrameCount{1}));
    REQUIRE(opened.consume(&sample, FrameCount{1}));
    REQUIRE(opened.close().ok());
    const auto beside = overrideDir / "mmath-path-probe-copy.wav";
    REQUIRE(copyWavFile(file, beside));
    CHECK(std::filesystem::file_size(beside) == std::filesystem::file_size(file));
    REQUIRE(copyWavToArtifacts(file, "mmath-path-probe.wav"));
    const auto artifact = artifactWavPath("mmath-path-probe.wav");
    if (!artifact.empty()) {
      CHECK(std::filesystem::is_regular_file(artifact));
      std::filesystem::remove(artifact, error);
    }
    std::filesystem::remove(file, error);
    std::filesystem::remove(beside, error);
  }
  std::filesystem::remove(overrideDir, error);
}
