// D28 — phase vocoder and TD-PSOLA (Catch2).
//
// Vocoder: stretch 1 reconstructs the input to roundoff (with and without
// phase locking); stretches of 0.75/1.5/2 keep a sine's frequency (D11
// estimator) and level, at the stated length; a pitch shift of +7
// semitones moves a sine by exactly that ratio at the same length.
// PSOLA: a vowel-like tone (200 Hz pulse train through a fixed 1 kHz
// resonance) shifted by 1.25 comes out at 250 Hz (YIN) with its level kept
// and its strongest harmonic still next to 1 kHz — the formant stays —
// while the vocoder's shift moves the formant to ~1.25 kHz. PSOLA time
// stretch 2 doubles the length at the same pitch.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/analysis/pitch.h>
#include <mmathen/analysis/spectrum.h>
#include <mmathen/dsp/biquad.h>
#include <mmathen/dsp/filters.h>
#include <mmathen/dsp/time_pitch.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

std::vector<double> sine(double f, std::size_t n, double amp = 0.5) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = amp * std::sin(2.0 * std::numbers::pi * f * static_cast<double>(i) /
                          kRate.value());
  return x;
}

std::vector<double> mid(const std::vector<double>& x, std::size_t n) {
  const std::size_t s = (x.size() - n) / 2;
  return {x.begin() + static_cast<std::ptrdiff_t>(s),
          x.begin() + static_cast<std::ptrdiff_t>(s + n)};
}

double rms(const std::vector<double>& x) {
  double s = 0.0;
  for (double v : x) s += v * v;
  return std::sqrt(s / static_cast<double>(x.size()));
}

// 200 Hz pulse train through a Q = 6 band-pass at 1 kHz: a fixed formant.
std::vector<double> vowel(std::size_t n) {
  std::vector<double> x(n, 0.0);
  for (std::size_t i = 0; i < n; i += 240) x[i] = 1.0;
  Biquad bp{unwrap(makeBandpass(FrequencyHz{1000.0}, 6.0, kRate))};
  bp.processBlock(x.data(), x.data(), FrameCount{static_cast<std::uint64_t>(n)});
  return x;
}

// Frequency of the strongest spectral peak between 300 Hz and 3 kHz.
double strongestHz(const std::vector<double>& x) {
  const auto s = unwrap(computeSpectrum(mid(x, 16384), kRate, Window::Hann));
  std::size_t best = 0;
  for (std::size_t k = 0; k < s.amplitude.size(); ++k) {
    const double f = static_cast<double>(k) * s.binHz;
    if (f < 300.0 || f > 3000.0) continue;
    if (best == 0 || s.amplitude[k] > s.amplitude[best]) best = k;
  }
  return static_cast<double>(best) * s.binHz;
}

double sineHz(const std::vector<double>& seg) {
  return unwrap(estimateSinusoid(seg, kRate, Window::Hann, FrequencyHz{100.0},
                                 FrequencyHz{2000.0}))
      .frequency.value();
}

double yinHz(const std::vector<double>& x) {
  PitchSearch s;
  s.fMinHz = 60.0;
  s.fMaxHz = 1000.0;
  return unwrap(yinPitch(mid(x, 4096), kRate, s)).frequencyHz;
}
}  // namespace

TEST_CASE("vocoder at stretch 1 reconstructs its input", "[d28tp]") {
  std::vector<double> x = sine(440.0, 20000);
  const auto y = sine(1234.5, 20000, 0.2);
  for (std::size_t i = 0; i < x.size(); ++i) x[i] += y[i];
  for (const bool lock : {false, true}) {
    PhaseVocoderOptions o;
    o.phaseLocking = lock;
    const auto out = unwrap(phaseVocoderStretch(x, 1.0, o));
    REQUIRE(out.size() == x.size());
    double worst = 0.0;
    for (std::size_t i = 2048; i + 2048 < x.size(); ++i)
      worst = std::max(worst, std::fabs(out[i] - x[i]));
    CHECK(worst < 1e-9);
  }
}

TEST_CASE("vocoder stretch keeps frequency and level", "[d28tp]") {
  const auto x = sine(440.0, 48000);
  for (const double st : {0.75, 1.5, 2.0}) {
    const auto out = unwrap(phaseVocoderStretch(x, st));
    CHECK(out.size() == static_cast<std::size_t>(std::llround(st * 48000.0)));
    const auto seg = mid(out, 8192);
    CHECK(std::fabs(sineHz(seg) - 440.0) < 0.05);
    CHECK(std::fabs(20.0 * std::log10(rms(seg) / rms(mid(x, 8192)))) < 0.5);
  }
}

TEST_CASE("vocoder pitch shift moves a sine by the ratio", "[d28tp]") {
  const auto x = sine(440.0, 48000);
  const double p = std::pow(2.0, 7.0 / 12.0);
  const auto out = unwrap(phaseVocoderPitchShift(x, kRate, p));
  CHECK(out.size() == x.size());
  CHECK(std::fabs(sineHz(mid(out, 8192)) / (440.0 * p) - 1.0) < 2e-4);
  const auto down = unwrap(phaseVocoderPitchShift(x, kRate, 1.0 / p));
  CHECK(std::fabs(sineHz(mid(down, 8192)) / (440.0 / p) - 1.0) < 2e-4);
}

TEST_CASE("PSOLA keeps the formant; the vocoder moves it", "[d28tp]") {
  const auto x = vowel(48000);
  CHECK(std::fabs(yinHz(x) - 200.0) < 0.5);
  CHECK(std::fabs(strongestHz(x) - 1000.0) < 10.0);  // harmonic 5

  const auto ps = unwrap(psola(x, kRate, 1.25, 1.0));
  CHECK(ps.size() == x.size());
  CHECK(std::fabs(yinHz(ps) / 250.0 - 1.0) < 1e-4);  // measured <= 1.4e-5
  CHECK(std::fabs(strongestHz(ps) - 1000.0) < 10.0);  // 4 x 250: formant kept
  CHECK(std::fabs(20.0 * std::log10(rms(mid(ps, 16384)) / rms(mid(x, 16384)))) <
        0.75);  // measured 0.37 dB

  const auto pv = unwrap(phaseVocoderPitchShift(x, kRate, 1.25));
  CHECK(std::fabs(yinHz(pv) / 250.0 - 1.0) < 1e-4);  // measured <= 1.4e-5
  CHECK(std::fabs(strongestHz(pv) - 1250.0) < 10.0);  // 5 x 250: moved

  // Down a minor third.
  const double down = std::pow(2.0, -3.0 / 12.0);
  const auto pd = unwrap(psola(x, kRate, down, 1.0));
  CHECK(std::fabs(yinHz(pd) / (200.0 * down) - 1.0) < 1e-4);  // measured <= 1.4e-5
}

TEST_CASE("PSOLA time stretch keeps pitch; refusals", "[d28tp]") {
  const auto x = vowel(24000);
  const auto slow = unwrap(psola(x, kRate, 1.0, 2.0));
  CHECK(slow.size() == 48000);
  CHECK(std::fabs(yinHz(slow) / 200.0 - 1.0) < 1e-4);  // measured <= 1.4e-5
  CHECK(std::fabs(strongestHz(slow) - 1000.0) < 10.0);

  CHECK(psola(x, kRate, 3.0, 1.0).error() == QuantityError::InvalidArgument);
  CHECK(psola(std::vector<double>(100, 0.0), kRate, 1.0, 1.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(phaseVocoderStretch(x, 10.0).error() == QuantityError::InvalidArgument);
  PhaseVocoderOptions bad;
  bad.frameSize = 1000;
  CHECK(phaseVocoderStretch(x, 1.0, bad).error() ==
        QuantityError::InvalidArgument);
}
