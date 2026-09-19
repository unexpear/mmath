// D24 — compressor and limiter (Catch2).
//
// Compressor: the static curve is continuous with slopes 1 and 1/R, the
// knee formula is exact at its edges; a DC step's gain reduction follows
// the exact one-pole attack and release (1 - 1/e at tau); steady state lands
// on the static curve plus makeup; stereo linking gives equal gains.
// Limiter: the ceiling holds on every sample of hostile input (impulses,
// +12 dBFS sines, square waves); below the ceiling it is an exact delay of
// its declared latency; attack ramps are linear over the look-ahead; linked
// stereo shares one gain; release recovers.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/dsp/dynamics.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};
FrameCount frames(std::size_t n) {
  return FrameCount{static_cast<std::uint64_t>(n)};
}
double db(double a) { return 20.0 * std::log10(a); }
}  // namespace

TEST_CASE("compressor static curve: knee, slopes, continuity", "[d24dyn]") {
  CompressorParams p;
  p.thresholdDb = -24.0;
  p.ratio = 4.0;
  p.kneeDb = 8.0;
  // Below the knee: identity. Above: T + (x - T)/R.
  CHECK(compressorCurveDb(p, -40.0) == -40.0);
  CHECK(std::fabs(compressorCurveDb(p, -8.0) - (-24.0 + 16.0 / 4.0)) < 1e-12);
  // Knee edges (x = T -/+ W/2) meet the straight parts exactly.
  CHECK(std::fabs(compressorCurveDb(p, -28.0) - (-28.0)) < 1e-12);
  CHECK(std::fabs(compressorCurveDb(p, -20.0) - (-24.0 + 4.0 / 4.0)) < 1e-12);
  // Slope continuity by finite differences either side of each edge.
  const double h = 1e-6;
  for (const double e : {-28.0, -20.0}) {
    const double left = (compressorCurveDb(p, e) - compressorCurveDb(p, e - h)) / h;
    const double right = (compressorCurveDb(p, e + h) - compressorCurveDb(p, e)) / h;
    CHECK(std::fabs(left - right) < 1e-5);
  }
  // Monotone non-decreasing everywhere.
  for (double x = -80.0; x < 0.0; x += 0.25)
    CHECK(compressorCurveDb(p, x + 0.25) >= compressorCurveDb(p, x));
  // Hard knee and auto makeup (0 dBFS back to 0 dBFS).
  p.kneeDb = 0.0;
  CHECK(std::fabs(compressorCurveDb(p, -24.0) - (-24.0)) < 1e-12);
  CHECK(std::fabs(autoMakeupDb(p) - 18.0) < 1e-12);  // -(-24 + 24/4)
}

TEST_CASE("compressor attack/release are exact one-pole steps", "[d24dyn]") {
  CompressorParams p;
  p.thresholdDb = -30.0;
  p.ratio = 5.0;
  p.kneeDb = 0.0;
  p.attackMs = 5.0;
  p.releaseMs = 50.0;
  auto c = unwrap(Compressor::create(p, kRate));
  // DC at -10 dBFS: gc = (T + (x-T)/R) - x = -16 dB.
  const double level = std::pow(10.0, -10.0 / 20.0);
  const double gc = -16.0;
  const double aA = std::exp(-1.0 / (0.005 * 48000.0));
  std::vector<double> in(20000, level), out(20000);
  c.processBlock(in.data(), out.data(), frames(in.size()));
  for (const std::size_t n : {1u, 10u, 240u, 1999u, 19999u}) {
    // After n+1 samples: gs = gc (1 - aA^(n+1)).
    const double expect = gc * (1.0 - std::pow(aA, static_cast<double>(n + 1)));
    CHECK(std::fabs(db(out[n] / level) - expect) < 1e-9);
  }
  // At tau (240 samples) the reduction is 1 - 1/e of its final value.
  CHECK(std::fabs(db(out[239] / level) / gc - (1.0 - std::exp(-1.0))) < 1e-9);
  // Steady state lands on the static curve.
  CHECK(std::fabs(db(out.back()) - compressorCurveDb(p, -10.0)) < 1e-6);
  // Release: drop to -40 dBFS (below T, gc = 0): decays by aR per sample.
  const double g0 = c.gainReductionDb();
  const double aR = std::exp(-1.0 / (0.05 * 48000.0));
  std::vector<double> quiet(2400, 0.01), q(2400);
  c.processBlock(quiet.data(), q.data(), frames(quiet.size()));
  CHECK(std::fabs(c.gainReductionDb() - g0 * std::pow(aR, 2400.0)) < 1e-9);
  CHECK(std::fabs(c.gainReductionDb() / g0 - std::exp(-1.0)) < 1e-3);
}

TEST_CASE("compressor: makeup, RMS detector, linked stereo", "[d24dyn]") {
  CompressorParams p;
  p.thresholdDb = -20.0;
  p.ratio = 3.0;
  p.kneeDb = 6.0;
  p.makeupDb = 4.0;
  p.detector = LevelDetector::Rms;
  p.rmsMs = 5.0;
  auto c = unwrap(Compressor::create(p, kRate));
  // A long DC input: RMS level converges to the DC level, so the output
  // level converges to curve(x) + makeup.
  const double x = std::pow(10.0, -6.0 / 20.0);
  std::vector<double> in(48000, x), out(48000);
  c.processBlock(in.data(), out.data(), frames(in.size()));
  CHECK(std::fabs(db(out.back()) - (compressorCurveDb(p, -6.0) + 4.0)) < 1e-6);

  // Linked stereo: same gain on both channels, driven by the louder one.
  p.detector = LevelDetector::Peak;
  auto s = unwrap(Compressor::create(p, kRate));
  std::vector<double> l(4000), r(4000), ol(4000), orr(4000);
  for (std::size_t i = 0; i < l.size(); ++i) {
    l[i] = 0.9 * std::sin(0.01 * static_cast<double>(i));
    r[i] = 0.05 * std::sin(0.07 * static_cast<double>(i) + 1.0);
  }
  s.processStereo(l.data(), r.data(), ol.data(), orr.data(), frames(4000));
  for (std::size_t i = 100; i < l.size(); i += 97)
    if (std::fabs(l[i]) > 1e-3 && std::fabs(r[i]) > 1e-3)
      CHECK(std::fabs(ol[i] / l[i] - orr[i] / r[i]) < 1e-12);
  CHECK(s.gainReductionDb() < -1.0);  // the loud channel drove it
}

TEST_CASE("limiter: the ceiling holds on every sample", "[d24dyn]") {
  for (const double ceilingDb : {-0.1, -1.0, -6.0}) {
    for (const double lookMs : {0.5, 5.0}) {
      LimiterParams p{ceilingDb, lookMs, 60.0};
      auto lim = unwrap(Limiter::create(p, kRate));
      const double c = std::pow(10.0, ceilingDb / 20.0);
      std::vector<double> x(48000), y(48000);
      for (std::size_t i = 0; i < x.size(); ++i) {
        const double t = static_cast<double>(i) / 48000.0;
        double v = 4.0 * std::sin(2.0 * std::numbers::pi * 220.0 * t);  // +12 dB
        if (i >= 12000 && i < 24000)                                   // square
          v = (i / 50) % 2 == 0 ? 1.0 : -1.0;
        if (i % 4801 == 0) v = 10.0;  // +20 dB impulses
        if (i >= 30000 && i < 36000) v *= 0.1;
        x[i] = v;
      }
      lim.processBlock(x.data(), y.data(), frames(x.size()));
      double peak = 0.0;
      for (double v : y) peak = std::max(peak, std::fabs(v));
      CHECK(peak <= c * (1.0 + 8.0 * std::numeric_limits<double>::epsilon()));
      CHECK(peak > 0.99 * c);  // and it is actually used
    }
  }
}

TEST_CASE("limiter: exact delay below the ceiling, linear attack ramps",
          "[d24dyn]") {
  LimiterParams p{-1.0, 2.0, 50.0};
  auto lim = unwrap(Limiter::create(p, kRate));
  const std::size_t L = lim.latencyFrames();
  CHECK(L == 96);  // 2 ms at 48 kHz
  std::vector<double> x(5000), y(5000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = 0.8 * std::sin(0.013 * static_cast<double>(i));  // < -1 dBFS
  lim.processBlock(x.data(), y.data(), frames(x.size()));
  for (std::size_t i = 0; i < L; ++i) CHECK(y[i] == 0.0);
  for (std::size_t i = L; i < y.size(); ++i) CHECK(y[i] == x[i - L]);
  CHECK(lim.gainReductionDb() == 0.0);

  // One impulse into silence-level DC: the gain ramps down linearly over
  // L+1 samples before the impulse leaves the delay, never stepping.
  lim.reset();
  const double c = std::pow(10.0, -1.0 / 20.0);
  std::vector<double> d(1000, 0.1), o(1000);
  d[500] = 2.0;
  lim.processBlock(d.data(), o.data(), frames(d.size()));
  CHECK(std::fabs(o[500 + L]) <= c * (1.0 + 1e-15));
  const double need = c / 2.0;
  const double maxStep = (1.0 - need) / static_cast<double>(L + 1);
  for (std::size_t i = 500; i <= 500 + L; ++i) {
    const double g1 = o[i] / d[i - L], g0 = o[i - 1] / d[i - 1 - L];
    CHECK(g0 - g1 <= maxStep * (1.0 + 1e-9) + 1e-15);
  }
}

TEST_CASE("limiter: linked stereo and release recovery", "[d24dyn]") {
  LimiterParams p{-3.0, 3.0, 20.0};
  auto lim = unwrap(Limiter::create(p, kRate));
  std::vector<double> l(20000), r(20000), ol(20000), orr(20000);
  for (std::size_t i = 0; i < l.size(); ++i) {
    const double burst = i < 5000 ? 3.0 : 0.2;
    l[i] = burst * std::sin(0.02 * static_cast<double>(i));
    r[i] = 0.3 * std::cos(0.05 * static_cast<double>(i));
  }
  lim.processStereo(l.data(), r.data(), ol.data(), orr.data(), frames(20000));
  const std::size_t L = lim.latencyFrames();
  for (std::size_t i = L + 10; i < 20000; i += 37)
    if (std::fabs(l[i - L]) > 1e-3 && std::fabs(r[i - L]) > 1e-3)
      CHECK(std::fabs(ol[i] / l[i - L] - orr[i] / r[i - L]) < 1e-12);
  // 15000 samples (~16 release time constants) after the burst: recovered.
  CHECK(lim.gainReductionDb() > -0.01);
}

TEST_CASE("dynamics reject invalid parameters", "[d24dyn]") {
  CompressorParams p;
  p.ratio = 0.5;
  CHECK(Compressor::create(p, kRate).error() == QuantityError::InvalidArgument);
  p = CompressorParams{};
  p.attackMs = 0.0;
  CHECK(Compressor::create(p, kRate).error() == QuantityError::InvalidArgument);
  auto c = unwrap(Compressor::create(CompressorParams{}, kRate));
  CompressorParams bad;
  bad.thresholdDb = 3.0;
  CHECK(c.setParams(bad).error() == QuantityError::InvalidArgument);
  CHECK(c.params().thresholdDb == -20.0);  // unchanged
  CHECK(Limiter::create(LimiterParams{1.0, 5.0, 50.0}, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(Limiter::create(LimiterParams{-1.0, 0.0, 50.0}, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(Limiter::create(LimiterParams{-1.0, 5.0, -1.0}, kRate).error() ==
        QuantityError::InvalidArgument);
}

namespace {
// Independent reference for true peak: 16x band-limited interpolation with
// a 128-tap Kaiser(8) windowed sinc (not the limiter's own 4x filter).
double refTruePeakDb(const std::vector<double>& y, std::size_t from) {
  constexpr int half = 64;
  auto i0 = [](double x) {
    double s = 1.0, t = 1.0;
    for (int k = 1; k < 60; ++k) {
      t *= (x / (2.0 * k)) * (x / (2.0 * k));
      s += t;
    }
    return s;
  };
  double pk = 0.0;
  for (std::size_t n = from; n + half < y.size(); ++n)
    for (int f = 0; f < 16; ++f) {
      const double frac = f / 16.0;
      double v = 0.0;
      for (int k = -half + 1; k <= half; ++k) {
        const double t = k - frac;
        const double s = t == 0.0 ? 1.0
                                  : std::sin(std::numbers::pi * t) /
                                        (std::numbers::pi * t);
        const double r = t / half;
        const double w =
            std::fabs(r) >= 1.0 ? 0.0 : i0(8.0 * std::sqrt(1.0 - r * r)) / i0(8.0);
        v += y[n + static_cast<std::size_t>(k + half) - half] * s * w;
      }
      pk = std::max(pk, std::fabs(v));
    }
  return 20.0 * std::log10(pk);
}
}  // namespace

TEST_CASE("true-peak limiter: inter-sample peaks held near the ceiling",
          "[d24dyn]") {
  // Measured against the 16x reference (ceiling -1 dB): plain mode lets
  // +2.0 dBTP through on a fs/4 sine; true-peak mode keeps it within the
  // 4x detector's own under-read (0.16 dB measured), sample peaks stay
  // guaranteed, and latency grows by exactly kTruePeakExtra.
  const double c = -1.0;
  for (int sig = 0; sig < 2; ++sig) {
    std::vector<double> x(6000);
    for (std::size_t i = 0; i < x.size(); ++i) {
      const double n = static_cast<double>(i);
      x[i] = sig == 0 ? 2.0 * std::sin(2.0 * std::numbers::pi * 0.25 * n +
                                       std::numbers::pi / 4.0)
                      : ((i / 20) % 2 ? 2.0 : -2.0);  // 1.2 kHz square
    }
    double plain = 0.0, tp = 0.0;
    for (const bool mode : {false, true}) {
      auto lim = unwrap(Limiter::create(LimiterParams{c, 5.0, 50.0, mode}, kRate));
      CHECK(lim.latencyFrames() == 240 + (mode ? Limiter::kTruePeakExtra : 0));
      std::vector<double> y(x.size());
      lim.processBlock(x.data(), y.data(), frames(x.size()));
      double sp = 0.0;
      for (std::size_t i = 1000; i < y.size(); ++i) sp = std::max(sp, std::fabs(y[i]));
      CHECK(sp <= std::pow(10.0, c / 20.0) * (1.0 + 1e-12));  // guaranteed
      (mode ? tp : plain) = refTruePeakDb(y, 1000);
    }
    CHECK(plain > c + 1.0);  // the problem: >1 dB over (2.2-3.0 measured)
    CHECK(tp < c + 0.2);     // the fix: within the detector's bound
  }
  // setParams keeps the mode (it fixes the latency).
  auto lim = unwrap(Limiter::create(LimiterParams{c, 5.0, 50.0, true}, kRate));
  const auto q = unwrap(lim.setParams(LimiterParams{-3.0, 1.0, 20.0, false}));
  CHECK(q.truePeak);
  CHECK(lim.latencyFrames() == 248);
}
