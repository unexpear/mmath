// D31 — BS.1770 loudness and true peak (Catch2).
//
// The derived K-weighting reproduces the standard's published 48 kHz
// coefficients. The standard's own anchor: a 0 dBFS 1 kHz sine in one
// channel reads -3.01 LKFS (at 48 and 44.1 kHz). Stereo 1 kHz at -23 dBFS
// reads -23.0 LUFS momentary, short-term and integrated. The relative gate
// removes quiet passages 13 LU down; the absolute gate removes everything
// below -70 LUFS. Loudness range of a -20/-30 dBFS programme is 10 LU. True
// peak finds the peak of a fs/4 sine whose samples all miss it by 3 dB, and
// keeps its stated error bounds over frequency and phase.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/bs1770.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

// Interleaved stereo 1 kHz segments: {seconds, dBFS left, dBFS right}.
struct Seg {
  double seconds, left, right;
};
std::vector<double> program(double fs, const std::vector<Seg>& segs,
                            double f = 1000.0) {
  std::vector<double> x;
  std::size_t n0 = 0;
  for (const auto& s : segs) {
    const auto n = static_cast<std::size_t>(s.seconds * fs);
    const double al = s.left <= -300.0 ? 0.0 : std::pow(10.0, s.left / 20.0);
    const double ar = s.right <= -300.0 ? 0.0 : std::pow(10.0, s.right / 20.0);
    for (std::size_t i = 0; i < n; ++i) {
      const double v = std::sin(2.0 * std::numbers::pi * f *
                                static_cast<double>(n0 + i) / fs);
      x.push_back(al * v);
      x.push_back(ar * v);
    }
    n0 += n;
  }
  return x;
}

LoudnessMeter meter(double fs, const std::vector<double>& x) {
  auto m = unwrap(LoudnessMeter::create(SampleRateHz{fs}, 2));
  m.push(x.data(), x.size() / 2);
  return m;
}
}  // namespace

TEST_CASE("K-weighting matches the published 48 kHz coefficients",
          "[d31lufs]") {
  const auto k = unwrap(kWeighting(SampleRateHz{48000.0}));
  // BS.1770-5 Table 1 (pre-filter) and Table 2 (RLB); unchanged from -4.
  CHECK(std::fabs(k.shelf.b0 - 1.53512485958697) < 1e-9);
  CHECK(std::fabs(k.shelf.b1 - -2.69169618940638) < 1e-9);
  CHECK(std::fabs(k.shelf.b2 - 1.19839281085285) < 1e-9);
  CHECK(std::fabs(k.shelf.a1 - -1.69065929318241) < 1e-9);
  CHECK(std::fabs(k.shelf.a2 - 0.73248077421585) < 1e-9);
  CHECK(k.highpass.b0 == 1.0);
  CHECK(k.highpass.b1 == -2.0);
  CHECK(k.highpass.b2 == 1.0);
  CHECK(std::fabs(k.highpass.a1 - -1.99004745483398) < 1e-9);
  CHECK(std::fabs(k.highpass.a2 - 0.99007225036621) < 1e-9);
  CHECK((isStable(k.shelf) && isStable(k.highpass)));
}

TEST_CASE("0 dBFS 1 kHz in one channel reads -3.01 LKFS", "[d31lufs]") {
  for (const double fs : {48000.0, 44100.0}) {
    const auto m = meter(fs, program(fs, {{5.0, 0.0, -400.0}}));
    CHECK(std::fabs(m.integratedLufs() - -3.01) < 0.01);
    CHECK(std::fabs(m.momentaryLufs() - -3.01) < 0.01);
  }
}

TEST_CASE("stereo -23 dBFS 1 kHz reads -23 LUFS on every window",
          "[d31lufs]") {
  const auto m = meter(48000.0, program(48000.0, {{20.0, -23.0, -23.0}}));
  CHECK(std::fabs(m.integratedLufs() - -23.0) < 0.05);
  CHECK(std::fabs(m.momentaryLufs() - -23.0) < 0.05);
  CHECK(std::fabs(m.shortTermLufs() - -23.0) < 0.05);
  CHECK(m.loudnessRangeLu() < 0.05);  // steady: no range
  CHECK(m.blocks() == 197);           // (200 sub-blocks) - 3
}

TEST_CASE("gates: relative removes quiet passages, absolute removes silence",
          "[d31lufs]") {
  // -36 / -23 / -36: the -36 blocks are 13 LU down, below the -10 LU gate.
  const auto a = meter(48000.0, program(48000.0, {{10.0, -36.0, -36.0},
                                                  {60.0, -23.0, -23.0},
                                                  {10.0, -36.0, -36.0}}));
  CHECK(std::fabs(a.integratedLufs() - -23.0) < 0.1);
  // Without gating the -36 dB parts would pull the mean down: check that a
  // programme only 5 LU apart (inside the gate) is NOT removed.
  const auto b = meter(48000.0, program(48000.0, {{30.0, -28.0, -28.0},
                                                  {30.0, -23.0, -23.0}}));
  const double ungated =
      LoudnessMeter::lufsOf(0.5 * (std::pow(10.0, (-28.0 + 0.691 - 3.01) / 10.0) +
                                   std::pow(10.0, (-23.0 + 0.691 - 3.01) / 10.0)) *
                            2.0);
  CHECK(std::fabs(b.integratedLufs() - ungated) < 0.1);
  // Everything below -70 LUFS: no reading at all.
  const auto c = meter(48000.0, program(48000.0, {{5.0, -75.0, -75.0}}));
  CHECK(c.integratedLufs() == kLoudnessFloor);
  // Silence before the programme does not dilute it.
  const auto d = meter(48000.0, program(48000.0, {{10.0, -400.0, -400.0},
                                                  {20.0, -23.0, -23.0}}));
  CHECK(std::fabs(d.integratedLufs() - -23.0) < 0.05);
}

TEST_CASE("loudness range of a two-level programme", "[d31lufs]") {
  const auto m = meter(48000.0, program(48000.0, {{20.0, -20.0, -20.0},
                                                  {20.0, -30.0, -30.0}}));
  CHECK(std::fabs(m.loudnessRangeLu() - 10.0) < 0.2);
}

TEST_CASE("true peak finds inter-sample peaks", "[d31lufs]") {
  // fs/4 sine, 45 degrees off: every sample is A/sqrt(2), the peak is A.
  const double fs = 48000.0, amp = 0.5;
  std::vector<double> x(48000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = amp * std::sin(2.0 * std::numbers::pi * 0.25 * static_cast<double>(i) +
                          std::numbers::pi / 4.0);
  auto tp = unwrap(TruePeakDetector::create(1));
  tp.push(x.data(), x.size());
  const double peakDb = 20.0 * std::log10(amp);
  CHECK(std::fabs(tp.samplePeakDb() - (peakDb - 3.0103)) < 0.01);
  CHECK(std::fabs(tp.truePeakDb() - peakDb) < 0.17);
  // A low-frequency sine: true and sample peak agree with the amplitude.
  auto tp2 = unwrap(TruePeakDetector::create(1));
  std::vector<double> y(48000);
  for (std::size_t i = 0; i < y.size(); ++i)
    y[i] = amp * std::sin(2.0 * std::numbers::pi * 997.0 * static_cast<double>(i) / fs);
  tp2.push(y.data(), y.size());
  CHECK(std::fabs(tp2.truePeakDb() - peakDb) < 0.02);
  CHECK(tp2.truePeakDb() >= tp2.samplePeakDb());
}

TEST_CASE("true-peak error bounds over frequency and phase", "[d31lufs]") {
  // Unit sines; the stated bounds: over-read <= 0.02 dB everywhere,
  // under-read <= 0.17 dB up to fs/4 and <= 0.5 dB up to 0.45 fs.
  for (const double f : {0.02, 0.1, 0.2, 0.25, 0.3, 0.4, 0.45}) {
    double lo = 0.0, hi = -99.0;
    for (int k = 0; k < 32; ++k) {
      const double phi = 2.0 * std::numbers::pi * k / 32.0;
      std::vector<double> x(600);
      for (std::size_t i = 0; i < x.size(); ++i)
        x[i] = std::sin(2.0 * std::numbers::pi * f * static_cast<double>(i) + phi);
      auto tp = unwrap(TruePeakDetector::create(1));
      tp.push(x.data(), 100);  // past the onset transient
      tp.clearPeaks();
      tp.push(x.data() + 100, x.size() - 100);
      lo = std::min(lo, tp.truePeakDb());
      hi = std::max(hi, tp.truePeakDb());
    }
    CHECK(hi <= 0.02);  // measured <= 0.0150 dB
    CHECK(lo >= (f <= 0.25 ? -0.17 : -0.5));
  }
  // A full-scale sine switched on from silence overshoots on reconstruction:
  // reported, not hidden (steady-state bounds exclude the onset).
  std::vector<double> x(600);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = std::cos(2.0 * std::numbers::pi * 0.02 * static_cast<double>(i));  // starts at its crest
  auto tp = unwrap(TruePeakDetector::create(1));
  tp.push(x.data(), x.size());
  CHECK(tp.truePeakDb() > 0.1);
}

TEST_CASE("loudness meter refusals", "[d31lufs]") {
  CHECK(LoudnessMeter::create(SampleRateHz{48000.0}, 6).error() ==
        QuantityError::InvalidArgument);
  CHECK(LoudnessMeter::create(SampleRateHz{4000.0}, 2).error() ==
        QuantityError::InvalidArgument);
  CHECK(TruePeakDetector::create(0).error() == QuantityError::InvalidArgument);
  auto m = unwrap(LoudnessMeter::create(SampleRateHz{48000.0}, 1));
  CHECK(m.momentaryLufs() == kLoudnessFloor);  // nothing pushed yet
  CHECK(m.integratedLufs() == kLoudnessFloor);
}
