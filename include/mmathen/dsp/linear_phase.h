#pragma once

// D37 — linear-phase EQ and mid/side.
//
// Linear-phase FIR from any magnitude response (frequency sampling):
//   sample the target |H(f)| on a dense grid (4x the tap count, power of
//   two), give it zero phase, inverse FFT (a real, even impulse response),
//   rotate it to be centred, keep L = taps (odd) samples around the centre
//   and apply a Blackman window. The result is exactly symmetric, so its
//   group delay is exactly (L - 1)/2 samples at every frequency: the EQ
//   changes no phase relations (no "phase smear"), at the price of that
//   latency and of pre-ringing. The magnitude matches the target within
//   the window's resolution (tested: within 0.1 dB above ~4 fs/L).
//   linearPhaseFromSections builds the target from an EQ's biquad cascade
//   (D23 cascadeResponseDb), i.e. "the same EQ curve, linear phase".
//
// FftConvolver: streaming FFT overlap-add convolution, block B (power of
// two, B >= L - 1), latency B samples (plus the FIR's own (L-1)/2 group
// delay). Allocation-free after creation. Equals direct convolution to
// roundoff (tested).
//
// Mid/side: M = (L + R)/2, S = (L - R)/2; L = M + S, R = M - S. Round trip
// to 1 ulp-scale roundoff. Processing only S changes the stereo width and
// leaves any mono content (L = R) untouched: tested.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <functional>
#include <numbers>
#include <utility>
#include <vector>

#include "../analysis/fft.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "filters.h"

namespace mmathen {

// Linear-phase FIR of `taps` (odd, 17..65537) matching magnitudeDb(f).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> designLinearPhaseFir(
    const std::function<double(double)>& magnitudeDb, SampleRateHz rate, std::size_t taps) {
  using R = Result<std::vector<double>, QuantityError>;
  const double fs = rate.value();
  if (taps < 17 || taps > 65537 || taps % 2 == 0 || !std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  std::size_t n = 1;
  while (n < 4 * taps) n *= 2;
  std::vector<std::complex<double>> h(n);
  for (std::size_t k = 0; k <= n / 2; ++k) {
    const double db = magnitudeDb(static_cast<double>(k) * fs / static_cast<double>(n));
    if (!std::isfinite(db)) return R::failure(QuantityError::InvalidArgument);
    h[k] = std::pow(10.0, db / 20.0);
    if (k > 0 && k < n / 2) h[n - k] = h[k];
  }
  detail::fftInPlace(h, true);
  const std::size_t mid = taps / 2;
  std::vector<double> fir(taps);
  for (std::size_t i = 0; i < taps; ++i) {
    // Circular index centred on 0.
    const std::size_t src = (i + n - mid) % n;
    const double u = 2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(taps - 1);
    const double w = 0.42 - 0.5 * std::cos(u) + 0.08 * std::cos(2.0 * u);  // Blackman
    fir[i] = h[src].real() * w;
  }
  // Enforce exact symmetry (roundoff in the IFFT is not symmetric).
  for (std::size_t i = 0; i < mid; ++i) {
    const double a = 0.5 * (fir[i] + fir[taps - 1 - i]);
    fir[i] = fir[taps - 1 - i] = a;
  }
  return R::success(std::move(fir));
}

// The magnitude of a biquad cascade, as a linear-phase FIR.
[[nodiscard]] inline Result<std::vector<double>, QuantityError> linearPhaseFromSections(
    const std::vector<BiquadCoeffs>& sections, SampleRateHz rate, std::size_t taps) {
  const double fs = rate.value();
  return designLinearPhaseFir(
      [&](double f) {
        auto db = cascadeResponseDb(sections, FrequencyHz{std::min(f, 0.5 * fs)}, rate);
        return db.ok() ? std::max(db.value(), -120.0) : -120.0;
      },
      rate, taps);
}

class FftConvolver {
 public:
  [[nodiscard]] static Result<FftConvolver, QuantityError> create(
      const std::vector<double>& fir, std::size_t block) {
    using R = Result<FftConvolver, QuantityError>;
    if (fir.empty() || block < 16 || !isPowerOfTwo(block) || fir.size() > block + 1)
      return R::failure(QuantityError::InvalidArgument);
    FftConvolver c;
    c.b_ = block;
    c.h_.assign(2 * block, {0.0, 0.0});
    for (std::size_t i = 0; i < fir.size(); ++i) c.h_[i] = fir[i];
    detail::fftInPlace(c.h_, false);
    c.buf_.assign(2 * block, {0.0, 0.0});
    c.in_.assign(block, 0.0);
    c.out_.assign(block, 0.0);
    c.tail_.assign(block, 0.0);
    return R::success(std::move(c));
  }

  // Streaming; in and out may alias. Latency: block samples.
  void process(const double* in, double* out, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) {
      const double x = in[i];
      out[i] = out_[fill_];
      in_[fill_] = x;
      if (++fill_ == b_) {
        fill_ = 0;
        block();
      }
    }
  }

  [[nodiscard]] std::size_t latencyFrames() const noexcept { return b_; }

 private:
  FftConvolver() = default;
  void block() {
    for (std::size_t i = 0; i < 2 * b_; ++i) buf_[i] = i < b_ ? in_[i] : 0.0;
    detail::fftInPlace(buf_, false);
    for (std::size_t i = 0; i < 2 * b_; ++i) buf_[i] *= h_[i];
    detail::fftInPlace(buf_, true);
    for (std::size_t i = 0; i < b_; ++i) {
      out_[i] = buf_[i].real() + tail_[i];
      tail_[i] = buf_[i + b_].real();
    }
  }
  std::size_t b_ = 0, fill_ = 0;
  std::vector<std::complex<double>> h_, buf_;
  std::vector<double> in_, out_, tail_;
};

// Mid/side encode and decode (in place allowed).
inline void midSideEncode(const double* l, const double* r, double* m, double* s,
                          std::size_t n) noexcept {
  for (std::size_t i = 0; i < n; ++i) {
    const double a = l[i], b = r[i];
    m[i] = 0.5 * (a + b);
    s[i] = 0.5 * (a - b);
  }
}
inline void midSideDecode(const double* m, const double* s, double* l, double* r,
                          std::size_t n) noexcept {
  for (std::size_t i = 0; i < n; ++i) {
    const double a = m[i], b = s[i];
    l[i] = a + b;
    r[i] = a - b;
  }
}

}  // namespace mmathen
