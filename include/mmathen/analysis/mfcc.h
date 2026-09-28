#pragma once

// D39 — mel scales, mel filterbanks, DCT-II and MFCCs (with deltas and
// cepstral mean normalisation).
//
// Pipeline (Davis & Mermelstein 1980; HTK Book ch. 5; the same steps as
// librosa / python_speech_features, with their conventions selectable):
//   x -> pre-emphasis y[n] = x[n] - a x[n-1]
//     -> frames (size N, hop H), window (Hamming or Hann, periodic)
//     -> |FFT|^2 (power) or |FFT| (magnitude, HTK's default)
//     -> M triangular mel filters -> log (natural, floored)
//     -> orthonormal DCT-II -> first C coefficients -> optional lifter.
//
// Mel scales (both exact to their published definitions):
//   HTK / O'Shaughnessy:  m = 2595 log10(1 + f/700)
//   Slaney (Auditory Toolbox, librosa's default): linear below 1 kHz,
//     m = f / (200/3); above, m = 15 + ln(f/1000) / (ln 6.4 / 27).
// Filters are triangles in Hz between mel-spaced edge frequencies,
// evaluated at the bin frequencies k fs / N (librosa's construction).
// Unnormalised (peak 1) filters form a partition of unity between the first
// and last centres; Slaney normalisation scales each to unit area in Hz
// (factor 2 / (f_{m+1} - f_{m-1})).
// The DCT is orthonormal, so it is exactly invertible (dctIII) and
// a constant offset in every log-mel value moves only c0, by offset*sqrt(M):
// a gain g changes c0 alone, by 2 ln(g) sqrt(M) for a power spectrum.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "fft.h"

namespace mmathen {

enum class MelScale { Htk, Slaney };
enum class MelNorm { None, Slaney };
enum class MfccWindow { Hamming, Hann };

[[nodiscard]] inline double hzToMel(double hz, MelScale s) noexcept {
  if (s == MelScale::Htk) return 2595.0 * std::log10(1.0 + hz / 700.0);
  constexpr double fSp = 200.0 / 3.0;
  if (hz < 1000.0) return hz / fSp;
  const double logStep = std::log(6.4) / 27.0;
  return 15.0 + std::log(hz / 1000.0) / logStep;
}
[[nodiscard]] inline double melToHz(double mel, MelScale s) noexcept {
  if (s == MelScale::Htk) return 700.0 * (std::pow(10.0, mel / 2595.0) - 1.0);
  constexpr double fSp = 200.0 / 3.0;
  if (mel < 15.0) return mel * fSp;
  const double logStep = std::log(6.4) / 27.0;
  return 1000.0 * std::exp(logStep * (mel - 15.0));
}

struct MelFilterbank {
  std::size_t fftSize = 0;
  std::vector<double> edgesHz;               // M + 2 edge frequencies
  std::vector<std::vector<double>> weights;  // M x (fftSize/2 + 1)
};

// M triangular filters on [fMin, fMax] for an N-point FFT.
[[nodiscard]] inline Result<MelFilterbank, QuantityError> melFilterbank(
    std::size_t numFilters, std::size_t fftSize, SampleRateHz rate, double fMinHz,
    double fMaxHz, MelScale scale = MelScale::Htk, MelNorm norm = MelNorm::None) {
  using R = Result<MelFilterbank, QuantityError>;
  const double fs = rate.value();
  if (numFilters < 1 || numFilters > 512 || !isPowerOfTwo(fftSize) || fftSize < 16 ||
      !(fs > 0.0) || !(fMinHz >= 0.0) || !(fMaxHz > fMinHz) || !(fMaxHz <= fs / 2.0))
    return R::failure(QuantityError::InvalidArgument);
  MelFilterbank fb;
  fb.fftSize = fftSize;
  const double m0 = hzToMel(fMinHz, scale), m1 = hzToMel(fMaxHz, scale);
  for (std::size_t i = 0; i < numFilters + 2; ++i)
    fb.edgesHz.push_back(melToHz(
        m0 + (m1 - m0) * static_cast<double>(i) / static_cast<double>(numFilters + 1), scale));
  const std::size_t bins = fftSize / 2 + 1;
  for (std::size_t m = 0; m < numFilters; ++m) {
    const double lo = fb.edgesHz[m], c = fb.edgesHz[m + 1], hi = fb.edgesHz[m + 2];
    if (!(c > lo) || !(hi > c)) return R::failure(QuantityError::DomainError);
    std::vector<double> w(bins, 0.0);
    const double scaleNorm = norm == MelNorm::Slaney ? 2.0 / (hi - lo) : 1.0;
    for (std::size_t k = 0; k < bins; ++k) {
      const double f = static_cast<double>(k) * fs / static_cast<double>(fftSize);
      const double up = (f - lo) / (c - lo), down = (hi - f) / (hi - c);
      w[k] = std::max(0.0, std::min(up, down)) * scaleNorm;
    }
    fb.weights.push_back(std::move(w));
  }
  return R::success(std::move(fb));
}

// Orthonormal DCT-II of x, first `count` outputs:
//   c_k = s_k sum_n x_n cos(pi k (2n+1) / (2N)), s_0 = sqrt(1/N), s_k = sqrt(2/N).
[[nodiscard]] inline std::vector<double> dctII(const std::vector<double>& x, std::size_t count) {
  const std::size_t n = x.size();
  count = std::min(count, n);
  std::vector<double> c(count, 0.0);
  if (n == 0) return c;
  const double dn = static_cast<double>(n);
  for (std::size_t k = 0; k < count; ++k) {
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i)
      s += x[i] * std::cos(std::numbers::pi * static_cast<double>(k) *
                           (2.0 * static_cast<double>(i) + 1.0) / (2.0 * dn));
    c[k] = s * (k == 0 ? std::sqrt(1.0 / dn) : std::sqrt(2.0 / dn));
  }
  return c;
}
// Its exact inverse (orthonormal DCT-III) to length n; missing
// coefficients are zero (a smoothed log-mel envelope).
[[nodiscard]] inline std::vector<double> dctIII(const std::vector<double>& c, std::size_t n) {
  std::vector<double> x(n, 0.0);
  if (n == 0) return x;
  const double dn = static_cast<double>(n);
  for (std::size_t i = 0; i < n; ++i) {
    double s = 0.0;
    for (std::size_t k = 0; k < c.size() && k < n; ++k)
      s += c[k] * (k == 0 ? std::sqrt(1.0 / dn) : std::sqrt(2.0 / dn)) *
           std::cos(std::numbers::pi * static_cast<double>(k) *
                    (2.0 * static_cast<double>(i) + 1.0) / (2.0 * dn));
    x[i] = s;
  }
  return x;
}

struct MfccOptions {
  std::size_t frameSize = 512;  // FFT size (power of two); 32 ms at 16 kHz
  std::size_t hop = 160;        // 10 ms at 16 kHz
  std::size_t numFilters = 26;
  std::size_t numCoeffs = 13;   // including c0
  double fMinHz = 0.0;
  double fMaxHz = 0.0;          // 0: fs/2
  double preEmphasis = 0.97;    // 0 disables
  MfccWindow window = MfccWindow::Hamming;
  bool powerSpectrum = true;    // false: magnitude (HTK default)
  MelScale scale = MelScale::Htk;
  MelNorm norm = MelNorm::None;
  double lifter = 22.0;         // HTK CEPLIFTER; 0 disables
  double logFloor = 1e-10;      // log(max(energy, floor))
};

// Log mel energies of one windowed spectrum's power/magnitude values.
[[nodiscard]] inline std::vector<double> logMelEnergies(const MelFilterbank& fb,
                                                        const std::vector<double>& spectrum,
                                                        double logFloor) {
  std::vector<double> e(fb.weights.size());
  for (std::size_t m = 0; m < fb.weights.size(); ++m) {
    double s = 0.0;
    for (std::size_t k = 0; k < spectrum.size() && k < fb.weights[m].size(); ++k)
      s += fb.weights[m][k] * spectrum[k];
    e[m] = std::log(std::max(s, logFloor));
  }
  return e;
}

// HTK sinusoidal lifter, applied in place: c_n *= 1 + (L/2) sin(pi n / L).
inline void applyLifter(std::vector<double>& c, double l) {
  if (!(l > 0.0)) return;
  for (std::size_t n = 0; n < c.size(); ++n)
    c[n] *= 1.0 + (l / 2.0) * std::sin(std::numbers::pi * static_cast<double>(n) / l);
}

struct MfccResult {
  std::vector<std::vector<double>> coeffs;  // frames x numCoeffs
  std::vector<std::vector<double>> logMel;  // frames x numFilters
  MelFilterbank filterbank;
};

// Frames start at 0 and advance by hop; the last partial frame is
// zero-padded (frames = ceil(max(len - N, 0) / hop) + 1 for len > 0).
[[nodiscard]] inline Result<MfccResult, QuantityError> mfcc(const std::vector<double>& x,
                                                           SampleRateHz rate,
                                                           const MfccOptions& o = {}) {
  using R = Result<MfccResult, QuantityError>;
  if (x.empty() || o.hop < 1 || o.numCoeffs < 1 || o.numCoeffs > o.numFilters ||
      !(o.preEmphasis >= 0.0) || !(o.preEmphasis < 1.0) ||
      !(o.logFloor > 0.0) || !std::isfinite(o.logFloor) ||
      !(o.lifter >= 0.0) || !std::isfinite(o.lifter) ||
      !(o.fMaxHz >= 0.0) || !std::isfinite(o.fMaxHz))
    return R::failure(QuantityError::InvalidArgument);
  for (double v : x)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const double fMax = o.fMaxHz > 0.0 ? o.fMaxHz : rate.value() / 2.0;
  auto fbr = melFilterbank(o.numFilters, o.frameSize, rate, o.fMinHz, fMax, o.scale, o.norm);
  if (!fbr.ok()) return R::failure(fbr.error());
  MfccResult res;
  res.filterbank = std::move(fbr).value();
  const std::size_t n = o.frameSize;
  std::vector<double> y(x.size());
  y[0] = x[0];
  for (std::size_t i = 1; i < x.size(); ++i) y[i] = x[i] - o.preEmphasis * x[i - 1];
  std::vector<double> win(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double ph = 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n);
    win[i] = o.window == MfccWindow::Hamming ? 0.54 - 0.46 * std::cos(ph) : 0.5 - 0.5 * std::cos(ph);
  }
  const std::size_t frames = y.size() <= n ? 1 : (y.size() - n + o.hop - 1) / o.hop + 1;
  std::vector<std::complex<double>> buf(n);
  std::vector<double> spec(n / 2 + 1);
  for (std::size_t f = 0; f < frames; ++f) {
    const std::size_t start = f * o.hop;
    for (std::size_t i = 0; i < n; ++i)
      buf[i] = {start + i < y.size() ? y[start + i] * win[i] : 0.0, 0.0};
    detail::fftInPlace(buf, false);
    for (std::size_t k = 0; k <= n / 2; ++k)
      spec[k] = o.powerSpectrum ? std::norm(buf[k]) : std::abs(buf[k]);
    auto lm = logMelEnergies(res.filterbank, spec, o.logFloor);
    auto c = dctII(lm, o.numCoeffs);
    applyLifter(c, o.lifter);
    res.coeffs.push_back(std::move(c));
    res.logMel.push_back(std::move(lm));
  }
  return R::success(std::move(res));
}

// Regression deltas (HTK eq. 5.16), edges replicated:
//   d_t = sum_{k=1..w} k (c_{t+k} - c_{t-k}) / (2 sum k^2).
// Throws std::invalid_argument if the frames have different dimensions.
[[nodiscard]] inline std::vector<std::vector<double>> deltas(
    const std::vector<std::vector<double>>& c, std::size_t w = 2) {
  std::vector<std::vector<double>> d(c.size());
  if (c.empty()) return d;
  const std::size_t dim = c[0].size();
  for (const auto& f : c)
    if (f.size() != dim) throw std::invalid_argument("deltas: inconsistent frame dimensions");
  if (w == 0) return d;
  double den = 0.0;
  for (std::size_t k = 1; k <= w; ++k) den += 2.0 * static_cast<double>(k * k);
  const auto last = static_cast<std::ptrdiff_t>(c.size()) - 1;
  for (std::size_t t = 0; t < c.size(); ++t) {
    d[t].assign(dim, 0.0);
    for (std::size_t k = 1; k <= w; ++k) {
      const auto ti = static_cast<std::ptrdiff_t>(t), kk = static_cast<std::ptrdiff_t>(k);
      const auto& a = c[static_cast<std::size_t>(std::min(ti + kk, last))];
      const auto& b = c[static_cast<std::size_t>(std::max(ti - kk, std::ptrdiff_t{0}))];
      for (std::size_t j = 0; j < dim; ++j) d[t][j] += static_cast<double>(k) * (a[j] - b[j]);
    }
    for (auto& v : d[t]) v /= den;
  }
  return d;
}

// Cepstral mean normalisation (per coefficient, over all frames), in place.
// Throws std::invalid_argument on inconsistent dimensions, before modifying c.
inline void cepstralMeanNormalize(std::vector<std::vector<double>>& c) {
  if (c.empty()) return;
  for (const auto& f : c)
    if (f.size() != c[0].size())
      throw std::invalid_argument("cepstralMeanNormalize: inconsistent frame dimensions");
  std::vector<double> mean(c[0].size(), 0.0);
  for (const auto& f : c)
    for (std::size_t j = 0; j < mean.size(); ++j) mean[j] += f[j];
  for (auto& m : mean) m /= static_cast<double>(c.size());
  for (auto& f : c)
    for (std::size_t j = 0; j < mean.size(); ++j) f[j] -= mean[j];
}

}  // namespace mmathen
