#pragma once

// D5 — biquad: the FIRST stateful processor, under the reserved contract.
//
// A biquad holds owned filter state (Direct Form 1 delay registers) and
// processes caller-supplied buffers sequentially: explicit construction
// from validated coefficients, reset() restores pristine state, and no
// backward seek exists (unsupported by construction, never silent reset).
// Coefficients follow the RBJ Audio EQ Cookbook (bilinear transform with
// prewarping; W3C normative version cross-checked): H(z) =
// (b0+b1 z^-1+b2 z^-2)/(a0+a1 z^-1+a2 z^-2), a0 normalized to 1.
// Declared latency is 0 samples (IIR has no block latency; group delay
// varies with frequency — use transferMagnitude for analysis, not a number
// here). Double precision throughout (32-bit accumulator guidance from
// fixed-point practice does not apply; NaN input poisons state and is the
// caller's to prevent — render stats detect it, reset() clears it).

#include <cmath>
#include <cstdint>
#include <numbers>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

struct BiquadCoeffs {
  double b0 = 1.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double a1 = 0.0;
  double a2 = 0.0;

  bool operator==(const BiquadCoeffs&) const = default;
};

namespace detail {
inline bool finiteBiquad(const BiquadCoeffs& c) {
  return std::isfinite(c.b0) && std::isfinite(c.b1) && std::isfinite(c.b2) &&
         std::isfinite(c.a1) && std::isfinite(c.a2);
}
}  // namespace detail

// RBJ lowpass: b = {(1-cos)/2, 1-cos, (1-cos)/2}, a0 = 1+alpha,
// a1 = -2cos, a2 = 1-alpha, alpha = sin/(2Q), w0 = 2*pi*fc/fs.
// Requires 0 < fc < fs/2 (strictly below Nyquist), Q > 0, fs > 0.
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeLowpass(
    FrequencyHz cutoff, double resonanceQ, SampleRateHz rate) {
  using R = Result<BiquadCoeffs, QuantityError>;
  const double fc = cutoff.value();
  const double fs = rate.value();
  if (!std::isfinite(fc) || !std::isfinite(resonanceQ) || !std::isfinite(fs))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fc > 0.0) || !(fc < fs / 2.0) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!(resonanceQ > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double w0 = 2.0 * std::numbers::pi * fc / fs;
  const double c = std::cos(w0);
  const double s = std::sin(w0);
  const double alpha = s / (2.0 * resonanceQ);
  const double a0 = 1.0 + alpha;
  BiquadCoeffs k{(1.0 - c) / 2.0 / a0, (1.0 - c) / a0, (1.0 - c) / 2.0 / a0,
                  -2.0 * c / a0, (1.0 - alpha) / a0};
  if (!detail::finiteBiquad(k)) return R::failure(QuantityError::RangeError);
  return R::success(k);
}

// RBJ highpass: b = {(1+cos)/2, -(1+cos), (1+cos)/2}, same a's. Same rules.
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeHighpass(
    FrequencyHz cutoff, double resonanceQ, SampleRateHz rate) {
  using R = Result<BiquadCoeffs, QuantityError>;
  const double fc = cutoff.value();
  const double fs = rate.value();
  if (!std::isfinite(fc) || !std::isfinite(resonanceQ) || !std::isfinite(fs))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fc > 0.0) || !(fc < fs / 2.0) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!(resonanceQ > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double w0 = 2.0 * std::numbers::pi * fc / fs;
  const double c = std::cos(w0);
  const double s = std::sin(w0);
  const double alpha = s / (2.0 * resonanceQ);
  const double a0 = 1.0 + alpha;
  BiquadCoeffs k{(1.0 + c) / 2.0 / a0, -(1.0 + c) / a0, (1.0 + c) / 2.0 / a0,
                  -2.0 * c / a0, (1.0 - alpha) / a0};
  if (!detail::finiteBiquad(k)) return R::failure(QuantityError::RangeError);
  return R::success(k);
}

// Poles of 1 + a1 z^-1 + a2 z^-2 inside the unit circle: |a2| < 1 and
// |a1| < 1 + a2 (stability triangle for real second order). Non-finite
// coefficients report false (a predicate, not a judgment).
[[nodiscard]] inline bool isStable(const BiquadCoeffs& k) noexcept {
  if (!detail::finiteBiquad(k)) return false;
  if (!(k.a2 > -1.0) || !(k.a2 < 1.0)) return false;
  const double bound = 1.0 + k.a2;
  return k.a1 > -bound && k.a1 < bound;
}

// |H(e^{jw})| at frequency f (0..fs/2). Pure analysis: no state touched.
[[nodiscard]] inline Result<double, QuantityError> transferMagnitude(
    const BiquadCoeffs& k, FrequencyHz frequency, SampleRateHz rate) {
  using R = Result<double, QuantityError>;
  const double f = frequency.value();
  const double fs = rate.value();
  if (!detail::finiteBiquad(k))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(f) || !std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (f < 0.0 || f > fs / 2.0)
    return R::failure(QuantityError::InvalidArgument);
  const double w = 2.0 * std::numbers::pi * f / fs;
  const double cw = std::cos(w);
  const double sw = std::sin(w);
  const double c2w = 2.0 * cw * cw - 1.0;
  const double s2w = 2.0 * sw * cw;
  const double reB = k.b0 + k.b1 * cw + k.b2 * c2w;
  const double imB = -(k.b1 * sw + k.b2 * s2w);
  const double reA = 1.0 + k.a1 * cw + k.a2 * c2w;
  const double imA = -(k.a1 * sw + k.a2 * s2w);
  const double den = reA * reA + imA * imA;
  if (!(den > 0.0) || !std::isfinite(den))
    return R::failure(QuantityError::DomainError);
  const double mag =
      std::sqrt((reB * reB + imB * imB) / den);
  if (!std::isfinite(mag)) return R::failure(QuantityError::RangeError);
  return R::success(mag);
}

class Biquad {
 public:
  explicit Biquad(BiquadCoeffs coeffs) : coeffs_(coeffs) { reset(); }

  void reset() noexcept {
    x1_ = 0.0;
    x2_ = 0.0;
    y1_ = 0.0;
    y2_ = 0.0;
  }

  [[nodiscard]] const BiquadCoeffs& coeffs() const noexcept { return coeffs_; }

  // Direct Form 1 into caller buffers (no allocation, bounded work, no
  // locks or I/O — realtime-suitable by construction). Buffers must be
  // valid for n frames; may alias (in == out).
  void processBlock(const double* in, double* out, FrameCount n) {
    const auto count = static_cast<std::uint64_t>(n.value());
    for (std::uint64_t i = 0; i < count; ++i) {
      const double x = in[i];
      const double y = coeffs_.b0 * x + coeffs_.b1 * x1_ + coeffs_.b2 * x2_ -
                       coeffs_.a1 * y1_ - coeffs_.a2 * y2_;
      x2_ = x1_;
      x1_ = x;
      y2_ = y1_;
      y1_ = y;
      out[i] = y;
    }
  }

 private:
  BiquadCoeffs coeffs_;
  double x1_ = 0.0;
  double x2_ = 0.0;
  double y1_ = 0.0;
  double y2_ = 0.0;
};

}  // namespace mmathen
