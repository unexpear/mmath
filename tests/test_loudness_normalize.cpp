// D33 — loudness normalization (Catch2).
//
// A quiet programme is brought to -14 LUFS within 0.1 LU under a -1 dBTP
// ceiling (the limiter engages; sample peaks within the ceiling, true peak
// within D31's residual); a programme that only needs gain is a pure,
// bit-exact gain with no limiting; turning down works the same way; a
// target too loud for the ceiling is reported as not reached (never
// faked); silence and bad arguments are refused; length is preserved.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/bs1770.h>
#include <mmathen/dsp/loudness_normalize.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

// Stereo "music-like" programme: decaying plucks over pink-ish noise, with
// attack transients: a crest factor high enough that reaching -14 LUFS
// needs the limiter.
std::vector<double> programme(double scale, double seconds = 8.0) {
  const auto n = static_cast<std::size_t>(seconds * 48000.0);
  std::vector<double> x(2 * n);
  std::uint64_t s = 99;
  double b = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const double w = static_cast<double>(s >> 11) * (2.0 / 9007199254740992.0) - 1.0;
    b = 0.97 * b + 0.03 * w;  // low-passed noise bed
    const double t = static_cast<double>(i % 24000) / 48000.0;
    const double pluck = std::exp(-6.0 * t) *
                         std::sin(2.0 * std::numbers::pi * 220.0 * static_cast<double>(i) / 48000.0);
    // Drum-like attack transients (8 samples) give a high crest factor.
    const double hit = (i % 24000) < 8 ? 6.0 : 0.0;
    x[2 * i] = scale * (0.8 * pluck + 0.6 * b + hit);
    x[2 * i + 1] = scale * (0.7 * pluck + 0.6 * b - hit);
  }
  return x;
}

double lufsOf(const std::vector<double>& x) {
  auto m = unwrap(LoudnessMeter::create(kRate, 2));
  m.push(x.data(), x.size() / 2);
  return m.integratedLufs();
}
}  // namespace

TEST_CASE("quiet programme is brought to -14 LUFS under -1 dBTP",
          "[d33norm]") {
  auto x = programme(0.05);
  const std::size_t len = x.size();
  const auto rep = unwrap(normalizeLoudness(x, 2, kRate));
  CHECK(x.size() == len);
  CHECK(rep.inputLufs < -30.0);
  CHECK(rep.reachedTarget);
  CHECK(std::fabs(rep.outputLufs - -14.0) <= 0.1);
  CHECK(std::fabs(lufsOf(x) - rep.outputLufs) < 1e-9);  // report is honest
  CHECK(rep.limited);
  double sp = 0.0;
  for (double v : x) sp = std::max(sp, std::fabs(v));
  CHECK(20.0 * std::log10(sp) <= -1.0 + 1e-9);  // sample peaks: guaranteed
  CHECK(rep.outputTruePeakDb <= -1.0 + 0.2);     // true peak: D31 residual
}

TEST_CASE("pure gain when no limiting is needed, up or down", "[d33norm]") {
  for (const double target : {-28.0, -40.0}) {  // below the ceiling: gain only
    auto x = programme(0.05);
    const auto orig = x;
    NormalizeTarget t;
    t.lufs = target;
    const auto rep = unwrap(normalizeLoudness(x, 2, kRate, t));
    CHECK_FALSE(rep.limited);
    CHECK(rep.passes == 1);
    CHECK(rep.reachedTarget);
    const double lin = std::pow(10.0, rep.gainDb / 20.0);
    bool exact = true;
    for (std::size_t i = 0; i < x.size(); ++i) exact = exact && x[i] == orig[i] * lin;
    CHECK(exact);
    // Pure gain moves loudness by exactly the gain.
    CHECK(std::fabs(rep.outputLufs - (rep.inputLufs + rep.gainDb)) < 1e-6);
  }
}

TEST_CASE("an unreachable target is reported, not faked", "[d33norm]") {
  auto x = programme(0.05);
  NormalizeTarget t;
  t.lufs = -3.0;  // far too loud for a -1 dBTP ceiling on this material
  t.maxPasses = 4;
  const auto rep = unwrap(normalizeLoudness(x, 2, kRate, t));
  CHECK_FALSE(rep.reachedTarget);
  CHECK(rep.passes == 4);
  CHECK(rep.outputLufs < -3.0 - 0.1);
  double sp = 0.0;
  for (double v : x) sp = std::max(sp, std::fabs(v));
  CHECK(20.0 * std::log10(sp) <= -1.0 + 1e-9);  // still within the ceiling
}

TEST_CASE("mono works; silence and bad arguments are refused", "[d33norm]") {
  std::vector<double> mono(48000 * 4);
  for (std::size_t i = 0; i < mono.size(); ++i)
    mono[i] = 0.1 * std::sin(2.0 * std::numbers::pi * 1000.0 * static_cast<double>(i) / 48000.0);
  NormalizeTarget t;
  t.lufs = -23.0;
  const auto rep = unwrap(normalizeLoudness(mono, 1, kRate, t));
  CHECK(std::fabs(rep.outputLufs - -23.0) <= 0.1);

  std::vector<double> silent(2 * 48000, 0.0);
  CHECK(normalizeLoudness(silent, 2, kRate).error() == QuantityError::DomainError);
  auto x = programme(0.05, 2.0);
  NormalizeTarget bad;
  bad.lufs = 3.0;
  CHECK(normalizeLoudness(x, 2, kRate, bad).error() == QuantityError::InvalidArgument);
  CHECK(normalizeLoudness(x, 3, kRate).error() == QuantityError::InvalidArgument);
}
