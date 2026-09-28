// D39 — mel scales, filterbanks, DCT and MFCCs (Catch2).
//
// The mel scales hit their published anchors (HTK: 1000 Hz ~ 1000 mel;
// Slaney: 1000 Hz = 15, 6400 Hz = 42 exactly) and invert to roundoff.
// Unnormalised filters are a partition of unity between the first and last
// centres; Slaney-normalised filters have unit area. The DCT is
// orthonormal (inverse and Parseval to roundoff) and equals the textbook
// matrix. MFCCs are checked against a brute-force DFT pipeline written
// independently here; a gain changes c0 alone, by exactly 2 ln(g) sqrt(M);
// deltas of a linear ramp are its slope; a tone peaks in the filter whose
// centre is nearest to it.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

#include <mmathen/analysis/mfcc.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz k16{16000.0};

std::vector<double> tone(double f, double a, std::size_t n, double fs = 16000.0) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = a * std::sin(2.0 * std::numbers::pi * f * static_cast<double>(i) / fs);
  return x;
}
}  // namespace

TEST_CASE("MFCC: ragged frames are rejected before access or mutation", "[d39mfcc][review-regression]") {
  for (const auto& frames : std::vector<std::vector<std::vector<double>>>{
           {{1.0, 2.0}, {3.0}}, {{1.0}, {2.0, 3.0}}, {{}, {1.0}}}) {
    CHECK_THROWS_AS(deltas(frames), std::invalid_argument);
    CHECK_THROWS_AS(deltas(frames, 0), std::invalid_argument);
    auto copy = frames;
    CHECK_THROWS_AS(cepstralMeanNormalize(copy), std::invalid_argument);
    CHECK(copy == frames);
  }
  CHECK(deltas({}).empty());
  std::vector<std::vector<double>> empty;
  CHECK_NOTHROW(cepstralMeanNormalize(empty));
  CHECK(deltas({{}, {}}) == std::vector<std::vector<double>>{{}, {}});
  CHECK(deltas({{2.0, 3.0}}) == std::vector<std::vector<double>>{{0.0, 0.0}});
}

TEST_CASE("MFCC: numeric options reject nonfinite values", "[d39mfcc][review-regression]") {
  const std::vector<double> x(512, 0.1);
  for (double bad : {std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()}) {
    for (auto member : {&MfccOptions::lifter, &MfccOptions::logFloor, &MfccOptions::fMaxHz,
                        &MfccOptions::fMinHz, &MfccOptions::preEmphasis}) {
      MfccOptions o;
      o.*member = bad;
      CHECK(mfcc(x, k16, o).error() == QuantityError::InvalidArgument);
    }
  }
  for (auto member : {&MfccOptions::lifter, &MfccOptions::logFloor, &MfccOptions::fMaxHz}) {
    MfccOptions o;
    o.*member = -1.0;
    CHECK(mfcc(x, k16, o).error() == QuantityError::InvalidArgument);
  }
  MfccOptions o;
  o.lifter = 0.0;
  const auto r = unwrap(mfcc(x, k16, o));
  for (const auto& frame : r.coeffs)
    for (double v : frame) CHECK(std::isfinite(v));
}

TEST_CASE("mel scales: published anchors and exact inverses", "[d39mfcc]") {
  CHECK(std::fabs(hzToMel(1000.0, MelScale::Htk) - 999.98553) < 1e-4);
  CHECK(std::fabs(hzToMel(700.0, MelScale::Htk) - 2595.0 * std::log10(2.0)) < 1e-12);
  CHECK(hzToMel(1000.0, MelScale::Slaney) == 15.0);
  CHECK(std::fabs(hzToMel(6400.0, MelScale::Slaney) - 42.0) < 1e-12);
  CHECK(std::fabs(hzToMel(500.0, MelScale::Slaney) - 7.5) < 1e-12);
  for (const auto s : {MelScale::Htk, MelScale::Slaney})
    for (double f : {0.0, 10.0, 440.0, 999.0, 1000.0, 1001.0, 4000.0, 8000.0, 22050.0}) {
      CHECK(std::fabs(melToHz(hzToMel(f, s), s) - f) < 1e-9 * std::max(1.0, f));
    }
  // Slaney is continuous at the break.
  CHECK(std::fabs(hzToMel(999.999999, MelScale::Slaney) - 15.0) < 1e-6);
}

TEST_CASE("mel filterbank: partition of unity, unit area", "[d39mfcc]") {
  const auto fb = unwrap(melFilterbank(26, 512, k16, 0.0, 8000.0));
  REQUIRE(fb.weights.size() == 26);
  REQUIRE(fb.weights[0].size() == 257);
  for (std::size_t k = 0; k <= 256; ++k) {
    const double f = static_cast<double>(k) * 16000.0 / 512.0;
    if (f < fb.edgesHz[1] || f > fb.edgesHz[26]) continue;
    double s = 0.0;
    for (const auto& w : fb.weights) s += w[k];
    CHECK(std::fabs(s - 1.0) < 1e-12);
  }
  // Each filter peaks at most 1, is zero outside its edges.
  for (std::size_t m = 0; m < 26; ++m)
    for (std::size_t k = 0; k <= 256; ++k) {
      const double f = static_cast<double>(k) * 16000.0 / 512.0;
      CHECK(fb.weights[m][k] <= 1.0);
      if (f <= fb.edgesHz[m] || f >= fb.edgesHz[m + 2]) CHECK(fb.weights[m][k] == 0.0);
    }
  // Slaney normalisation: continuous triangle area 1, so the Riemann sum
  // on a fine grid approaches 1 (filters wide compared to the bin spacing).
  const auto fn = unwrap(melFilterbank(40, 65536, SampleRateHz{16000.0}, 100.0, 7000.0,
                                       MelScale::Slaney, MelNorm::Slaney));
  for (const auto& w : fn.weights) {
    double area = 0.0;
    for (double v : w) area += v * 16000.0 / 65536.0;
    CHECK(std::fabs(area - 1.0) < 1e-3);
  }
  CHECK(melFilterbank(26, 500, k16, 0.0, 8000.0).error() == QuantityError::InvalidArgument);
  CHECK(melFilterbank(26, 512, k16, 0.0, 9000.0).error() == QuantityError::InvalidArgument);
}

TEST_CASE("DCT-II is orthonormal and exactly invertible", "[d39mfcc]") {
  std::vector<double> x(26);
  for (std::size_t i = 0; i < x.size(); ++i) x[i] = std::sin(0.7 * static_cast<double>(i)) + 0.1 * i;
  const auto c = dctII(x, 26);
  const auto y = dctIII(c, 26);
  double e2x = 0.0, e2c = 0.0;
  for (std::size_t i = 0; i < 26; ++i) {
    CHECK(std::fabs(y[i] - x[i]) < 1e-12);
    e2x += x[i] * x[i];
    e2c += c[i] * c[i];
  }
  CHECK(std::fabs(e2x - e2c) < 1e-10 * e2x);  // Parseval
  // A constant vector has only c0 = value * sqrt(N).
  const auto k = dctII(std::vector<double>(26, 3.0), 26);
  CHECK(std::fabs(k[0] - 3.0 * std::sqrt(26.0)) < 1e-12);
  for (std::size_t i = 1; i < 26; ++i) CHECK(std::fabs(k[i]) < 1e-12);
}

TEST_CASE("MFCC equals an independent brute-force pipeline", "[d39mfcc]") {
  std::vector<double> x(4000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = 0.3 * std::sin(0.11 * static_cast<double>(i)) + 0.2 * std::sin(0.93 * static_cast<double>(i)) +
           0.05 * std::cos(2.1 * static_cast<double>(i) + 0.3);
  MfccOptions o;
  const auto r = unwrap(mfcc(x, k16, o));
  // Brute force: explicit pre-emphasis, Hamming, O(N^2) DFT, the filter
  // formula, log, and the DCT sum; no shared code except the filterbank's
  // edge frequencies' formula (hzToMel), itself tested above.
  const std::size_t n = 512;
  std::vector<double> y(x.size());
  y[0] = x[0];
  for (std::size_t i = 1; i < x.size(); ++i) y[i] = x[i] - 0.97 * x[i - 1];
  const std::size_t frames = (x.size() - n + 159) / 160 + 1;
  REQUIRE(r.coeffs.size() == frames);
  const double m0 = 0.0, m1 = 2595.0 * std::log10(1.0 + 8000.0 / 700.0);
  std::vector<double> edges;
  for (int i = 0; i < 28; ++i)
    edges.push_back(700.0 * (std::pow(10.0, (m0 + (m1 - m0) * i / 27.0) / 2595.0) - 1.0));
  double worst = 0.0;
  for (std::size_t f = 0; f < frames; f += 5) {
    std::vector<double> pow(n / 2 + 1);
    for (std::size_t k = 0; k <= n / 2; ++k) {
      std::complex<double> s{0.0, 0.0};
      for (std::size_t i = 0; i < n; ++i) {
        const std::size_t idx = f * 160 + i;
        const double v = idx < y.size() ? y[idx] : 0.0;
        const double w = 0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * i / static_cast<double>(n));
        s += v * w * std::polar(1.0, -2.0 * std::numbers::pi * k * i / static_cast<double>(n));
      }
      pow[k] = std::norm(s);
    }
    std::vector<double> lm(26);
    for (std::size_t m = 0; m < 26; ++m) {
      double e = 0.0;
      for (std::size_t k = 0; k <= n / 2; ++k) {
        const double fr = k * 16000.0 / n;
        double w = 0.0;
        if (fr > edges[m] && fr < edges[m + 1]) w = (fr - edges[m]) / (edges[m + 1] - edges[m]);
        else if (fr >= edges[m + 1] && fr < edges[m + 2])
          w = (edges[m + 2] - fr) / (edges[m + 2] - edges[m + 1]);
        e += w * pow[k];
      }
      lm[m] = std::log(std::max(e, 1e-10));
    }
    for (std::size_t q = 0; q < 13; ++q) {
      double s = 0.0;
      for (std::size_t m = 0; m < 26; ++m)
        s += lm[m] * std::cos(std::numbers::pi * q * (2.0 * m + 1.0) / 52.0);
      s *= q == 0 ? std::sqrt(1.0 / 26.0) : std::sqrt(2.0 / 26.0);
      s *= 1.0 + 11.0 * std::sin(std::numbers::pi * q / 22.0);
      worst = std::max(worst, std::fabs(s - r.coeffs[f][q]));
    }
  }
  CHECK(worst < 1e-8);
}

TEST_CASE("MFCC: gain moves c0 only; deltas; tone placement", "[d39mfcc]") {
  MfccOptions o;
  o.preEmphasis = 0.0;
  auto x = tone(440.0, 0.2, 8000);
  for (std::size_t i = 0; i < x.size(); ++i) x[i] += 0.05 * std::sin(1.3 * static_cast<double>(i));
  auto x2 = x;
  for (auto& v : x2) v *= 3.0;
  const auto a = unwrap(mfcc(x, k16, o)), b = unwrap(mfcc(x2, k16, o));
  double worstRest = 0.0, worstC0 = 0.0;
  const double shift = 2.0 * std::log(3.0) * std::sqrt(26.0);
  for (std::size_t f = 0; f < a.coeffs.size(); ++f) {
    worstC0 = std::max(worstC0, std::fabs(b.coeffs[f][0] - a.coeffs[f][0] - shift));
    for (std::size_t q = 1; q < 13; ++q)
      worstRest = std::max(worstRest, std::fabs(b.coeffs[f][q] - a.coeffs[f][q]));
  }
  CHECK(worstC0 < 1e-9);
  CHECK(worstRest < 1e-9);
  // Deltas of a linear ramp: the slope, in the interior.
  std::vector<std::vector<double>> ramp;
  for (int t = 0; t < 20; ++t) ramp.push_back({0.5 * t, -2.0 * t + 1.0});
  const auto d = deltas(ramp, 2);
  for (std::size_t t = 2; t < 18; ++t) {
    CHECK(std::fabs(d[t][0] - 0.5) < 1e-12);
    CHECK(std::fabs(d[t][1] + 2.0) < 1e-12);
  }
  // CMN zeroes the mean.
  auto c = a.coeffs;
  cepstralMeanNormalize(c);
  double mean1 = 0.0;
  for (const auto& f : c) mean1 += f[1];
  CHECK(std::fabs(mean1) < 1e-9);
  // A 1 kHz tone's log-mel peaks in the filter whose centre is nearest.
  const auto t = unwrap(mfcc(tone(1000.0, 0.5, 4000), k16, o));
  const auto& lm = t.logMel[3];
  const auto peak = static_cast<std::size_t>(std::max_element(lm.begin(), lm.end()) - lm.begin());
  std::size_t nearest = 0;
  for (std::size_t m = 0; m < 26; ++m)
    if (std::fabs(t.filterbank.edgesHz[m + 1] - 1000.0) <
        std::fabs(t.filterbank.edgesHz[nearest + 1] - 1000.0))
      nearest = m;
  CHECK(peak == nearest);
  CHECK(mfcc({}, k16, o).error() == QuantityError::InvalidArgument);
  o.numCoeffs = 30;
  CHECK(mfcc(x, k16, o).error() == QuantityError::InvalidArgument);
}
