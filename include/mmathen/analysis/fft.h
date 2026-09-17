#pragma once

// D11 — discrete Fourier transform and precise sinusoid measurement.
//
// fft/ifft: iterative radix-2 transform (bit reversal + in-place butterflies)
// on power-of-two lengths, with the conventions
//   X[k] = sum_n x[n] e^{-2 pi i k n / N},   x[n] = (1/N) sum_k X[k] e^{+...}
// so Parseval reads sum |x|^2 = (1/N) sum |X|^2. Verified against a naive
// O(N^2) DFT written independently in the tests.
//
// Windows are periodic ("DFT-even", Harris 1978, Proc. IEEE 66(1)): Hann and
// the 4-term Blackman-Harris (a = 0.35875, 0.48829, 0.14128, 0.01168; highest
// sidelobe about -92 dB, main lobe +-4 bins).
//
// estimateSinusoid measures ONE sinusoid's frequency, amplitude and phase
// from finite samples: a coarse FFT peak inside the caller's search band,
// then the stationary point of the windowed DTFT power
// |X(f)|^2, X(f) = sum w[n] x[n] e^{-2 pi i f n/fs}, over the main lobe
// around that bin: bisection on the sign of d|X|^2/df = 2 Re(conj(X) X'),
// X'(f) = sum w[n] x[n] (-2 pi i n/fs) e^{...}. (A first version maximized
// |X| by golden section; on the flat peak that only resolves f to about
// sqrt(eps) of the lobe width — measured 8.6e-7 Hz on a 1e-6 Hz test — so
// the derivative root, which bisects to machine precision, replaced it.) The
// peak of the windowed DTFT is the sinusoid's frequency up to leakage from
// its negative-frequency image and from other components; with
// Blackman-Harris and components several bins apart that bias sits at the
// -92 dB sidelobe level. The tests state the achieved accuracy (single tone
// at 440 Hz: 7e-7 Hz at N = 8192, 1.1e-7 Hz at N = 65536; residual source
// not isolated, size asserted below 1e-5 cents). Amplitude =
// 2|X(f)|/sum w; phase is referenced to sample 0. A maximum at the edge of
// the search band is not a peak: DomainError. Main-lobe check: a real
// sinusoid's windowed spectrum at +-halfLobe bins from its peak falls to the
// sidelobe floor (-92 dB Blackman-Harris; exactly 0 for Hann at +-2 bins and
// rectangular at +-1 bin), while a sidelobe ripple of some distant component
// does not. If either side exceeds a window-specific fraction of the peak
// (-60 dB BH, -20 dB Hann, -6 dB rectangular) the maximum is not an isolated sinusoid
// (a sidelobe, or components closer than the main lobe): DomainError.
// Found necessary in testing — without it a band containing no tone
// returned a sidelobe ripple as a "sinusoid". Noise-free measurement only;
// no statistical estimator is claimed.

#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"

namespace mmathen {

inline constexpr std::size_t kMaxFftSize = std::size_t{1} << 24;
// NOTE (RAM bound, cf. kMaxRenderFrames): a full-size transform holds three
// buffers of 16M complex doubles (~256 MB each with temporaries), so large-N
// renders are format-legal but memory-bound on ordinary machines.

[[nodiscard]] inline bool isPowerOfTwo(std::size_t n) noexcept {
  return n != 0 && (n & (n - 1)) == 0;
}

namespace detail {
inline void fftInPlace(std::vector<std::complex<double>>& a, bool inverse) {
  const std::size_t n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; (j & bit) != 0; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1) {
    const double angle =
        (inverse ? 1.0 : -1.0) * constants::twoPi / static_cast<double>(len);
    for (std::size_t start = 0; start < n; start += len) {
      for (std::size_t k = 0; k < len / 2; ++k) {
        // Direct trig per butterfly (no accumulated rotation drift).
        const std::complex<double> w{
            std::cos(angle * static_cast<double>(k)),
            std::sin(angle * static_cast<double>(k))};
        const std::complex<double> u = a[start + k];
        const std::complex<double> v = a[start + k + len / 2] * w;
        a[start + k] = u + v;
        a[start + k + len / 2] = u - v;
      }
    }
  }
  if (inverse)
    for (auto& z : a) z /= static_cast<double>(n);
}
}  // namespace detail

// Forward DFT. Length must be a power of two in [1, kMaxFftSize] and every
// value finite; otherwise InvalidArgument.
[[nodiscard]] inline Result<std::vector<std::complex<double>>, QuantityError>
fft(std::vector<std::complex<double>> data) {
  using R = Result<std::vector<std::complex<double>>, QuantityError>;
  if (!isPowerOfTwo(data.size()) || data.size() > kMaxFftSize)
    return R::failure(QuantityError::InvalidArgument);
  for (const auto& z : data)
    if (!std::isfinite(z.real()) || !std::isfinite(z.imag()))
      return R::failure(QuantityError::InvalidArgument);
  detail::fftInPlace(data, false);
  return R::success(std::move(data));
}

// Inverse DFT (includes the 1/N). Same validation as fft.
[[nodiscard]] inline Result<std::vector<std::complex<double>>, QuantityError>
ifft(std::vector<std::complex<double>> data) {
  using R = Result<std::vector<std::complex<double>>, QuantityError>;
  if (!isPowerOfTwo(data.size()) || data.size() > kMaxFftSize)
    return R::failure(QuantityError::InvalidArgument);
  for (const auto& z : data)
    if (!std::isfinite(z.real()) || !std::isfinite(z.imag()))
      return R::failure(QuantityError::InvalidArgument);
  detail::fftInPlace(data, true);
  return R::success(std::move(data));
}

enum class Window { Rectangular, Hann, BlackmanHarris };

// Periodic window of length n >= 1 (n == 0: InvalidArgument).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> windowCoefficients(
    Window window, std::size_t n) {
  using R = Result<std::vector<double>, QuantityError>;
  if (n == 0 || n > kMaxFftSize) return R::failure(QuantityError::InvalidArgument);
  std::vector<double> w(n, 1.0);
  const double nn = static_cast<double>(n);
  for (std::size_t i = 0; i < n; ++i) {
    const double x = constants::twoPi * static_cast<double>(i) / nn;
    switch (window) {
      case Window::Rectangular:
        break;
      case Window::Hann:
        w[i] = 0.5 - 0.5 * std::cos(x);
        break;
      case Window::BlackmanHarris:
        w[i] = 0.35875 - 0.48829 * std::cos(x) + 0.14128 * std::cos(2.0 * x) -
               0.01168 * std::cos(3.0 * x);
        break;
    }
  }
  return R::success(std::move(w));
}

struct SinusoidEstimate {
  FrequencyHz frequency;
  double amplitude;
  PhaseRadians phase;  // of cos(2 pi f n/fs + phase) at sample 0
};

namespace detail {
// Windowed DTFT at frequency f (Hz).
inline std::complex<double> windowedDtft(const std::vector<double>& x,
                                         const std::vector<double>& w,
                                         double f, double fs) {
  const double omega = constants::twoPi * f / fs;
  double re = 0.0;
  double im = 0.0;
  for (std::size_t n = 0; n < x.size(); ++n) {
    const double v = w[n] * x[n];
    const double a = omega * static_cast<double>(n);
    re += v * std::cos(a);
    im -= v * std::sin(a);
  }
  return {re, im};
}
}  // namespace detail

// Measure the strongest sinusoid with frequency in [fMin, fMax]. samples:
// power-of-two length >= 16, finite; rate > 0; 0 <= fMin < fMax <= fs/2.
// Coarse peak = largest windowed-FFT bin inside the band; refinement =
// derivative-root bisection over +-(halfLobe - 1/2) bins (halfLobe = 4
// Blackman-Harris, 2 Hann, 1 rectangular), clipped to the band. If the derivative does not change
// sign across that interval there is no interior peak: DomainError.
[[nodiscard]] inline Result<SinusoidEstimate, QuantityError> estimateSinusoid(
    const std::vector<double>& samples, SampleRateHz rate, Window window,
    FrequencyHz fMin, FrequencyHz fMax) {
  using R = Result<SinusoidEstimate, QuantityError>;
  const std::size_t n = samples.size();
  const double fs = rate.value();
  const double lo = fMin.value();
  const double hi = fMax.value();
  if (n < 16 || !isPowerOfTwo(n) || n > kMaxFftSize)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(lo) || !std::isfinite(hi) || lo < 0.0 || !(lo < hi) ||
      hi > fs / 2.0)
    return R::failure(QuantityError::InvalidArgument);
  for (const double v : samples)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);

  auto w = windowCoefficients(window, n);
  if (!w.ok()) return R::failure(w.error());
  const std::vector<double>& win = w.value();
  std::vector<std::complex<double>> buf(n);
  for (std::size_t i = 0; i < n; ++i) buf[i] = win[i] * samples[i];
  detail::fftInPlace(buf, false);

  const double binHz = fs / static_cast<double>(n);
  const auto firstBin = static_cast<std::size_t>(std::ceil(lo / binHz));
  auto lastBin = static_cast<std::size_t>(std::floor(hi / binHz));
  if (lastBin > n / 2) lastBin = n / 2;
  if (firstBin > lastBin) return R::failure(QuantityError::DomainError);
  std::size_t peak = firstBin;
  double best = -1.0;
  for (std::size_t k = firstBin; k <= lastBin; ++k) {
    const double m = std::abs(buf[k]);
    if (m > best) {
      best = m;
      peak = k;
    }
  }
  if (!(best > 0.0)) return R::failure(QuantityError::DomainError);

  const double halfLobe = window == Window::BlackmanHarris ? 4.0
                          : window == Window::Hann         ? 2.0
                                                           : 1.0;
  // Bracket half a bin inside the main-lobe nulls, where |X|^2 has minima
  // and the slope's sign is roundoff. The true peak lies within half a bin
  // of the largest bin, so it stays inside.
  double a = (static_cast<double>(peak) - (halfLobe - 0.5)) * binHz;
  double b = (static_cast<double>(peak) + (halfLobe - 0.5)) * binHz;
  if (a < lo) a = lo;
  if (b > hi) b = hi;
  auto dtftAndSlope = [&](double f, std::complex<double>& x,
                          std::complex<double>& dx) {
    const double omega = constants::twoPi * f / fs;
    const double scale = constants::twoPi / fs;
    double re = 0.0, im = 0.0, dre = 0.0, dim = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      const double v = win[i] * samples[i];
      const double ang = omega * static_cast<double>(i);
      const double co = std::cos(ang);
      const double si = std::sin(ang);
      re += v * co;
      im -= v * si;
      // d/df of v e^{-i ang} = v (-i 2 pi i_n / fs) e^{-i ang}
      const double k = scale * static_cast<double>(i) * v;
      dre += k * (-si);  // Re(-i e^{-i ang}) = -sin(ang)
      dim += k * (-co);  // Im(-i e^{-i ang}) = -cos(ang)
    }
    x = {re, im};
    dx = {dre, dim};
  };
  auto slope = [&](double f) {
    std::complex<double> x, dx;
    dtftAndSlope(f, x, dx);
    return (std::conj(x) * dx).real();
  };
  auto mag = [&](double f) {
    return std::abs(detail::windowedDtft(samples, win, f, fs));
  };
  if (!(slope(a) > 0.0) || !(slope(b) < 0.0))
    return R::failure(QuantityError::DomainError);
  for (int it = 0; it < 200; ++it) {
    const double mid = 0.5 * (a + b);
    if (!(mid > a) || !(mid < b)) break;  // interval at machine resolution
    if (slope(mid) > 0.0)
      a = mid;
    else
      b = mid;
  }
  const double f = 0.5 * (a + b);
  const double lobeRatio = window == Window::BlackmanHarris ? 1e-3
                           : window == Window::Hann         ? 0.1
                                                            : 0.5;
  const double peakMag = mag(f);
  const double lowSide = f - halfLobe * binHz;
  const double highSide = f + halfLobe * binHz;
  if ((lowSide > 0.0 && mag(lowSide) > lobeRatio * peakMag) ||
      (highSide < fs / 2.0 && mag(highSide) > lobeRatio * peakMag))
    return R::failure(QuantityError::DomainError);
  double wsum = 0.0;
  for (const double v : win) wsum += v;
  const std::complex<double> x = detail::windowedDtft(samples, win, f, fs);
  return R::success(SinusoidEstimate{FrequencyHz{f}, 2.0 * std::abs(x) / wsum,
                                     PhaseRadians{std::arg(x)}});
}

}  // namespace mmathen
