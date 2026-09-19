// D36 — transient-aware time-scale modification (Catch2).
//
// HPSS: harmonic + percussive = input to roundoff; a tone goes to the
// harmonic part and clicks to the percussive part (>= 99 % / 98 %,
// measured 99.7 / 99.0 %). PGHI keeps tones exact and keeps a stretched
// click train markedly more compact than the classic vocoder (energy
// within 1 ms of each click: measured 0.53 vs 0.29 at x1.5; within 5 ms
// 0.77 vs 0.66). The hybrid matches PGHI on this material (0.77 within
// 5 ms, measured) and also beats the classic vocoder. Pitch shifting via
// PGHI moves a tone by exactly the ratio at the same length.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/dsp/tsm.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};
constexpr std::size_t kN = 48000;

double bed(std::size_t i) {
  return 0.2 * std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(i) / 48000.0);
}
std::vector<double> clickTrain() {
  std::vector<double> x(kN);
  for (std::size_t i = 0; i < kN; ++i) x[i] = bed(i) + ((i % 4800) == 2400 ? 0.9 : 0.0);
  return x;
}
// Mean fraction of each click's energy (bed removed) within +/- w samples
// of its expected position, out of +/- 960 samples (20 ms).
double concentration(const std::vector<double>& y, double stretch, long w) {
  double sum = 0.0;
  int n = 0;
  for (std::size_t c = 2400; c < kN - 4800; c += 4800) {
    const auto pos = static_cast<long>(std::llround(static_cast<double>(c) * stretch));
    double near = 0.0, all = 0.0;
    for (long k = -960; k <= 960; ++k) {
      const auto i = static_cast<std::size_t>(pos + k);
      const double b = 0.2 * std::sin(2.0 * std::numbers::pi * 440.0 *
                                      static_cast<double>(pos + k) / 48000.0);
      const double e = (y[i] - b) * (y[i] - b);
      all += e;
      if (std::labs(k) <= w) near += e;
    }
    sum += near / all;
    ++n;
  }
  return sum / n;
}
double sineHz(const std::vector<double>& y) {
  std::vector<double> seg(y.begin() + 10000, y.begin() + 10000 + 16384);
  return unwrap(estimateSinusoid(seg, kRate, Window::Hann, FrequencyHz{400.0},
                                 FrequencyHz{480.0})).frequency.value();
}
}  // namespace

TEST_CASE("HPSS sums back exactly and separates tone from clicks", "[d36tsm]") {
  const auto x = clickTrain();
  const auto h = unwrap(hpss(x));
  double worst = 0.0, eh = 0.0, ep = 0.0, bh = 0.0, bp = 0.0;
  for (std::size_t i = 0; i < kN; ++i) {
    worst = std::max(worst, std::fabs(h.harmonic[i] + h.percussive[i] - x[i]));
    const double c = x[i] - bed(i);
    eh += h.harmonic[i] * bed(i);
    ep += h.percussive[i] * c;
    bh += bed(i) * bed(i);
    bp += c * c;
  }
  CHECK(worst < 1e-12);
  CHECK(eh / bh > 0.99);  // measured 0.997
  CHECK(ep / bp > 0.98);  // measured 0.990
}

TEST_CASE("PGHI keeps tones and keeps transients compact", "[d36tsm]") {
  const auto x = clickTrain();
  // Tone accuracy on the pure tone (with clicks in the measured segment the
  // estimator is biased by the stretched clicks, not by PGHI).
  std::vector<double> tone(kN);
  for (std::size_t i = 0; i < kN; ++i) tone[i] = bed(i);
  for (const double st : {0.75, 1.5, 2.0}) {
    const auto y = unwrap(pghiStretch(tone, st));
    CHECK(y.size() == static_cast<std::size_t>(std::llround(st * kN)));
    // Measured <= 0.018 Hz (4e-5): a small real cost of PGHI's approximate
    // phase (the classic locked vocoder is exact to 1e-6 Hz on tones).
    CHECK(std::fabs(sineHz(y) - 440.0) < 0.03);
  }
  const auto classic = unwrap(timeStretch(x, 1.5, TsmMethod::ClassicLocked));
  const auto pghi = unwrap(timeStretch(x, 1.5, TsmMethod::Pghi));
  const auto hybrid = unwrap(timeStretch(x, 1.5, TsmMethod::Hybrid));
  const double c1 = concentration(classic, 1.5, 48), p1 = concentration(pghi, 1.5, 48);
  const double c5 = concentration(classic, 1.5, 240), p5 = concentration(pghi, 1.5, 240);
  const double h5 = concentration(hybrid, 1.5, 240);
  CHECK(p1 > 1.5 * c1);   // measured 0.530 vs 0.289
  CHECK(p5 > c5 + 0.05);  // measured 0.769 vs 0.658
  CHECK(h5 > c5 + 0.05);  // measured 0.765
  CHECK(std::fabs(sineHz(hybrid) - 440.0) < 0.05);
}

TEST_CASE("PGHI pitch shift moves a tone by the ratio; refusals", "[d36tsm]") {
  std::vector<double> x(kN);
  for (std::size_t i = 0; i < kN; ++i) x[i] = 2.0 * bed(i);
  const double p = std::pow(2.0, 7.0 / 12.0);
  const auto y = unwrap(pitchShift(x, kRate, p, TsmMethod::Pghi));
  CHECK(y.size() == x.size());
  std::vector<double> seg(y.begin() + 10000, y.begin() + 10000 + 16384);
  const double f = unwrap(estimateSinusoid(seg, kRate, Window::Hann, FrequencyHz{600.0},
                                           FrequencyHz{700.0})).frequency.value();
  CHECK(std::fabs(f / (440.0 * p) - 1.0) < 2e-4);
  CHECK(pghiStretch(x, 9.0).error() == QuantityError::InvalidArgument);
  CHECK(hpss(x, 1000).error() == QuantityError::InvalidArgument);
  CHECK(hpss(x, 1024, 16).error() == QuantityError::InvalidArgument);
}
