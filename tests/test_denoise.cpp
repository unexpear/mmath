// D34 — noise reduction and spectral repair (Catch2).
//
// Special functions match power series and tabulated values. With a 0 dB
// reduction the denoiser is an exact delay. On tones in white noise every
// method raises the SNR by more than 12 dB (measured 14-18 dB), keeps the
// tones' level within 1 dB, and pulls noise-only input down (by less than
// the floor, as the decision-directed estimator does; measured and stated).
// Spectral repair attenuates a tone inside the box by the stated gain and
// leaves a tone outside it, and time outside it, untouched.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/dsp/denoise.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

std::vector<double> whiteNoise(std::size_t n, double amp, std::uint64_t seed) {
  std::vector<double> x(n);
  std::uint64_t s = seed;
  for (auto& v : x) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v = amp * (static_cast<double>(s >> 11) * (2.0 / 9007199254740992.0) - 1.0);
  }
  return x;
}
std::vector<double> tones(std::size_t n) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    x[i] = 0.3 * std::sin(2.0 * std::numbers::pi * 440.0 * t) +
           0.2 * std::sin(2.0 * std::numbers::pi * 1320.0 * t);
  }
  return x;
}
double snrDb(const std::vector<double>& clean, const std::vector<double>& y,
             std::size_t from) {
  double s = 0.0, e = 0.0;
  for (std::size_t i = from; i < clean.size(); ++i) {
    s += clean[i] * clean[i];
    e += (y[i] - clean[i]) * (y[i] - clean[i]);
  }
  return 10.0 * std::log10(s / e);
}
double rms(const std::vector<double>& x, std::size_t from) {
  double s = 0.0;
  for (std::size_t i = from; i < x.size(); ++i) s += x[i] * x[i];
  return std::sqrt(s / static_cast<double>(x.size() - from));
}
double seriesI(int order, double x) {  // I0/I1 power series
  double sum = 0.0, term = order == 0 ? 1.0 : x / 2.0;
  for (int k = 0; k < 400; ++k) {
    sum += term;
    term *= (x / 2.0) * (x / 2.0) / ((k + 1.0) * (k + 1.0 + order));
  }
  return sum;
}
}  // namespace

TEST_CASE("special functions agree with series and tables", "[d34nr]") {
  for (const double x : {0.01, 0.5, 2.0, 3.7, 3.8, 8.0, 20.0, 40.0}) {
    CHECK(std::fabs(special::i0e(x) / (seriesI(0, x) * std::exp(-x)) - 1.0) < 1e-6);
    CHECK(std::fabs(special::i1e(x) / (seriesI(1, x) * std::exp(-x)) - 1.0) < 1e-6);
  }
  // E1 reference values (A&S Table 5.1).
  CHECK(std::fabs(special::e1(0.5) - 0.5597735947761608) < 1e-13);
  CHECK(std::fabs(special::e1(1.0) - 0.2193839343955203) < 1e-13);
  CHECK(std::fabs(special::e1(2.0) - 0.04890051070806112) < 1e-13);
  CHECK(std::fabs(special::e1(5.0) - 0.001148295591275326) < 1e-15);
  // Gains: all methods -> 1 at high SNR, -> 0 at no signal.
  for (auto m : {DenoiseMethod::SpectralSubtraction, DenoiseMethod::MmseStsa,
                 DenoiseMethod::MmseLsa}) {
    CHECK(std::fabs(SpectralDenoiser::gain(m, 1e4, 1e4) - 1.0) < 1e-3);
    CHECK(SpectralDenoiser::gain(m, 1.0, 1e-4) < 0.05);
  }
}

TEST_CASE("0 dB reduction is an exact delay", "[d34nr]") {
  const auto n = whiteNoise(48000, 0.1, 5);
  const auto prof = unwrap(learnNoiseProfile(n, 1024));
  auto d = unwrap(SpectralDenoiser::create(prof, DenoiseMethod::MmseLsa, 0.0));
  auto x = tones(12000);
  std::vector<double> y(x.size());
  d.process(x.data(), y.data(), x.size());
  const std::size_t L = d.latencyFrames();
  double worst = 0.0;
  for (std::size_t i = L; i < x.size(); ++i) worst = std::max(worst, std::fabs(y[i] - x[i - L]));
  CHECK(worst < 1e-12);
}

TEST_CASE("every method raises SNR, keeps tones, floors noise", "[d34nr]") {
  const std::size_t n = 3 * 48000;
  const auto clean = tones(n);
  const auto noise = whiteNoise(n, 0.3, 17);  // ~0 dB SNR broadband
  std::vector<double> noisy(n);
  for (std::size_t i = 0; i < n; ++i) noisy[i] = clean[i] + noise[i];
  // Profile from a DIFFERENT stretch of the same noise process.
  const auto prof = unwrap(learnNoiseProfile(whiteNoise(48000, 0.3, 99), 2048));
  const double before = snrDb(clean, noisy, 4096);
  for (auto m : {DenoiseMethod::SpectralSubtraction, DenoiseMethod::MmseStsa,
                 DenoiseMethod::MmseLsa}) {
    const auto y = unwrap(denoise(noisy, prof, m, 30.0));
    REQUIRE(y.size() == noisy.size());
    const double after = snrDb(clean, y, 4096);
    // Measured at 0 dB input SNR, 30 dB floor: subtraction +17.0,
    // MMSE-STSA +14.2, MMSE-LSA +17.6 dB.
    CHECK(after - before > 12.0);
    // Tone levels kept within 1 dB (sinusoid estimator, D11).
    for (const double f : {440.0, 1320.0}) {
      std::vector<double> seg(y.begin() + 48000, y.begin() + 48000 + 16384);
      const auto est = unwrap(estimateSinusoid(seg, kRate, Window::Hann,
                                               FrequencyHz{f - 30.0}, FrequencyHz{f + 30.0}));
      const double want = f == 440.0 ? 0.3 : 0.2;
      CHECK(std::fabs(20.0 * std::log10(est.amplitude / want)) < 1.0);
    }
  }
  // Noise alone is pulled down, but NOT to the -30 dB floor: the
  // decision-directed a priori SNR fluctuates on pure noise, so MMSE gains
  // stay above the floor (measured -19.3 dB for LSA). The floor is a bound.
  const auto y = unwrap(denoise(noise, prof, DenoiseMethod::MmseLsa, 30.0));
  const double drop = 20.0 * std::log10(rms(y, 4096) / rms(noise, 4096));
  CHECK(drop < -16.0);
  CHECK(drop >= -30.0 - 1e-9);
}

TEST_CASE("spectral repair attenuates the box and nothing else", "[d34nr]") {
  const std::size_t n = 2 * 48000;
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / 48000.0;
    x[i] = 0.3 * std::sin(2.0 * std::numbers::pi * 1000.0 * t) +
           0.3 * std::sin(2.0 * std::numbers::pi * 3000.0 * t);
  }
  // Box: 0.8-1.2 s, 800-1200 Hz, -20 dB.
  const auto y = unwrap(spectralGainRegion(x, kRate, 0.8, 1.2, 800.0, 1200.0, -20.0));
  REQUIRE(y.size() == x.size());
  auto level = [&](const std::vector<double>& s, std::size_t at, double f) {
    std::vector<double> seg(s.begin() + static_cast<std::ptrdiff_t>(at),
                            s.begin() + static_cast<std::ptrdiff_t>(at + 8192));
    return unwrap(estimateSinusoid(seg, kRate, Window::Hann, FrequencyHz{f - 100.0},
                                   FrequencyHz{f + 100.0})).amplitude;
  };
  const std::size_t mid = static_cast<std::size_t>(0.9 * 48000.0);  // inside in time
  CHECK(std::fabs(20.0 * std::log10(level(y, mid, 1000.0) / 0.3) - -20.0) < 1.0);
  CHECK(std::fabs(20.0 * std::log10(level(y, mid, 3000.0) / 0.3)) < 0.05);
  // Far outside the box in time: the exact OLA identity.
  double worst = 0.0;
  for (std::size_t i = 4096; i < static_cast<std::size_t>(0.5 * 48000.0); ++i)
    worst = std::max(worst, std::fabs(y[i] - x[i]));
  CHECK(worst < 1e-12);
  CHECK(spectralGainRegion(x, kRate, 1.0, 0.5, 0.0, 100.0, -6.0).error() ==
        QuantityError::InvalidArgument);
}
