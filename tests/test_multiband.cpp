// D37 — multiband compressor, dynamic EQ, linear-phase EQ, mid/side.
//
// Multiband with neutral compressors equals the all-pass chain
// AP_high(AP_low(x)) to roundoff (flat magnitude); a loud low band is
// compressed while mid and high stay untouched. Dynamic EQ is transparent
// to roundoff below threshold, cuts its band by the gain computer's amount
// above it, and leaves other frequencies alone. The linear-phase FIR is
// exactly symmetric, matches the cascade magnitude within 0.1 dB, and the
// FFT convolver equals direct convolution. Mid/side round-trips and
// side-only processing leaves mono content untouched.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/dsp/filters.h>
#include <mmathen/dsp/linear_phase.h>
#include <mmathen/dsp/multiband.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};
FrameCount fc(std::size_t n) { return FrameCount{static_cast<std::uint64_t>(n)}; }

std::vector<double> sine(double f, double a, std::size_t n) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = a * std::sin(2.0 * std::numbers::pi * f * static_cast<double>(i) / 48000.0);
  return x;
}
double amp(const std::vector<double>& y, double f) {
  std::vector<double> seg(y.end() - 16384, y.end());
  return unwrap(estimateSinusoid(seg, kRate, Window::Hann, FrequencyHz{f * 0.9},
                                 FrequencyHz{f * 1.1})).amplitude;
}
}  // namespace

TEST_CASE("multiband with neutral compressors is an exact all-pass", "[d37mb]") {
  MultibandParams p;
  for (auto& b : p.band) b.ratio = 1.0, b.makeupDb = 0.0;
  auto mb = unwrap(MultibandCompressor::create(p, kRate));
  std::vector<double> x(20000), y(20000), yr(20000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = std::sin(0.013 * static_cast<double>(i)) + 0.5 * std::sin(0.4 * static_cast<double>(i)) +
           (i == 3000 ? 1.0 : 0.0);
  mb.processStereo(x.data(), x.data(), y.data(), yr.data(), fc(x.size()));
  // Reference: the all-pass chain built from the same LR4 sections.
  auto apply = [&](const std::vector<BiquadCoeffs>& s, std::vector<double> v) {
    for (const auto& k : s) {
      Biquad b{k};
      b.processBlock(v.data(), v.data(), fc(v.size()));
    }
    return v;
  };
  auto lrs = [&](bool hp, double f) { return unwrap(designLinkwitzRiley(hp, 4, FrequencyHz{f}, kRate)); };
  auto ap = [&](double f, const std::vector<double>& v) {
    auto a = apply(lrs(false, f), v), b = apply(lrs(true, f), v);
    for (std::size_t i = 0; i < a.size(); ++i) a[i] += b[i];
    return a;
  };
  const auto ref = ap(2500.0, ap(200.0, x));
  double worst = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i) worst = std::max(worst, std::fabs(y[i] - ref[i]));
  CHECK(worst < 1e-12);
  // And its magnitude is flat: a steady sine keeps its amplitude.
  const auto s = sine(1000.0, 0.5, 48000);
  std::vector<double> o(s.size()), orr(s.size());
  auto mb2 = unwrap(MultibandCompressor::create(p, kRate));
  mb2.processStereo(s.data(), s.data(), o.data(), orr.data(), fc(s.size()));
  CHECK(std::fabs(amp(o, 1000.0) / 0.5 - 1.0) < 1e-9);
}

TEST_CASE("multiband compresses only the loud band", "[d37mb]") {
  MultibandParams p;
  for (auto& b : p.band) b.thresholdDb = -20.0, b.ratio = 8.0, b.kneeDb = 0.0;
  auto mb = unwrap(MultibandCompressor::create(p, kRate));
  const std::size_t n = 96000;
  auto lo = sine(80.0, 0.8, n), hi = sine(6000.0, 0.02, n);  // loud low, quiet high
  std::vector<double> x(n), y(n), yr(n);
  for (std::size_t i = 0; i < n; ++i) x[i] = lo[i] + hi[i];
  mb.processStereo(x.data(), x.data(), y.data(), yr.data(), fc(n));
  CHECK(mb.gainReductionDb(0) < -5.0);   // low band compressed
  CHECK(mb.gainReductionDb(2) == 0.0);   // quiet high band untouched
  CHECK(std::fabs(20.0 * std::log10(amp(y, 6000.0) / 0.02)) < 0.05);
  CHECK(amp(y, 80.0) < 0.8 * std::pow(10.0, -5.0 / 20.0));
}

TEST_CASE("dynamic EQ: transparent below, cuts its band above", "[d37mb]") {
  DynamicEqBand b;
  b.frequency = FrequencyHz{6500.0};
  b.q = 2.0;
  b.thresholdDb = -30.0;
  b.ratio = 4.0;
  b.kneeDb = 0.0;
  b.rangeDb = 18.0;
  // Quiet: below threshold everywhere -> identity to roundoff (a 0 dB RBJ
  // peaking section is b = a; the direct-form sums still round).
  auto d = unwrap(DynamicEq::create(b, kRate));
  const auto q = sine(6500.0, 0.01, 24000);  // -40 dBFS
  std::vector<double> y(q.size()), yr(q.size());
  d.processStereo(q.data(), q.data(), y.data(), yr.data(), fc(q.size()));
  double worst = 0.0;
  for (std::size_t i = 0; i < q.size(); ++i) worst = std::max(worst, std::fabs(y[i] - q[i]));
  CHECK(worst < 1e-15);
  CHECK(d.dynamicGainDb() == 0.0);
  // Loud sibilance at the band: steady state cut = curve at its level.
  auto d2 = unwrap(DynamicEq::create(b, kRate));
  const double a = std::pow(10.0, -10.0 / 20.0);  // -10 dBFS peak
  const auto s = sine(6500.0, a, 96000), l = sine(300.0, 0.3, 96000);
  std::vector<double> x(s.size()), o(s.size()), orr(s.size());
  for (std::size_t i = 0; i < x.size(); ++i) x[i] = s[i] + l[i];
  d2.processStereo(x.data(), x.data(), o.data(), orr.data(), fc(x.size()));
  // Gain computer at -10 dB, T -30, R 4: cut 15 dB (inside the 18 dB range).
  CHECK(std::fabs(d2.dynamicGainDb() - -15.0) < 0.5);
  CHECK(std::fabs(20.0 * std::log10(amp(o, 6500.0) / a) - -15.0) < 0.6);
  CHECK(std::fabs(20.0 * std::log10(amp(o, 300.0) / 0.3)) < 0.1);  // lows kept
}

TEST_CASE("linear-phase FIR: symmetric, accurate, convolver exact", "[d37mb]") {
  const std::vector<BiquadCoeffs> eq = {
      unwrap(makeLowShelf(FrequencyHz{120.0}, 6.0, 0.7071, kRate)),
      unwrap(makePeaking(FrequencyHz{2000.0}, -5.0, 1.4, kRate)),
      unwrap(makeHighShelf(FrequencyHz{9000.0}, 4.0, 0.7071, kRate))};
  const std::size_t taps = 4097;
  const auto h = unwrap(linearPhaseFromSections(eq, kRate, taps));
  for (std::size_t i = 0; i < taps / 2; ++i) REQUIRE(h[i] == h[taps - 1 - i]);  // exact symmetry
  // Magnitude via the DTFT of h at test frequencies vs the cascade.
  double worst = 0.0;
  for (const double f : {60.0, 120.0, 500.0, 2000.0, 5000.0, 9000.0, 15000.0}) {
    std::complex<double> s{0.0, 0.0};
    for (std::size_t i = 0; i < taps; ++i)
      s += h[i] * std::polar(1.0, -2.0 * std::numbers::pi * f * static_cast<double>(i) / 48000.0);
    const double got = 20.0 * std::log10(std::abs(s));
    const double want = unwrap(cascadeResponseDb(eq, FrequencyHz{f}, kRate));
    worst = std::max(worst, std::fabs(got - want));
  }
  CHECK(worst < 0.1);
  // FFT convolver equals direct convolution (short FIR for speed).
  std::vector<double> g(129);
  for (std::size_t i = 0; i < g.size(); ++i) g[i] = std::sin(0.3 * static_cast<double>(i)) / (1.0 + i);
  auto conv = unwrap(FftConvolver::create(g, 256));
  std::vector<double> x(3000), y(3000);
  for (std::size_t i = 0; i < x.size(); ++i) x[i] = std::cos(0.07 * static_cast<double>(i)) * (i % 7 == 0 ? 2.0 : 1.0);
  conv.process(x.data(), y.data(), x.size());
  double worstC = 0.0;
  const std::size_t L = conv.latencyFrames();
  for (std::size_t i = L; i < x.size(); ++i) {
    double d = 0.0;
    for (std::size_t k = 0; k < g.size() && k <= i - L; ++k) d += g[k] * x[i - L - k];
    worstC = std::max(worstC, std::fabs(y[i] - d));
  }
  CHECK(worstC < 1e-12);
  CHECK(FftConvolver::create(std::vector<double>(300), 256).error() == QuantityError::InvalidArgument);
  CHECK(designLinearPhaseFir([](double) { return 0.0; }, kRate, 100).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("mid/side round trip; side-only processing keeps mono", "[d37mb]") {
  std::vector<double> l(1000), r(1000), m(1000), s(1000), l2(1000), r2(1000);
  for (std::size_t i = 0; i < l.size(); ++i) {
    l[i] = std::sin(0.01 * static_cast<double>(i));
    r[i] = 0.3 * std::cos(0.07 * static_cast<double>(i));
  }
  midSideEncode(l.data(), r.data(), m.data(), s.data(), l.size());
  midSideDecode(m.data(), s.data(), l2.data(), r2.data(), l.size());
  for (std::size_t i = 0; i < l.size(); ++i) {
    CHECK(std::fabs(l2[i] - l[i]) < 1e-15);
    CHECK(std::fabs(r2[i] - r[i]) < 1e-15);
  }
  // Mono content (L = R): the side channel is exactly zero, so any side
  // processing (here x3 gain) cannot change it.
  midSideEncode(l.data(), l.data(), m.data(), s.data(), l.size());
  for (auto& v : s) v *= 3.0;
  midSideDecode(m.data(), s.data(), l2.data(), r2.data(), l.size());
  CHECK(l2 == l);
  CHECK(r2 == l);
}
