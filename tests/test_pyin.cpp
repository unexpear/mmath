// D35 — pYIN (Catch2).
//
// On a clean tone pYIN matches YIN's precision and is voiced throughout.
// Across tone / silence / noise / tone it marks voicing correctly. On a
// noisy tone whose fundamental keeps fading, plain frame-wise YIN makes
// gross (octave) errors in half the frames; pYIN's HMM removes all of them
// (measured 142 -> 0 of 278). On clean vibrato pYIN equals YIN exactly.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/pitch.h>
#include <mmathen/analysis/pyin.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

void addTone(std::vector<double>& x, std::size_t from, std::size_t to, double f0,
             const std::vector<double>& amps, double phase = 0.0) {
  for (std::size_t i = from; i < to; ++i)
    for (std::size_t h = 0; h < amps.size(); ++h)
      x[i] += amps[h] * std::sin(2.0 * std::numbers::pi * f0 * static_cast<double>(h + 1) *
                                     static_cast<double>(i) / 48000.0 + phase);
}
void addNoise(std::vector<double>& x, std::size_t from, std::size_t to, double amp,
              std::uint64_t seed) {
  std::uint64_t s = seed;
  for (std::size_t i = from; i < to; ++i) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    x[i] += amp * (static_cast<double>(s >> 11) * (2.0 / 9007199254740992.0) - 1.0);
  }
}
}  // namespace

TEST_CASE("pYIN on a clean tone matches YIN and stays voiced", "[d35pyin]") {
  std::vector<double> x(48000, 0.0);
  addTone(x, 0, x.size(), 261.63, {0.5, 0.25, 0.1});
  const auto p = unwrap(pyinTrack(x, kRate, 2048, 512));
  for (const auto& fr : p) {
    CHECK(fr.estimate.voiced);
    CHECK(std::fabs(fr.estimate.frequencyHz / 261.63 - 1.0) < 1e-4);
  }
}

TEST_CASE("pYIN voicing across tone, silence, noise, tone", "[d35pyin]") {
  std::vector<double> x(4 * 24000, 0.0);
  addTone(x, 0, 24000, 220.0, {0.5, 0.2});
  addNoise(x, 48000, 72000, 0.3, 7);
  addTone(x, 72000, 96000, 330.0, {0.5, 0.2});
  const auto p = unwrap(pyinTrack(x, kRate, 2048, 512));
  int wrong = 0, total = 0;
  for (const auto& fr : p) {
    const double t = fr.timeSeconds;
    // Skip frames straddling a boundary (the frame is 43 ms long).
    const double edge = std::fmod(t, 0.5);
    if (edge < 0.03 || edge > 0.47) continue;
    const bool shouldVoice = t < 0.5 || t > 1.5;
    ++total;
    if (fr.estimate.voiced != shouldVoice) ++wrong;
    if (shouldVoice && fr.estimate.voiced) {
      const double f = t < 0.5 ? 220.0 : 330.0;
      CHECK(std::fabs(fr.estimate.frequencyHz / f - 1.0) < 2e-3);
    }
  }
  CHECK(total > 100);
  CHECK(wrong == 0);
}

TEST_CASE("pYIN removes YIN's gross errors on a noisy, fading fundamental",
          "[d35pyin]") {
  // 180 Hz with a fundamental that fades to nearly nothing every 0.25 s
  // and a strong 2nd harmonic, plus noise at about 6 dB SNR.
  const std::size_t n = 3 * 48000;
  std::vector<double> x(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    const double fund = 0.05 + 0.25 * (0.5 + 0.5 * std::cos(2.0 * std::numbers::pi * 4.0 * t));
    x[i] = fund * std::sin(2.0 * std::numbers::pi * 180.0 * t) +
           0.35 * std::sin(2.0 * std::numbers::pi * 360.0 * t + 0.4) +
           0.12 * std::sin(2.0 * std::numbers::pi * 540.0 * t + 1.1);
  }
  addNoise(x, 0, n, 0.25, 31);
  PitchSearch s;
  s.fMinHz = 60.0;
  s.fMaxHz = 1000.0;
  const auto y = unwrap(trackPitch(x, kRate, 2048, 512, PitchMethod::Yin, s));
  const auto p = unwrap(pyinTrack(x, kRate, 2048, 512));
  // Gross pitch error (the literature's GPE): off by more than 20 %.
  auto gross = [](const std::vector<PitchFrame>& tr) {
    int g = 0;
    for (const auto& fr : tr)
      if (std::fabs(fr.estimate.frequencyHz / 180.0 - 1.0) > 0.20) ++g;
    return g;
  };
  const int gy = gross(y), gp = gross(p);
  INFO("YIN gross errors " << gy << ", pYIN " << gp << " of " << p.size());
  CHECK(gy > 100);  // measured 142 of 278: YIN jumps to the octave
  CHECK(gp == 0);   // measured 0
  // Within 3 % (fine accuracy): pYIN misses only 2 frames, at the deepest
  // fades of the fundamental in 6 dB SNR noise (measured 186.5 Hz there).
  int fine = 0;
  for (const auto& fr : p)
    if (std::fabs(fr.estimate.frequencyHz / 180.0 - 1.0) > 0.03) ++fine;
  CHECK(fine <= 2);
}

TEST_CASE("pYIN follows vibrato; refusals", "[d35pyin]") {
  const std::size_t n = 2 * 48000;
  std::vector<double> x(n);
  double ph = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    const double f = 300.0 * std::pow(2.0, 0.5 / 12.0 * std::sin(2.0 * std::numbers::pi * 5.0 * t));
    ph += 2.0 * std::numbers::pi * f / 48000.0;
    x[i] = 0.5 * std::sin(ph) + 0.2 * std::sin(2.0 * ph);
  }
  const auto p = unwrap(pyinTrack(x, kRate, 2048, 256));
  PitchSearch s;
  s.fMinHz = 60.0;
  s.fMaxHz = 1000.0;
  const auto yin = unwrap(trackPitch(x, kRate, 2048, 256, PitchMethod::Yin, s));
  REQUIRE(yin.size() == p.size());
  double worstCents = 0.0, worstVsYin = 0.0;
  for (std::size_t i = 0; i < p.size(); ++i)
    worstVsYin = std::max(worstVsYin, std::fabs(1200.0 * std::log2(
                                          p[i].estimate.frequencyHz / yin[i].estimate.frequencyHz)));
  CHECK(worstVsYin < 0.01);  // measured 0: on a clean tone pYIN = YIN
  for (const auto& fr : p) {
    CHECK(fr.estimate.voiced);
    // Reference: the instantaneous f0 averaged over the frame's window (a
    // 43 ms frame measures the mean period, not the centre value).
    double f = 0.0;
    for (int k = -1024; k < 1024; ++k) {
      const double t = fr.timeSeconds + k / 48000.0;
      f += 300.0 * std::pow(2.0, 0.5 / 12.0 * std::sin(2.0 * std::numbers::pi * 5.0 * t));
    }
    f /= 2048.0;
    worstCents = std::max(worstCents, std::fabs(1200.0 * std::log2(fr.estimate.frequencyHz / f)));
  }
  // The remaining offset (measured 10.9 cents at the fastest pitch change)
  // is YIN's own window bias for a 43 ms frame, identical in both.
  CHECK(worstCents < 12.0);
  PyinOptions bad;
  bad.switchProb = 0.0;
  CHECK(pyinTrack(x, kRate, 2048, 256, bad).error() == QuantityError::InvalidArgument);
  CHECK(pyinTrack(std::vector<double>(100, 0.0), kRate, 2048, 256).error() ==
        QuantityError::InvalidArgument);
}
