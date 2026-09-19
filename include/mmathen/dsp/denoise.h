#pragma once

// D34 — noise reduction and spectral repair on the D26 overlap-add engine.
//
// Noise profile: the mean power of every FFT bin over a noise-only stretch
// (frames of N, sqrt-Hann analysis window, hop N/4: the same analysis the
// denoiser uses, so the profile and the processed spectra are comparable).
//
// Per frame and bin k, with Y the noisy spectrum and L the profile power:
//   a posteriori SNR  gamma = |Y|^2 / L
//   a priori SNR      xi = a |A_prev|^2 / L + (1 - a) max(gamma - 1, 0),
//                     a = 0.98 (Ephraim & Malah's "decision-directed" rule),
//                     A_prev the previous frame's amplitude estimate;
//   gain G, by method:
//   - SpectralSubtraction (Boll 1979, power form, with Berouti et al.'s
//     1979 over-subtraction factor alpha = 4, stated):
//       G = sqrt(max(1 - alpha L/|Y|^2, 0))
//     (without over-subtraction, measured: only +4.9 dB SNR gain at 0 dB
//     input SNR, because a third of pure-noise bins exceed their mean);
//   - MmseStsa (Ephraim & Malah 1984): v = xi gamma / (1 + xi),
//       G = (sqrt(pi)/2) (sqrt(v)/gamma) exp(-v/2) [(1+v) I0(v/2) + v I1(v/2)]
//     evaluated with exponentially scaled Bessel functions (exp(-v/2) I(v/2)
//     never overflows);
//   - MmseLsa (Ephraim & Malah 1985, log-spectral amplitude):
//       G = xi/(1 + xi) exp(E1(v)/2), E1 the exponential integral.
//   Then G = max(G, floor), floor = 10^(-reductionDb/20): the reduction
//   amount. The floor bounds how far any bin is pulled down, which limits
//   "musical noise" and keeps the result natural.
// A reduction of 0 dB makes every gain exactly 1: the processor is then an
// exact N-sample delay (the OverlapAdd identity), which the tests pin.
// The noise profile is fixed (learned once); tracking a changing noise
// floor (minimum statistics etc.) is not implemented, stated.
//
// Special functions (used here; tested against series and tabulated
// values): i0e, i1e (Abramowitz & Stegun 9.8.1-9.8.4 polynomials, relative
// error about 2e-7) and E1 (series below 1, continued fraction above).
//
// Spectral repair: spectralGainRegion scales the bins of a time x frequency
// box by a gain (an "attenuate this area of the spectrogram" edit), through
// the same OverlapAdd engine, latency-compensated and length-preserving.
// Outside the box the signal passes through the exact OLA identity.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include "../analysis/fft.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "overlap_add.h"

namespace mmathen {

namespace special {
// exp(-|x|) I0(x)
[[nodiscard]] inline double i0e(double x) {
  const double ax = std::fabs(x);
  if (ax < 3.75) {
    const double t = (x / 3.75) * (x / 3.75);
    const double i0 =
        1.0 + t * (3.5156229 + t * (3.0899424 + t * (1.2067492 +
        t * (0.2659732 + t * (0.0360768 + t * 0.0045813)))));
    return i0 * std::exp(-ax);
  }
  const double y = 3.75 / ax;
  return (0.39894228 + y * (0.01328592 + y * (0.00225319 + y * (-0.00157565 +
          y * (0.00916281 + y * (-0.02057706 + y * (0.02635537 +
          y * (-0.01647633 + y * 0.00392377)))))))) / std::sqrt(ax);
}
// exp(-|x|) I1(x)
[[nodiscard]] inline double i1e(double x) {
  const double ax = std::fabs(x);
  double r;
  if (ax < 3.75) {
    const double t = (x / 3.75) * (x / 3.75);
    r = ax * (0.5 + t * (0.87890594 + t * (0.51498869 + t * (0.15084934 +
        t * (0.02658733 + t * (0.00301532 + t * 0.00032411)))))) * std::exp(-ax);
  } else {
    const double y = 3.75 / ax;
    r = (0.39894228 + y * (-0.03988024 + y * (-0.00362018 + y * (0.00163801 +
         y * (-0.01031555 + y * (0.02282967 + y * (-0.02895312 +
         y * (0.01787654 - y * 0.00420059)))))))) / std::sqrt(ax);
  }
  return x < 0.0 ? -r : r;
}
// Exponential integral E1(x), x > 0.
[[nodiscard]] inline double e1(double x) {
  if (!(x > 0.0)) return std::numeric_limits<double>::infinity();
  if (x <= 1.0) {
    constexpr double gamma = 0.57721566490153286060;
    double sum = 0.0, term = 1.0;
    for (int k = 1; k < 60; ++k) {
      term *= -x / k;
      const double add = -term / k;
      sum += add;
      if (std::fabs(add) < 1e-17 * std::fabs(sum)) break;
    }
    return -gamma - std::log(x) + sum;
  }
  // Continued fraction (modified Lentz), E1(x) = e^-x / (x + 1 - 1/(x + 3 - ...)).
  const double tiny = 1e-300;
  double b = x + 1.0, c = 1.0 / tiny, d = 1.0 / b, h = d;
  for (int i = 1; i < 200; ++i) {
    const double an = -static_cast<double>(i) * i;
    b += 2.0;
    d = 1.0 / (an * d + b);
    c = b + an / c;
    const double del = c * d;
    h *= del;
    if (std::fabs(del - 1.0) < 1e-16) break;
  }
  return h * std::exp(-x);
}
}  // namespace special

enum class DenoiseMethod { SpectralSubtraction, MmseStsa, MmseLsa };

struct NoiseProfile {
  std::size_t frameSize = 0;
  std::vector<double> power;  // bins 0..N/2
};

// Learn a profile from a noise-only stretch (at least one frame).
[[nodiscard]] inline Result<NoiseProfile, QuantityError> learnNoiseProfile(
    const std::vector<double>& noise, std::size_t frameSize = 2048) {
  using R = Result<NoiseProfile, QuantityError>;
  if (frameSize < 64 || frameSize > 65536 || !isPowerOfTwo(frameSize) ||
      noise.size() < frameSize)
    return R::failure(QuantityError::InvalidArgument);
  NoiseProfile p;
  p.frameSize = frameSize;
  p.power.assign(frameSize / 2 + 1, 0.0);
  std::vector<double> w(frameSize);
  for (std::size_t i = 0; i < frameSize; ++i)
    w[i] = std::sqrt(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                          static_cast<double>(frameSize)));
  std::vector<std::complex<double>> buf(frameSize);
  std::size_t frames = 0;
  for (std::size_t s = 0; s + frameSize <= noise.size(); s += frameSize / 4) {
    for (std::size_t i = 0; i < frameSize; ++i)
      buf[i] = std::complex<double>(noise[s + i] * w[i], 0.0);
    detail::fftInPlace(buf, false);
    for (std::size_t k = 0; k <= frameSize / 2; ++k) p.power[k] += std::norm(buf[k]);
    ++frames;
  }
  for (double& v : p.power) v /= static_cast<double>(frames);
  return R::success(std::move(p));
}

class SpectralDenoiser {
 public:
  // reductionDb in [0, 60]; the profile's frame size sets the latency.
  [[nodiscard]] static Result<SpectralDenoiser, QuantityError> create(
      NoiseProfile profile, DenoiseMethod method, double reductionDb) {
    using R = Result<SpectralDenoiser, QuantityError>;
    if (profile.frameSize < 64 || profile.power.size() != profile.frameSize / 2 + 1 ||
        !std::isfinite(reductionDb) || reductionDb < 0.0 || reductionDb > 60.0)
      return R::failure(QuantityError::InvalidArgument);
    for (double v : profile.power)
      if (!std::isfinite(v) || v < 0.0) return R::failure(QuantityError::InvalidArgument);
    auto ola = OverlapAdd::create(profile.frameSize, 4);
    if (!ola.ok()) return R::failure(ola.error());
    return R::success(SpectralDenoiser{std::move(profile), method, reductionDb,
                                       std::move(ola).value()});
  }

  // Streaming, allocation-free: in and out may alias. Latency frameSize.
  void process(const double* in, double* out, std::size_t n) {
    ola_.process(in, out, n, [this](std::vector<std::complex<double>>& bins) {
      frame(bins);
    });
  }

  void reset() {
    ola_.reset();
    std::fill(prevA2_.begin(), prevA2_.end(), 0.0);
    first_ = true;
  }
  [[nodiscard]] std::size_t latencyFrames() const noexcept { return ola_.latencyFrames(); }
  // The profile power as the processor sees it (scaled to its frames).
  [[nodiscard]] const NoiseProfile& profile() const noexcept { return profile_; }

  static constexpr double kOverSubtraction = 4.0;  // Berouti et al. 1979

  // One bin's gain for given a posteriori / a priori SNRs (exposed for tests).
  [[nodiscard]] static double gain(DenoiseMethod m, double gamma, double xi) {
    gamma = std::max(gamma, 1e-12);
    xi = std::max(xi, 1e-12);
    switch (m) {
      case DenoiseMethod::SpectralSubtraction:
        return std::sqrt(std::max(1.0 - kOverSubtraction / gamma, 0.0));
      case DenoiseMethod::MmseStsa: {
        const double v = xi * gamma / (1.0 + xi);
        const double h = 0.5 * v;
        return 0.5 * std::sqrt(std::numbers::pi) * std::sqrt(v) / gamma *
               ((1.0 + v) * special::i0e(h) + v * special::i1e(h));
      }
      case DenoiseMethod::MmseLsa: {
        const double v = std::max(xi * gamma / (1.0 + xi), 1e-12);
        return xi / (1.0 + xi) * std::exp(0.5 * special::e1(v));
      }
    }
    return 1.0;
  }

 private:
  SpectralDenoiser(NoiseProfile p, DenoiseMethod m, double reductionDb,
                   OverlapAdd ola)
      : profile_(std::move(p)),
        method_(m),
        floor_(std::pow(10.0, -reductionDb / 20.0)),
        ola_(std::move(ola)),
        prevA2_(profile_.power.size(), 0.0) {}

  void frame(std::vector<std::complex<double>>& bins) {
    const std::size_t n = bins.size(), half = n / 2;
    for (std::size_t k = 0; k <= half; ++k) {
      double g = 1.0;
      if (floor_ < 1.0) {
        const double y2 = std::norm(bins[k]);
        const double l = std::max(profile_.power[k], 1e-300);
        const double gamma = y2 / l;
        const double xi = first_ ? std::max(gamma - 1.0, 0.0)
                                 : 0.98 * prevA2_[k] / l +
                                       0.02 * std::max(gamma - 1.0, 0.0);
        g = std::clamp(gain(method_, gamma, xi), floor_, 1.0);
        prevA2_[k] = g * g * y2;
      }
      bins[k] *= g;
      if (k > 0 && k < half) bins[n - k] = std::conj(bins[k]);
    }
    first_ = false;
  }

  NoiseProfile profile_;
  DenoiseMethod method_;
  double floor_;
  OverlapAdd ola_;
  std::vector<double> prevA2_;
  bool first_ = true;
};

// Offline, length-preserving denoise of a whole signal (latency removed).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> denoise(
    const std::vector<double>& x, const NoiseProfile& profile,
    DenoiseMethod method, double reductionDb) {
  using R = Result<std::vector<double>, QuantityError>;
  auto d = SpectralDenoiser::create(profile, method, reductionDb);
  if (!d.ok()) return R::failure(d.error());
  SpectralDenoiser den = std::move(d).value();
  const std::size_t lat = den.latencyFrames();
  std::vector<double> buf(x.size() + lat, 0.0);
  std::copy(x.begin(), x.end(), buf.begin());
  den.process(buf.data(), buf.data(), buf.size());
  return R::success(std::vector<double>(buf.begin() + static_cast<std::ptrdiff_t>(lat),
                                        buf.end()));
}

// Spectral repair: scale the bins of [t0, t1] x [f0, f1] by gainDb.
// Frames are selected by their centre time; latency removed; same length.
[[nodiscard]] inline Result<std::vector<double>, QuantityError> spectralGainRegion(
    const std::vector<double>& x, SampleRateHz rate, double t0, double t1,
    double f0, double f1, double gainDb, std::size_t frameSize = 2048) {
  using R = Result<std::vector<double>, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || !(fs > 0.0) || !(t0 < t1) || !(f0 < f1) ||
      f0 < 0.0 || !std::isfinite(gainDb) || gainDb > 24.0 || gainDb < -120.0 ||
      x.empty())
    return R::failure(QuantityError::InvalidArgument);
  auto o = OverlapAdd::create(frameSize, 4);
  if (!o.ok()) return R::failure(o.error());
  OverlapAdd ola = std::move(o).value();
  const std::size_t lat = ola.latencyFrames(), hop = ola.hop();
  const double g = std::pow(10.0, gainDb / 20.0);
  const double binHz = fs / static_cast<double>(frameSize);
  std::vector<double> buf(x.size() + lat, 0.0);
  std::copy(x.begin(), x.end(), buf.begin());
  std::size_t frame = 0;
  ola.process(buf.data(), buf.data(), buf.size(),
              [&](std::vector<std::complex<double>>& bins) {
                // Frame m holds input samples [(m+1)hop - N, (m+1)hop).
                const double centre =
                    (static_cast<double>((frame + 1) * hop) -
                     0.5 * static_cast<double>(frameSize)) / fs;
                ++frame;
                if (centre < t0 || centre > t1) return;
                const std::size_t n = bins.size(), half = n / 2;
                for (std::size_t k = 0; k <= half; ++k) {
                  const double f = static_cast<double>(k) * binHz;
                  if (f < f0 || f > f1) continue;
                  bins[k] *= g;
                  if (k > 0 && k < half) bins[n - k] = std::conj(bins[k]);
                }
              });
  return R::success(std::vector<double>(buf.begin() + static_cast<std::ptrdiff_t>(lat),
                                        buf.end()));
}

}  // namespace mmathen
