// D26 — smooth cuts: crossfades, gain ramps, SpliceReader, overlap-add.
//
// Curves meet their sum/power identities exactly; a crossfaded jump between
// opposite-phase positions of a sine moves by at most the sine's own slope
// plus the fade's, where a hard cut jumps by ~2; loops seam without a step
// and play the source exactly outside the seams; a non-looping source ends;
// the overlap-add processor is an exact N-sample delay with an identity
// callback (hop N/2 and N/4) and applies spectral gains exactly.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/dsp/crossfade.h>
#include <mmathen/dsp/overlap_add.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

std::vector<float> sine(std::size_t n, double cyclesPerSample) {
  std::vector<float> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = static_cast<float>(
        std::sin(2.0 * std::numbers::pi * cyclesPerSample *
                 static_cast<double>(i)));
  return x;
}

double maxStep(const std::vector<double>& y) {
  double m = 0.0;
  for (std::size_t i = 1; i < y.size(); ++i)
    m = std::max(m, std::fabs(y[i] - y[i - 1]));
  return m;
}
}  // namespace

TEST_CASE("fade curves meet their identities", "[d26fade]") {
  for (double t = 0.0; t <= 1.0; t += 1.0 / 64.0) {
    CHECK(fadeInGain(FadeCurve::Linear, t) + fadeOutGain(FadeCurve::Linear, t) ==
          1.0);
    CHECK(std::fabs(fadeInGain(FadeCurve::RaisedCosine, t) +
                    fadeOutGain(FadeCurve::RaisedCosine, t) - 1.0) < 1e-15);
    const double gi = fadeInGain(FadeCurve::EqualPower, t);
    const double go = fadeOutGain(FadeCurve::EqualPower, t);
    CHECK(std::fabs(gi * gi + go * go - 1.0) < 1e-15);
  }
  CHECK(fadeInGain(FadeCurve::EqualPower, 0.0) == 0.0);
  CHECK(fadeOutGain(FadeCurve::RaisedCosine, 1.0) < 1e-16);
  // Identical inputs through a constant-sum crossfade stay put.
  std::vector<double> a(100, 0.7), out(100);
  crossfadeBlocks(a.data(), a.data(), out.data(), 100, FadeCurve::Linear);
  for (double v : out) CHECK(std::fabs(v - 0.7) < 1e-15);
}

TEST_CASE("gain ramp reaches its target exactly and linearly", "[d26fade]") {
  GainRamp g(1.0);
  g.setTarget(0.0, 4);
  CHECK(g.next() == 0.75);
  CHECK(g.next() == 0.5);
  CHECK(g.next() == 0.25);
  CHECK(g.next() == 0.0);
  CHECK_FALSE(g.ramping());
  CHECK(g.next() == 0.0);
  g.setTarget(0.3, 0);
  CHECK(g.value() == 0.3);
}

TEST_CASE("crossfaded jumps remove the cut's step", "[d26fade]") {
  const double cps = 440.0 / 48000.0;
  const auto src = sine(48000, cps);
  const double slope = 2.0 * std::numbers::pi * cps;  // max |dx/dn|
  // Jump from a crest (sample 1009, phase 9.25 cycles) to a trough half a
  // period later: a hard cut steps by ~2.
  const std::size_t crest = 1009;
  const std::size_t half = 55;  // 0.504 periods
  for (const std::size_t fade : {std::size_t{1}, std::size_t{480}}) {
    auto r = unwrap(SpliceReader<float>::create(1, fade, FadeCurve::EqualPower));
    r.setSource(src.data(), src.size());
    std::vector<double> y(2000);
    r.read(y.data(), crest + 1);
    r.jumpTo(crest + 1 + half);  // opposite phase
    r.read(y.data() + crest + 1, 2000 - crest - 1);
    const double step = maxStep(y);
    if (fade == 1) {
      // A 1-frame fade is one equal-power midpoint sample: the ~2 cut is
      // split into two steps of ~1, still 15x the crossfaded bound.
      CHECK(step > 0.95);
    } else {
      // Sine slope plus the fade's own change: 2 * (pi/2)/F per sample
      // bounds the gain slopes, times full scale.
      CHECK(step <= slope + 2.0 * (std::numbers::pi / 2.0) / 480.0 + 1e-6);
      // After the fade the output is the source, exactly.
      CHECK(y[1999] == static_cast<double>(src[1999 + half]));
    }
  }
}

TEST_CASE("loops seam with a crossfade and play the source exactly",
          "[d26fade]") {
  // A loop whose ends do not match (non-integer cycles): hard looping steps.
  const double cps = 97.3 / 48000.0;
  const auto src = sine(10000, cps);
  auto r = unwrap(SpliceReader<float>::create(1, 256, FadeCurve::EqualPower));
  r.setSource(src.data(), src.size());
  r.setLooping(true);
  REQUIRE(r.setLoopRegion(1000, 9000).ok());
  r.jumpTo(1000);
  std::vector<double> y(40000);
  CHECK(r.read(y.data(), y.size()) == y.size());  // looping never ends
  const double slope = 2.0 * std::numbers::pi * cps;
  CHECK(maxStep(y) <= slope + 2.0 * (std::numbers::pi / 2.0) / 256.0 + 1e-6);
  // Away from fades the output is a plain copy of the loop: before the
  // first seam (which starts at output 7744 = 8744 - 1000) and after it,
  // where the loop restarts from 1000 + 256.
  CHECK(y[5000] == static_cast<double>(src[6000]));
  CHECK(y[7744 + 256 + 100] == static_cast<double>(src[1000 + 256 + 100]));
  // Region too short for the seam fade is refused.
  CHECK(r.setLoopRegion(0, 300).error() == QuantityError::InvalidArgument);
}

TEST_CASE("non-looping source ends; stereo interleave kept", "[d26fade]") {
  std::vector<double> st(200);
  for (std::size_t i = 0; i < 100; ++i) {
    st[2 * i] = static_cast<double>(i);
    st[2 * i + 1] = -static_cast<double>(i);
  }
  auto r = unwrap(SpliceReader<double>::create(2, 8));
  r.setSource(st.data(), 100);
  std::vector<double> y(2 * 150);
  CHECK(r.read(y.data(), 150) == 100);
  for (std::size_t i = 0; i < 100; ++i) {
    CHECK(y[2 * i] == static_cast<double>(i));
    CHECK(y[2 * i + 1] == -static_cast<double>(i));
  }
  // setPosition is a plain cut (no fade), clamped to the buffer.
  r.setPosition(40);
  CHECK_FALSE(r.fading());
  REQUIRE(r.read(y.data(), 1) == 1);
  CHECK(y[0] == 40.0);
  r.setPosition(1000);
  CHECK(r.position() == 99);
  CHECK(SpliceReader<double>::create(0, 8).error() ==
        QuantityError::InvalidArgument);
  CHECK(SpliceReader<double>::create(1, 0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("overlap-add is an exact delay and applies spectral gains",
          "[d26fade]") {
  for (const int div : {2, 4}) {
    auto ola = unwrap(OverlapAdd::create(512, div));
    const std::size_t L = ola.latencyFrames();
    CHECK(L == 512);
    std::vector<double> x(6000), y(6000);
    for (std::size_t i = 0; i < x.size(); ++i)
      x[i] = std::sin(0.0371 * static_cast<double>(i)) +
             0.3 * std::cos(0.9 * static_cast<double>(i)) +
             (i == 777 ? 1.0 : 0.0);
    // Feed in uneven blocks: block size must not matter.
    std::size_t done = 0;
    for (std::size_t b : {1u, 37u, 500u, 1024u, 4438u}) {
      ola.process(x.data() + done, y.data() + done, b,
                  [](std::vector<std::complex<double>>&) {});
      done += b;
    }
    REQUIRE(done == x.size());
    for (std::size_t i = 0; i < L; ++i) CHECK(std::fabs(y[i]) < 1e-12);
    double worst = 0.0;
    for (std::size_t i = L; i < y.size(); ++i)
      worst = std::max(worst, std::fabs(y[i] - x[i - L]));
    CHECK(worst < 1e-12);

    // A spectral gain of 0.25 on every bin is a 0.25 gain on the output.
    auto g = unwrap(OverlapAdd::create(256, div));
    std::vector<double> z(6000);
    g.process(x.data(), z.data(), x.size(),
              [](std::vector<std::complex<double>>& bins) {
                for (auto& b : bins) b *= 0.25;
              });
    for (std::size_t i = 256; i < z.size(); ++i)
      CHECK(std::fabs(z[i] - 0.25 * x[i - 256]) < 1e-12);
  }
  CHECK(OverlapAdd::create(100).error() == QuantityError::InvalidArgument);
  CHECK(OverlapAdd::create(512, 3).error() == QuantityError::InvalidArgument);
}
