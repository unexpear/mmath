#pragma once

// D36 — transient-aware time-scale modification.
//
// 1. PGHI phase vocoder (Průša & Holighaus, "Phase Vocoder Done Right",
//    EUSIPCO 2017, Algorithm 1). The classic vocoder (D28) integrates the
//    phase along time only; PGHI also uses the phase derivative along
//    frequency, choosing per bin the propagation direction by magnitude
//    with a max-heap, so vertical phase coherence (transients, chirps) is
//    kept without peak picking. Per the paper:
//      time derivative   centred heterodyned difference, eqs (13)-(15)
//                        (backward at the last frame, forward at the first)
//      frequency deriv.  centred wrapped difference across bins, (16)-(18),
//                        in radians per bin
//      integration       trapezoidal: along time with the synthesis hop
//                        (line 10), along frequency scaled by the stretch
//                        (lines 17, 22: b_s = alpha b_a)
//      tolerance         bins below 1e-6 of the two frames' maximum get a
//                        pseudo-random phase (fixed seed: reproducible).
//    Frames are zero-phase (window centre rotated to index 0), as the
//    frequency derivative requires: with frames starting at index 0 a
//    component at the window centre steps by pi per bin, exactly the
//    principal-argument wrap, and the integration fails (measured: only
//    10 % of a click's energy stayed near it, even at stretch 1).
//    Stated difference from the paper's evaluation setup: no zero-padding
//    (FFT length = window length); window Hann, synthesis hop N/4.
// 2. Harmonic-percussive separation (Fitzgerald, DAFx 2010): median filter
//    the STFT magnitude along time (harmonic) and along frequency
//    (percussive), binary masks H >= P. The masks are complementary and
//    the inverse STFT is linear, so harmonic + percussive = the input
//    exactly (to roundoff; tested).
// 3. Hybrid TSM (Driedger, Müller & Ewert, IEEE SPL 21(1), 2014): the
//    harmonic part through a phase vocoder, the percussive part through
//    plain overlap-add with short frames (256 samples), which keeps attacks
//    compact instead of smearing them, then summed.
// All offline. Pitch shifting = stretch by p, then resample by p (as D28).

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <queue>
#include <utility>
#include <vector>

#include "../analysis/fft.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "time_pitch.h"

namespace mmathen {

namespace detail {
inline std::vector<double> hannPeriodic(std::size_t n) {
  std::vector<double> w(n);
  for (std::size_t i = 0; i < n; ++i)
    w[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                static_cast<double>(n));
  return w;
}
// Overlap-add the windowed frames of `frames` (time-domain, length n) at
// hop `hs`, normalised by the exact sum of squared windows (floored).
inline std::vector<double> olaNormalised(const std::vector<std::vector<double>>& frames,
                                         const std::vector<double>& win, std::size_t hs,
                                         std::size_t outLen, std::size_t offset) {
  const std::size_t n = win.size();
  std::vector<double> acc(frames.size() * hs + n, 0.0), norm(acc.size(), 0.0);
  for (std::size_t m = 0; m < frames.size(); ++m)
    for (std::size_t i = 0; i < n; ++i) {
      acc[m * hs + i] += frames[m][i] * win[i];
      norm[m * hs + i] += win[i] * win[i];
    }
  double nmax = 0.0;
  for (double v : norm) nmax = std::max(nmax, v);
  std::vector<double> out(outLen, 0.0);
  for (std::size_t i = 0; i < outLen && i + offset < acc.size(); ++i)
    out[i] = acc[i + offset] / std::max(norm[i + offset], 1e-3 * nmax);
  return out;
}
}  // namespace detail

// PGHI time stretch; stretch in [0.25, 4], output length round(stretch n).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> pghiStretch(
    const std::vector<double>& x, double stretch, std::size_t frameSize = 2048) {
  using R = Result<std::vector<double>, QuantityError>;
  const std::size_t n = frameSize;
  if (!std::isfinite(stretch) || stretch < 0.25 || stretch > 4.0 || n < 64 ||
      n > 65536 || !isPowerOfTwo(n) || x.empty())
    return R::failure(QuantityError::InvalidArgument);
  for (double v : x)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t hs = n / 4, half = n / 2;
  const std::size_t outLen =
      static_cast<std::size_t>(std::llround(stretch * static_cast<double>(x.size())));
  std::vector<double> in(x.size() + half + n, 0.0);
  std::copy(x.begin(), x.end(), in.begin() + static_cast<std::ptrdiff_t>(half));
  const auto win = detail::hannPeriodic(n);
  // Analysis frames at round(m hs / stretch) (actual integer hops kept).
  std::vector<std::size_t> starts;
  for (std::size_t m = 0;; ++m) {
    const auto s = static_cast<std::size_t>(std::llround(static_cast<double>(m * hs) / stretch));
    if (s + n > in.size() || m * hs > outLen + n) break;
    starts.push_back(s);
  }
  const std::size_t frames = starts.size();
  if (frames < 2) return R::failure(QuantityError::InvalidArgument);
  std::vector<std::vector<double>> mag(frames, std::vector<double>(half + 1)),
      ph(frames, std::vector<double>(half + 1));
  std::vector<std::complex<double>> buf(n);
  for (std::size_t m = 0; m < frames; ++m) {
    // Zero-phase frame: the window centre is rotated to index 0, so the
    // frequency derivative measures the group delay relative to the frame
    // centre (a component at the centre has ~0, not a wrap-ambiguous pi).
    for (std::size_t i = 0; i < n; ++i)
      buf[i] = {in[starts[m] + (i + half) % n] * win[(i + half) % n], 0.0};
    detail::fftInPlace(buf, false);
    for (std::size_t k = 0; k <= half; ++k) {
      mag[m][k] = std::abs(buf[k]);
      ph[m][k] = std::arg(buf[k]);
    }
  }
  const double twoPi = 2.0 * std::numbers::pi;
  // (13)-(15): time derivative in rad/sample, centred where possible.
  auto dt = [&](std::size_t m, std::size_t k) {
    const double wk = twoPi * static_cast<double>(k) / static_cast<double>(n);
    auto diff = [&](std::size_t a, std::size_t b) {  // from frame a to b
      const double da = static_cast<double>(starts[b] - starts[a]);
      return da > 0.0 ? detail::princarg(ph[b][k] - ph[a][k] - wk * da) / da + wk : wk;
    };
    if (m == 0) return diff(0, 1);
    if (m + 1 >= frames) return diff(m - 1, m);
    return 0.5 * (diff(m - 1, m) + diff(m, m + 1));
  };
  // (16)-(18): frequency derivative in rad/bin, centred.
  auto df = [&](std::size_t m, std::size_t k) {
    if (k == 0) return detail::princarg(ph[m][1] - ph[m][0]);
    if (k == half) return detail::princarg(ph[m][half] - ph[m][half - 1]);
    return 0.5 * (detail::princarg(ph[m][k] - ph[m][k - 1]) +
                  detail::princarg(ph[m][k + 1] - ph[m][k]));
  };
  std::vector<double> psiPrev(half + 1), psi(half + 1), dtPrev(half + 1), dtCur(half + 1),
      dfCur(half + 1);
  std::vector<std::vector<double>> outFrames(frames, std::vector<double>(n));
  std::uint64_t seed = 0x9E3779B97F4A7C15ULL;
  auto rnd = [&] {
    seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
    return (static_cast<double>(seed >> 11) / 9007199254740992.0) * twoPi - std::numbers::pi;
  };
  for (std::size_t m = 0; m < frames; ++m) {
    for (std::size_t k = 0; k <= half; ++k) {
      dtCur[k] = dt(m, k);
      dfCur[k] = df(m, k);
    }
    if (m == 0) {
      psi = ph[0];
    } else {
      // Algorithm 1 for frame m.
      double mx = 0.0;
      for (std::size_t k = 0; k <= half; ++k) mx = std::max({mx, mag[m][k], mag[m - 1][k]});
      const double abstol = 1e-6 * mx;
      std::vector<char> todo(half + 1, 0);
      std::size_t left = 0;
      for (std::size_t k = 0; k <= half; ++k) {
        if (mag[m][k] > abstol) {
          todo[k] = 1;
          ++left;
        } else {
          psi[k] = rnd();
        }
      }
      using Item = std::pair<double, std::pair<std::size_t, int>>;  // (mag, (bin, frame 0=prev 1=cur))
      std::priority_queue<Item> heap;
      for (std::size_t k = 0; k <= half; ++k)
        if (todo[k]) heap.push({mag[m - 1][k], {k, 0}});
      while (left > 0 && !heap.empty()) {
        const auto [mv, pos] = heap.top();
        heap.pop();
        (void)mv;
        const std::size_t k = pos.first;
        if (pos.second == 0) {
          if (todo[k]) {  // line 10: along time
            psi[k] = psiPrev[k] + 0.5 * static_cast<double>(hs) * (dtPrev[k] + dtCur[k]);
            todo[k] = 0;
            --left;
            heap.push({mag[m][k], {k, 1}});
          }
        } else {
          if (k + 1 <= half && todo[k + 1]) {  // line 17: up in frequency
            psi[k + 1] = psi[k] + 0.5 * stretch * (dfCur[k] + dfCur[k + 1]);
            todo[k + 1] = 0;
            --left;
            heap.push({mag[m][k + 1], {k + 1, 1}});
          }
          if (k >= 1 && todo[k - 1]) {  // line 22: down in frequency
            psi[k - 1] = psi[k] - 0.5 * stretch * (dfCur[k] + dfCur[k - 1]);
            todo[k - 1] = 0;
            --left;
            heap.push({mag[m][k - 1], {k - 1, 1}});
          }
        }
      }
    }
    for (std::size_t k = 0; k <= half; ++k) buf[k] = std::polar(mag[m][k], psi[k]);
    for (std::size_t k = 1; k < half; ++k) buf[n - k] = std::conj(buf[k]);
    detail::fftInPlace(buf, true);
    for (std::size_t i = 0; i < n; ++i) outFrames[m][i] = buf[(i + half) % n].real();
    psiPrev = psi;
    dtPrev = dtCur;
  }
  return R::success(detail::olaNormalised(outFrames, win, hs, outLen, half));
}

struct HpssResult {
  std::vector<double> harmonic, percussive;
};

// Median-filtering harmonic-percussive separation (see header).
[[nodiscard]] inline Result<HpssResult, QuantityError> hpss(
    const std::vector<double>& x, std::size_t frameSize = 1024, std::size_t kernel = 17) {
  using R = Result<HpssResult, QuantityError>;
  const std::size_t n = frameSize;
  if (n < 64 || n > 65536 || !isPowerOfTwo(n) || kernel < 3 || kernel % 2 == 0 ||
      x.empty())
    return R::failure(QuantityError::InvalidArgument);
  const std::size_t hop = n / 4, half = n / 2;
  std::vector<double> in(x.size() + 2 * n, 0.0);
  std::copy(x.begin(), x.end(), in.begin() + static_cast<std::ptrdiff_t>(n));
  // sqrt-Hann analysis and synthesis: product Hann, sum 2 at hop N/4.
  std::vector<double> w(n);
  for (std::size_t i = 0; i < n; ++i)
    w[i] = std::sqrt(0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) /
                                          static_cast<double>(n)));
  const std::size_t frames = (in.size() - n) / hop + 1;
  std::vector<std::vector<std::complex<double>>> spec(frames,
                                                      std::vector<std::complex<double>>(half + 1));
  std::vector<std::complex<double>> buf(n);
  for (std::size_t m = 0; m < frames; ++m) {
    for (std::size_t i = 0; i < n; ++i) buf[i] = {in[m * hop + i] * w[i], 0.0};
    detail::fftInPlace(buf, false);
    for (std::size_t k = 0; k <= half; ++k) spec[m][k] = buf[k];
  }
  const std::size_t r = kernel / 2;
  std::vector<double> tmp;
  auto median = [&](std::vector<double>& v) {
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
    return v[v.size() / 2];
  };
  HpssResult out;
  out.harmonic.assign(in.size(), 0.0);
  out.percussive.assign(in.size(), 0.0);
  std::vector<std::complex<double>> bh(n), bp(n);
  for (std::size_t m = 0; m < frames; ++m) {
    for (std::size_t k = 0; k <= half; ++k) {
      tmp.clear();
      for (std::size_t j = (m >= r ? m - r : 0); j <= std::min(frames - 1, m + r); ++j)
        tmp.push_back(std::abs(spec[j][k]));
      const double hmed = median(tmp);
      tmp.clear();
      for (std::size_t j = (k >= r ? k - r : 0); j <= std::min(half, k + r); ++j)
        tmp.push_back(std::abs(spec[m][j]));
      const double pmed = median(tmp);
      const bool harm = hmed >= pmed;
      bh[k] = harm ? spec[m][k] : 0.0;
      bp[k] = harm ? 0.0 : spec[m][k];
      if (k > 0 && k < half) {
        bh[n - k] = std::conj(bh[k]);
        bp[n - k] = std::conj(bp[k]);
      }
    }
    detail::fftInPlace(bh, true);
    detail::fftInPlace(bp, true);
    for (std::size_t i = 0; i < n; ++i) {
      out.harmonic[m * hop + i] += bh[i].real() * w[i] * 0.5;
      out.percussive[m * hop + i] += bp[i].real() * w[i] * 0.5;
    }
  }
  out.harmonic.erase(out.harmonic.begin(), out.harmonic.begin() + static_cast<std::ptrdiff_t>(n));
  out.percussive.erase(out.percussive.begin(), out.percussive.begin() + static_cast<std::ptrdiff_t>(n));
  out.harmonic.resize(x.size());
  out.percussive.resize(x.size());
  return R::success(std::move(out));
}

// Plain overlap-add time stretch with short frames (percussive part).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> olaStretch(
    const std::vector<double>& x, double stretch, std::size_t frameSize = 256) {
  using R = Result<std::vector<double>, QuantityError>;
  const std::size_t n = frameSize;
  if (!std::isfinite(stretch) || stretch < 0.25 || stretch > 4.0 || n < 16 || x.empty())
    return R::failure(QuantityError::InvalidArgument);
  const std::size_t hs = n / 2, half = n / 2;
  const std::size_t outLen =
      static_cast<std::size_t>(std::llround(stretch * static_cast<double>(x.size())));
  std::vector<double> in(x.size() + half + n, 0.0);
  std::copy(x.begin(), x.end(), in.begin() + static_cast<std::ptrdiff_t>(half));
  const auto win = detail::hannPeriodic(n);
  std::vector<std::vector<double>> frames;
  for (std::size_t m = 0;; ++m) {
    const auto s = static_cast<std::size_t>(std::llround(static_cast<double>(m * hs) / stretch));
    if (s + n > in.size() || m * hs > outLen + n) break;
    std::vector<double> f(n);
    for (std::size_t i = 0; i < n; ++i) f[i] = in[s + i] * win[i];
    frames.push_back(std::move(f));
  }
  // Analysis already windowed: normalise by the sum of windows (not squared).
  std::vector<double> acc(frames.size() * hs + n, 0.0), norm(acc.size(), 0.0);
  for (std::size_t m = 0; m < frames.size(); ++m)
    for (std::size_t i = 0; i < n; ++i) {
      acc[m * hs + i] += frames[m][i];
      norm[m * hs + i] += win[i];
    }
  std::vector<double> out(outLen, 0.0);
  for (std::size_t i = 0; i < outLen && i + half < acc.size(); ++i)
    out[i] = acc[i + half] / std::max(norm[i + half], 1e-3);
  return R::success(std::move(out));
}

enum class TsmMethod { ClassicLocked, Pghi, Hybrid };

// Time stretch by method (Hybrid = HPSS + PGHI for harmonic + OLA for
// percussive).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> timeStretch(
    const std::vector<double>& x, double stretch, TsmMethod method,
    std::size_t frameSize = 2048) {
  using R = Result<std::vector<double>, QuantityError>;
  if (method == TsmMethod::ClassicLocked) {
    PhaseVocoderOptions o;
    o.frameSize = frameSize;
    return phaseVocoderStretch(x, stretch, o);
  }
  if (method == TsmMethod::Pghi) return pghiStretch(x, stretch, frameSize);
  auto sep = hpss(x);
  if (!sep.ok()) return R::failure(sep.error());
  auto h = pghiStretch(sep.value().harmonic, stretch, frameSize);
  auto p = olaStretch(sep.value().percussive, stretch);
  if (!h.ok()) return R::failure(h.error());
  if (!p.ok()) return R::failure(p.error());
  std::vector<double> out = std::move(h).value();
  const auto& pp = p.value();
  for (std::size_t i = 0; i < out.size() && i < pp.size(); ++i) out[i] += pp[i];
  return R::success(std::move(out));
}

// Pitch shift by ratio p keeping length: stretch by p, resample by p (with
// the same anti-alias low-pass as D28 for p > 1).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> pitchShift(
    const std::vector<double>& x, SampleRateHz rate, double ratio, TsmMethod method) {
  using R = Result<std::vector<double>, QuantityError>;
  if (!std::isfinite(ratio) || ratio < 0.25 || ratio > 4.0)
    return R::failure(QuantityError::InvalidArgument);
  auto st = timeStretch(x, ratio, method);
  if (!st.ok()) return R::failure(st.error());
  std::vector<double> y = std::move(st).value();
  if (ratio > 1.0) {
    auto lp = designButterworth(false, 8, FrequencyHz{0.45 * rate.value() / ratio}, rate);
    if (!lp.ok()) return R::failure(lp.error());
    for (const auto& k : lp.value()) {
      Biquad b{k};
      b.processBlock(y.data(), y.data(), FrameCount{static_cast<std::uint64_t>(y.size())});
    }
  }
  std::vector<double> out(x.size());
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = detail::hermiteAt(y, static_cast<double>(i) * ratio);
  return R::success(std::move(out));
}

}  // namespace mmathen
