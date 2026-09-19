#pragma once

// D23 — digital filter design: the remaining RBJ sections, bilinear
// first-order sections, Butterworth and Linkwitz-Riley cascades, complex
// response analysis, and a fixed-capacity biquad cascade for realtime use.
//
// RBJ Audio EQ Cookbook sections (w0 = 2 pi f0/fs, c = cos w0,
// alpha = sin w0 / (2Q), all divided by a0 = 1 + alpha):
//   band-pass (constant 0 dB peak): b = {alpha, 0, -alpha}
//       |H(f0)| = 1, |H(0)| = |H(fs/2)| = 0
//   notch:     b = {1, -2c, 1}          |H(f0)| = 0, |H(0)| = |H(fs/2)| = 1
//   all-pass:  b = {1 - alpha, -2c, 1 + alpha}   |H(f)| = 1 for every f,
//       phase -pi at f0
//   denominators: a = {1 + alpha, -2c, 1 - alpha}.
// First-order bilinear sections, K = tan(pi fc/fs) (prewarped):
//   low-pass  b = {K, K}/(1+K),  high-pass b = {1, -1}/(1+K),
//   a1 = (K - 1)/(K + 1);  |H(fc)| = 1/sqrt(2) exactly.
//
// Butterworth of order N (1..8) is the bilinear transform of the analog
// prototype, prewarped at fc: floor(N/2) RBJ sections with
//   Q_k = 1 / (2 sin((2k - 1) pi / (2N))),  k = 1 .. floor(N/2),
// plus one first-order section when N is odd. Because every section shares
// the same prewarping, the cascade's magnitude is EXACTLY
//   |H(f)|^2 = 1 / (1 + (tan(pi f/fs) / tan(pi fc/fs))^(2N))   (low-pass)
//   |H(f)|^2 = 1 / (1 + (tan(pi fc/fs) / tan(pi f/fs))^(2N))   (high-pass)
// which the tests check at many frequencies for every order (maximally
// flat, -3.0103 dB at fc, 6N dB/octave asymptotically in the analog
// variable tan(pi f/fs)).
// Linkwitz-Riley LR(2N) is the Butterworth-N cascade applied twice
// (orders 2, 4, 8 here, from N = 1, 2, 4): -6.02 dB at fc, and for N even
// (LR4, LR8) LP + HP is an all-pass, |H_LP + H_HP| = 1 at every f: a
// crossover whose bands sum flat. LR2 sums flat only with the HP inverted
// (|H_LP - H_HP| = 1); both are tested.

#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "biquad.h"

namespace mmathen {

namespace detail {
inline bool validFilterArgs(double f0, double q, double fs) {
  return std::isfinite(f0) && std::isfinite(q) && std::isfinite(fs) &&
         fs > 0.0 && f0 > 0.0 && f0 < fs / 2.0 && q > 0.0;
}
inline Result<BiquadCoeffs, QuantityError> rbjCommon(double f0, double q,
                                                     double fs, int kind) {
  using R = Result<BiquadCoeffs, QuantityError>;
  if (!validFilterArgs(f0, q, fs))
    return R::failure(QuantityError::InvalidArgument);
  if (!(q > 0.0)) return R::failure(QuantityError::InvalidArgument);  // C4723
  const double w0 = 2.0 * std::numbers::pi * f0 / fs;
  const double c = std::cos(w0);
  const double alpha = std::sin(w0) / (2.0 * q);
  const double a0 = 1.0 + alpha;
  BiquadCoeffs k;
  if (kind == 0) {  // band-pass, 0 dB peak
    k = {alpha / a0, 0.0, -alpha / a0, -2.0 * c / a0, (1.0 - alpha) / a0};
  } else if (kind == 1) {  // notch
    k = {1.0 / a0, -2.0 * c / a0, 1.0 / a0, -2.0 * c / a0,
         (1.0 - alpha) / a0};
  } else {  // all-pass
    k = {(1.0 - alpha) / a0, -2.0 * c / a0, (1.0 + alpha) / a0,
         -2.0 * c / a0, (1.0 - alpha) / a0};
  }
  if (!finiteBiquad(k)) return R::failure(QuantityError::RangeError);
  return R::success(k);
}
}  // namespace detail

[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeBandpass(
    FrequencyHz center, double q, SampleRateHz rate) {
  return detail::rbjCommon(center.value(), q, rate.value(), 0);
}
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeNotch(
    FrequencyHz center, double q, SampleRateHz rate) {
  return detail::rbjCommon(center.value(), q, rate.value(), 1);
}
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeAllpass(
    FrequencyHz center, double q, SampleRateHz rate) {
  return detail::rbjCommon(center.value(), q, rate.value(), 2);
}

// First-order bilinear low/high-pass as a biquad (b2 = a2 = 0).
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeFirstOrderLowpass(
    FrequencyHz cutoff, SampleRateHz rate) {
  using R = Result<BiquadCoeffs, QuantityError>;
  const double fc = cutoff.value(), fs = rate.value();
  if (!detail::validFilterArgs(fc, 1.0, fs))
    return R::failure(QuantityError::InvalidArgument);
  const double kk = std::tan(std::numbers::pi * fc / fs);
  BiquadCoeffs k{kk / (1.0 + kk), kk / (1.0 + kk), 0.0,
                 (kk - 1.0) / (kk + 1.0), 0.0};
  if (!detail::finiteBiquad(k)) return R::failure(QuantityError::RangeError);
  return R::success(k);
}
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError>
makeFirstOrderHighpass(FrequencyHz cutoff, SampleRateHz rate) {
  using R = Result<BiquadCoeffs, QuantityError>;
  const double fc = cutoff.value(), fs = rate.value();
  if (!detail::validFilterArgs(fc, 1.0, fs))
    return R::failure(QuantityError::InvalidArgument);
  const double kk = std::tan(std::numbers::pi * fc / fs);
  BiquadCoeffs k{1.0 / (1.0 + kk), -1.0 / (1.0 + kk), 0.0,
                 (kk - 1.0) / (kk + 1.0), 0.0};
  if (!detail::finiteBiquad(k)) return R::failure(QuantityError::RangeError);
  return R::success(k);
}

// Section Q of an order-N Butterworth, k = 1 .. N/2.
[[nodiscard]] inline double butterworthQ(int order, int k) {
  return 1.0 / (2.0 * std::sin((2.0 * k - 1.0) * std::numbers::pi /
                               (2.0 * order)));
}

// Butterworth low/high-pass of order 1..8 as a cascade of sections.
[[nodiscard]] inline Result<std::vector<BiquadCoeffs>, QuantityError>
designButterworth(bool highpass, int order, FrequencyHz cutoff,
                  SampleRateHz rate) {
  using R = Result<std::vector<BiquadCoeffs>, QuantityError>;
  if (order < 1 || order > 8) return R::failure(QuantityError::InvalidArgument);
  std::vector<BiquadCoeffs> out;
  for (int k = 1; k <= order / 2; ++k) {
    auto s = highpass ? makeHighpass(cutoff, butterworthQ(order, k), rate)
                      : makeLowpass(cutoff, butterworthQ(order, k), rate);
    if (!s.ok()) return R::failure(s.error());
    out.push_back(s.value());
  }
  if (order % 2 == 1) {
    auto s = highpass ? makeFirstOrderHighpass(cutoff, rate)
                      : makeFirstOrderLowpass(cutoff, rate);
    if (!s.ok()) return R::failure(s.error());
    out.push_back(s.value());
  }
  return R::success(std::move(out));
}

// Linkwitz-Riley of order 2, 4 or 8: the Butterworth order/2 cascade twice.
[[nodiscard]] inline Result<std::vector<BiquadCoeffs>, QuantityError>
designLinkwitzRiley(bool highpass, int order, FrequencyHz cutoff,
                    SampleRateHz rate) {
  using R = Result<std::vector<BiquadCoeffs>, QuantityError>;
  if (order != 2 && order != 4 && order != 8)
    return R::failure(QuantityError::InvalidArgument);
  auto half = designButterworth(highpass, order / 2, cutoff, rate);
  if (!half.ok()) return R::failure(half.error());
  std::vector<BiquadCoeffs> out = half.value();
  out.insert(out.end(), half.value().begin(), half.value().end());
  return R::success(std::move(out));
}

// Complex response H(e^{jw}) of one section at f in [0, fs/2].
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
transferResponse(const BiquadCoeffs& k, FrequencyHz frequency,
                 SampleRateHz rate) {
  using R = Result<std::complex<double>, QuantityError>;
  const double f = frequency.value(), fs = rate.value();
  if (!detail::finiteBiquad(k) || !std::isfinite(f) || !std::isfinite(fs) ||
      !(fs > 0.0) || f < 0.0 || f > fs / 2.0)
    return R::failure(QuantityError::InvalidArgument);
  const std::complex<double> z1 =
      std::polar(1.0, -2.0 * std::numbers::pi * f / fs);
  const std::complex<double> z2 = z1 * z1;
  const std::complex<double> num = k.b0 + k.b1 * z1 + k.b2 * z2;
  const std::complex<double> den = 1.0 + k.a1 * z1 + k.a2 * z2;
  if (!(std::abs(den) > 0.0)) return R::failure(QuantityError::DomainError);
  return R::success(num / den);
}

// Product of the sections' responses.
[[nodiscard]] inline Result<std::complex<double>, QuantityError>
cascadeResponse(const std::vector<BiquadCoeffs>& sections, FrequencyHz f,
                SampleRateHz rate) {
  using R = Result<std::complex<double>, QuantityError>;
  std::complex<double> h{1.0, 0.0};
  for (const auto& k : sections) {
    auto r = transferResponse(k, f, rate);
    if (!r.ok()) return R::failure(r.error());
    h *= r.value();
  }
  return R::success(h);
}

// Magnitude in dB; an exact zero reports kCascadeFloorDb (a stated floor).
inline constexpr double kCascadeFloorDb = -400.0;
[[nodiscard]] inline Result<double, QuantityError> cascadeResponseDb(
    const std::vector<BiquadCoeffs>& sections, FrequencyHz f,
    SampleRateHz rate) {
  using R = Result<double, QuantityError>;
  auto h = cascadeResponse(sections, f, rate);
  if (!h.ok()) return R::failure(h.error());
  const double m = std::abs(h.value());
  return R::success(m > 0.0 ? 20.0 * std::log10(m) : kCascadeFloorDb);
}

// A series cascade with a capacity fixed at creation: after create(), every
// operation is allocation-free, so sections can be swapped on an audio
// thread. Unused slots are identity sections (b0 = 1). setSections keeps
// the state of every slot (Biquad::setCoeffs), so a live change continues
// the signal; slots that become identity pass it through unchanged.
class BiquadCascade {
 public:
  // capacity in [1, 256]; every section must be stable.
  [[nodiscard]] static Result<BiquadCascade, QuantityError> create(
      std::size_t capacity) {
    using R = Result<BiquadCascade, QuantityError>;
    if (capacity < 1 || capacity > 256)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(BiquadCascade{capacity});
  }

  // Load n sections (n <= capacity, all stable); the rest become identity.
  // On error nothing changes.
  [[nodiscard]] Result<std::size_t, QuantityError> setSections(
      const BiquadCoeffs* sections, std::size_t n) {
    using R = Result<std::size_t, QuantityError>;
    if (n > slots_.size() || (n > 0 && sections == nullptr))
      return R::failure(QuantityError::InvalidArgument);
    for (std::size_t i = 0; i < n; ++i)
      if (!isStable(sections[i])) return R::failure(QuantityError::DomainError);
    for (std::size_t i = 0; i < slots_.size(); ++i)
      slots_[i].setCoeffs(i < n ? sections[i] : BiquadCoeffs{});
    active_ = n;
    return R::success(n);
  }

  // Series processing into caller buffers (may alias). With no active
  // sections the input is copied.
  void processBlock(const double* in, double* out, FrameCount n) {
    if (active_ == 0) {
      const auto count = static_cast<std::size_t>(n.value());
      if (in != out)
        for (std::size_t i = 0; i < count; ++i) out[i] = in[i];
      return;
    }
    slots_[0].processBlock(in, out, n);
    for (std::size_t i = 1; i < active_; ++i)
      slots_[i].processBlock(out, out, n);
  }

  void reset() noexcept {
    for (auto& s : slots_) s.reset();
  }
  [[nodiscard]] std::size_t capacity() const noexcept { return slots_.size(); }
  [[nodiscard]] std::size_t size() const noexcept { return active_; }

 private:
  explicit BiquadCascade(std::size_t capacity)
      : slots_(capacity, Biquad{BiquadCoeffs{}}) {}
  std::vector<Biquad> slots_;
  std::size_t active_ = 0;
};

}  // namespace mmathen
