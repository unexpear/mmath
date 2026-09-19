// D19 — spectrum analysis (Catch2).
//
// Exact references: a bin-centred sine reads its exact amplitude; the
// power spectrum satisfies Parseval to roundoff for every window; IEC
// 61260-1 base-ten band geometry (1 kHz reference, G = 10^0.3, geometric
// centres, exact tiling); band levels of a known tone; a spectrogram of a
// known sweep tracks the instantaneous frequency frame by frame.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/analysis/spectrum.h>
#include <mmathen/math/comparison.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

constexpr double kPi = std::numbers::pi;
constexpr double kFs = 48000.0;

std::vector<double> sine(std::size_t n, double f, double a, double phase = 0.3) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = a * std::cos(2.0 * kPi * f * static_cast<double>(i) / kFs + phase);
  return x;
}

std::vector<double> lcgNoise(std::size_t n) {
  std::uint64_t s = 12345;
  std::vector<double> x(n);
  for (auto& v : x) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v = static_cast<double>(s >> 11) / 9007199254740992.0 - 0.5;
  }
  return x;
}
}  // namespace

TEST_CASE("a bin-centred sine reads its exact amplitude", "[d19spec]") {
  const std::size_t n = 4096;
  const double f = 37.0 * kFs / static_cast<double>(n);  // bin 37
  for (const Window w : {Window::Rectangular, Window::Hann,
                         Window::BlackmanHarris}) {
    const auto s = unwrap(computeSpectrum(sine(n, f, 0.5), SampleRateHz{kFs}, w));
    CHECK(nearlyEqual(s.amplitude[37], 0.5, 1e-12, 0.0));
    CHECK(nearlyEqual(amplitudeDb(s)[37], 20.0 * std::log10(0.5), 1e-10, 0.0));
    CHECK(s.amplitude.size() == n / 2 + 1);
    CHECK(nearlyEqual(s.binHz, kFs / 4096.0, 0.0, 1e-15));
  }
  // Full-scale sine: 0 dBFS on its bin.
  const auto fullScale = unwrap(computeSpectrum(sine(n, f, 1.0), SampleRateHz{kFs},
                                                Window::Hann));
  CHECK(std::fabs(amplitudeDb(fullScale)[37]) < 1e-10);
}

TEST_CASE("power spectrum satisfies Parseval exactly", "[d19spec]") {
  const auto x = lcgNoise(2048);
  for (const Window w : {Window::Rectangular, Window::Hann,
                         Window::BlackmanHarris}) {
    const auto s = unwrap(computeSpectrum(x, SampleRateHz{kFs}, w));
    const auto win = unwrap(windowCoefficients(w, x.size()));
    double num = 0.0, den = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
      num += win[i] * x[i] * win[i] * x[i];
      den += win[i] * win[i];
    }
    double total = 0.0;
    for (const double p : s.power) total += p;
    CHECK(nearlyEqual(total, num / den, 0.0, 1e-12));
  }
  // A steady sine has mean square A^2/2 (-3.01 dB at full scale).
  const auto tone = unwrap(computeSpectrum(sine(32768, 997.3, 1.0),
                                           SampleRateHz{kFs}, Window::Hann));
  double total = 0.0;
  for (const double p : tone.power) total += p;
  CHECK(nearlyEqual(total, 0.5, 0.0, 1e-3));
}

TEST_CASE("IEC 61260-1 base-ten band geometry", "[d19spec]") {
  const double g = std::pow(10.0, 0.3);
  // 19 Hz lower limit: the nominal "20 Hz" band's exact centre is
  // 1000 * 10^-1.7 = 19.95 Hz, which a 20 Hz limit would (correctly) drop.
  const auto thirds = unwrap(fractionalOctaveBands(3, FrequencyHz{19.0},
                                                   FrequencyHz{20000.0}));
  // 1 kHz is the reference band; neighbours are G^(1/3) apart.
  bool hasReference = false;
  for (std::size_t i = 0; i < thirds.size(); ++i) {
    const auto& b = thirds[i];
    if (b.center.value() == 1000.0) hasReference = true;
    CHECK(nearlyEqual(std::sqrt(b.lower.value() * b.upper.value()),
                      b.center.value(), 0.0, 1e-12));  // geometric centre
    CHECK(nearlyEqual(b.upper.value() / b.lower.value(), std::pow(g, 1.0 / 3.0),
                      0.0, 1e-12));
    if (i + 1 < thirds.size()) {
      CHECK(nearlyEqual(thirds[i + 1].center.value() / b.center.value(),
                        std::pow(g, 1.0 / 3.0), 0.0, 1e-12));
      CHECK(nearlyEqual(thirds[i + 1].lower.value(), b.upper.value(), 0.0,
                        1e-12));  // bands tile
    }
  }
  CHECK(hasReference);
  // Nominal 20 Hz .. 20 kHz: 31 third-octave bands (exact 19.95 .. 19953 Hz).
  CHECK(thirds.size() == 31);
  CHECK(nearlyEqual(thirds.front().center.value(), 1000.0 * std::pow(g, -17.0 / 3.0),
                    0.0, 1e-12));
  const auto octaves = unwrap(fractionalOctaveBands(1, FrequencyHz{30.0},
                                                    FrequencyHz{17000.0}));
  // Exact centres 31.62 .. 15849 Hz: the nominal 31.5 .. 16k labels.
  CHECK(octaves.size() == 10);
  CHECK(nearlyEqual(octaves.front().center.value(), 1000.0 * std::pow(g, -5.0),
                    0.0, 1e-12));
  CHECK(fractionalOctaveBands(2, FrequencyHz{20.0}, FrequencyHz{20000.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(fractionalOctaveBands(3, FrequencyHz{0.0}, FrequencyHz{100.0})
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("band levels put a tone's power in its band", "[d19spec]") {
  const auto s = unwrap(computeSpectrum(sine(32768, 1000.0, 0.5),
                                        SampleRateHz{kFs}, Window::Hann));
  const auto bands = unwrap(fractionalOctaveBands(3, FrequencyHz{20.0},
                                                  FrequencyHz{20000.0}));
  const auto levels = bandLevels(s, bands);
  double total = 0.0;
  for (const auto& l : levels) {
    total += l.power;
    if (l.band.center.value() == 1000.0) {
      // Mean square of a 0.5 sine: 0.125 -> -9.03 dB.
      CHECK(nearlyEqual(l.levelDb, 10.0 * std::log10(0.125), 0.02, 0.0));
    } else if (std::fabs(std::log10(l.band.center.value() / 1000.0)) > 0.15) {
      CHECK(l.levelDb < -60.0);  // far bands: leakage only
    }
  }
  // Partition: bands + bins outside them = the whole spectrum, exactly.
  double outside = 0.0;
  for (std::size_t k = 0; k < s.power.size(); ++k) {
    const double f = static_cast<double>(k) * s.binHz;
    if (f < bands.front().lower.value() || f >= bands.back().upper.value())
      outside += s.power[k];
  }
  double all = 0.0;
  for (const double p : s.power) all += p;
  CHECK(nearlyEqual(total + outside, all, 0.0, 1e-12));
}

TEST_CASE("coarse resolution reports empty bands honestly", "[d19spec]") {
  // 16 samples at 48 kHz: 3 kHz bins; the 31.6 Hz band holds none.
  const auto s = unwrap(computeSpectrum(sine(16, 3000.0, 1.0), SampleRateHz{kFs},
                                        Window::Hann));
  const auto bands = unwrap(fractionalOctaveBands(3, FrequencyHz{30.0},
                                                  FrequencyHz{35.0}));
  const auto levels = bandLevels(s, bands);
  REQUIRE(levels.size() == 1);
  CHECK(levels[0].bins == 0);
  CHECK(levels[0].levelDb == kSpectrumFloorDb);
}

TEST_CASE("spectrogram tracks a known sweep frame by frame", "[d19spec]") {
  // Linear sweep 200 -> 4200 Hz over 1 s: f(t) = 200 + 4000 t.
  const std::size_t n = 48000;
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / kFs;
    x[i] = 0.5 * std::sin(2.0 * kPi * (200.0 * t + 2000.0 * t * t));
  }
  const auto g = unwrap(computeSpectrogram(x, SampleRateHz{kFs},
                                           Window::Hann, 1024, 512));
  CHECK(g.frames == 1 + (n - 1024) / 512);
  CHECK(g.bins == 513);
  for (std::size_t fr = 0; fr < g.frames; ++fr) {
    std::size_t peak = 0;
    for (std::size_t k = 1; k < g.bins; ++k)
      if (g.at(fr, k) > g.at(fr, peak)) peak = k;
    const double tMid = (static_cast<double>(fr) * 512.0 + 512.0) / kFs;
    const double expected = 200.0 + 4000.0 * tMid;
    CHECK(std::fabs(static_cast<double>(peak) * g.binHz - expected) <=
          1.5 * g.binHz);
  }
  // A steady tone gives the same peak in every frame.
  const auto steady = unwrap(computeSpectrogram(sine(16384, 1500.0, 0.5),
                                                SampleRateHz{kFs}, Window::Hann,
                                                2048, 1024));
  for (std::size_t fr = 0; fr < steady.frames; ++fr) {
    std::size_t peak = 0;
    for (std::size_t k = 1; k < steady.bins; ++k)
      if (steady.at(fr, k) > steady.at(fr, peak)) peak = k;
    CHECK(peak == 64);  // 1500 / (48000/2048) = 64
  }
  CHECK(computeSpectrogram(x, SampleRateHz{kFs}, Window::Hann, 1000, 512)
            .error() == QuantityError::InvalidArgument);
  CHECK(computeSpectrogram(x, SampleRateHz{kFs}, Window::Hann, 1024, 0)
            .error() == QuantityError::InvalidArgument);
}
