// D11 — FFT and sinusoid measurement (Catch2).
//
// The FFT is checked against a naive O(N^2) DFT written here, plus exact
// identities (impulse, constant, Parseval, round trip). The estimator is
// checked on synthesized sinusoids whose frequency, amplitude and phase are
// known exactly, alone and next to a second component.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <array>
#include <complex>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/math/comparison.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;
using cplx = std::complex<double>;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

// Deterministic test signal (LCG), independent of any library RNG.
std::vector<cplx> lcgSignal(std::size_t n) {
  std::uint64_t s = 0x9E3779B97F4A7C15ULL;
  std::vector<cplx> out(n);
  for (auto& z : out) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const double re = static_cast<double>(s >> 11) / 9007199254740992.0 - 0.5;
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    const double im = static_cast<double>(s >> 11) / 9007199254740992.0 - 0.5;
    z = {re, im};
  }
  return out;
}

std::vector<cplx> naiveDft(const std::vector<cplx>& x) {
  const std::size_t n = x.size();
  std::vector<cplx> out(n);
  for (std::size_t k = 0; k < n; ++k) {
    cplx sum{0.0, 0.0};
    for (std::size_t j = 0; j < n; ++j) {
      const double a = -2.0 * std::numbers::pi * static_cast<double>(k * j) /
                       static_cast<double>(n);
      sum += x[j] * cplx{std::cos(a), std::sin(a)};
    }
    out[k] = sum;
  }
  return out;
}

std::vector<double> tone(std::size_t n, double fs,
                         std::vector<std::array<double, 3>> parts) {
  std::vector<double> x(n, 0.0);
  for (std::size_t i = 0; i < n; ++i)
    for (const auto& p : parts)
      x[i] += p[1] * std::cos(2.0 * std::numbers::pi * p[0] *
                                  static_cast<double>(i) / fs +
                              p[2]);
  return x;
}
}  // namespace

TEST_CASE("FFT matches a naive DFT", "[d11fft]") {
  for (const std::size_t n : {1u, 2u, 8u, 64u, 512u}) {
    const auto x = lcgSignal(n);
    const auto fast = unwrap(fft(x));
    const auto slow = naiveDft(x);
    double worst = 0.0;
    for (std::size_t k = 0; k < n; ++k)
      worst = (std::max)(worst, std::abs(fast[k] - slow[k]));
    CHECK(worst < 1e-11);
  }
}

TEST_CASE("FFT identities: impulse, constant, round trip, Parseval",
          "[d11fft]") {
  std::vector<cplx> impulse(16, cplx{0.0, 0.0});
  impulse[0] = 1.0;
  for (const auto& z : unwrap(fft(impulse))) CHECK(z == cplx{1.0, 0.0});

  std::vector<cplx> ones(32, cplx{1.0, 0.0});
  const auto dc = unwrap(fft(ones));
  CHECK(std::abs(dc[0] - cplx{32.0, 0.0}) < 1e-12);
  for (std::size_t k = 1; k < 32; ++k) CHECK(std::abs(dc[k]) < 1e-12);

  const auto x = lcgSignal(1024);
  const auto back = unwrap(ifft(unwrap(fft(x))));
  double worst = 0.0;
  double timeEnergy = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i) {
    worst = (std::max)(worst, std::abs(back[i] - x[i]));
    timeEnergy += std::norm(x[i]);
  }
  CHECK(worst < 1e-13);
  double freqEnergy = 0.0;
  for (const auto& z : unwrap(fft(x))) freqEnergy += std::norm(z);
  CHECK(nearlyEqual(timeEnergy, freqEnergy / 1024.0, 0.0, 1e-12));
}

TEST_CASE("FFT and windows reject invalid input", "[d11fft]") {
  CHECK(fft({}).error() == QuantityError::InvalidArgument);
  CHECK(fft(std::vector<cplx>(12)).error() == QuantityError::InvalidArgument);
  std::vector<cplx> bad(8);
  bad[3] = cplx{std::numeric_limits<double>::quiet_NaN(), 0.0};
  CHECK(fft(bad).error() == QuantityError::InvalidArgument);
  CHECK(windowCoefficients(Window::Hann, 0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("periodic window sums are exact", "[d11fft]") {
  // Periodic cosine sums vanish over whole periods: sum = a0 * N.
  const auto hann = unwrap(windowCoefficients(Window::Hann, 1024));
  const auto bh = unwrap(windowCoefficients(Window::BlackmanHarris, 1024));
  double sh = 0.0;
  double sb = 0.0;
  for (std::size_t i = 0; i < 1024; ++i) {
    sh += hann[i];
    sb += bh[i];
  }
  CHECK(nearlyEqual(sh, 512.0, 1e-9, 0.0));
  CHECK(nearlyEqual(sb, 0.35875 * 1024.0, 1e-9, 0.0));
  CHECK(hann[0] == 0.0);
  CHECK(nearlyEqual(bh[0], 0.35875 - 0.48829 + 0.14128 - 0.01168, 1e-16, 0.0));
}

TEST_CASE("single sinusoid is measured to high precision", "[d11fft]") {
  const double fs = 48000.0;
  const std::size_t n = 8192;  // bin = 5.859375 Hz
  const auto x = tone(n, fs, {{440.123456789, 0.7, 0.4}});
  const auto est = unwrap(estimateSinusoid(x, SampleRateHz{fs},
                                           Window::BlackmanHarris,
                                           FrequencyHz{300.0},
                                           FrequencyHz{600.0}));
  const double err8k = std::fabs(est.frequency.value() - 440.123456789);
  CHECK(err8k < 5e-6);
  CHECK(nearlyEqual(est.amplitude, 0.7, 0.0, 1e-6));
  CHECK(nearlyEqual(est.phase.value(), 0.4, 1e-5, 0.0));
  // Measured residual: 7.0e-7 Hz at N = 8192 and 1.1e-7 Hz at N = 65536
  // (GCC). A leakage-bias hypothesis (the tone's negative-frequency image)
  // predicted a >= 10x drop; the observed drop is ~6x, so the residual's
  // source is NOT claimed. What is asserted is its size: it improves with N
  // and stays below 1e-5 cents — over 1000x finer than the FDTD pitch errors
  // (0.01..10 cents) this estimator is used to measure.
  const auto y = tone(65536, fs, {{440.123456789, 0.7, 0.4}});
  const auto longer = unwrap(estimateSinusoid(y, SampleRateHz{fs},
                                              Window::BlackmanHarris,
                                              FrequencyHz{300.0},
                                              FrequencyHz{600.0}));
  const double err64k = std::fabs(longer.frequency.value() - 440.123456789);
  CHECK(err64k < err8k);
  const double cents = 1200.0 * std::log2(1.0 + err8k / 440.123456789);
  CHECK(cents < 1e-5);
}

TEST_CASE("a neighbour component several bins away barely biases it",
          "[d11fft]") {
  const double fs = 48000.0;
  const std::size_t n = 16384;  // bin = 2.9296875 Hz
  // 440 Hz next to 480 Hz at the same level (13.7 bins apart).
  const auto x = tone(n, fs, {{440.0, 0.5, 0.0}, {480.0, 0.5, 1.0}});
  const auto a = unwrap(estimateSinusoid(x, SampleRateHz{fs},
                                         Window::BlackmanHarris,
                                         FrequencyHz{420.0},
                                         FrequencyHz{460.0}));
  const auto b = unwrap(estimateSinusoid(x, SampleRateHz{fs},
                                         Window::BlackmanHarris,
                                         FrequencyHz{465.0},
                                         FrequencyHz{495.0}));
  CHECK(std::fabs(a.frequency.value() - 440.0) < 1e-4);
  CHECK(std::fabs(b.frequency.value() - 480.0) < 1e-4);
  CHECK(nearlyEqual(a.amplitude, 0.5, 0.0, 1e-4));
}

TEST_CASE("estimator rejects bad input and edge maxima", "[d11fft]") {
  const double fs = 48000.0;
  const auto x = tone(4096, fs, {{1000.0, 1.0, 0.0}});
  // Band that excludes the tone: only sidelobe ripple lives there. This
  // returned a "sinusoid" before the main-lobe check existed.
  for (const Window w : {Window::BlackmanHarris, Window::Hann,
                         Window::Rectangular})
    CHECK(estimateSinusoid(x, SampleRateHz{fs}, w, FrequencyHz{2000.0},
                           FrequencyHz{2500.0})
              .error() == QuantityError::DomainError);
  // Two tones inside one main lobe are not an isolated sinusoid.
  const auto close = tone(4096, fs, {{1000.0, 1.0, 0.0}, {1020.0, 1.0, 0.0}});
  CHECK(estimateSinusoid(close, SampleRateHz{fs}, Window::BlackmanHarris,
                         FrequencyHz{900.0}, FrequencyHz{1100.0})
            .error() == QuantityError::DomainError);
  // The same windows still accept the real tone.
  for (const Window w : {Window::BlackmanHarris, Window::Hann})
    CHECK(estimateSinusoid(x, SampleRateHz{fs}, w, FrequencyHz{900.0},
                           FrequencyHz{1100.0})
              .ok());
  CHECK(estimateSinusoid(std::vector<double>(100, 0.0), SampleRateHz{fs},
                         Window::Hann, FrequencyHz{10.0}, FrequencyHz{20.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(estimateSinusoid(x, SampleRateHz{fs}, Window::Hann,
                         FrequencyHz{500.0}, FrequencyHz{30000.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(estimateSinusoid(std::vector<double>(4096, 0.0), SampleRateHz{fs},
                         Window::Hann, FrequencyHz{500.0}, FrequencyHz{900.0})
            .error() == QuantityError::DomainError);
}
