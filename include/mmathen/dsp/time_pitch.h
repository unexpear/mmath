#pragma once

// D28 — time stretching and pitch shifting, offline: a frequency-domain
// phase vocoder and time-domain PSOLA.
//
// Phase vocoder (Flanagan & Golden 1966; phase locking after Laroche &
// Dolson, "Improved phase vocoder time-scale modification of audio", IEEE
// TSAP 7(3), 1999). Frames of N samples, periodic Hann windows for analysis
// and synthesis, synthesis hop Hs = N/4, analysis frame m starting at
// round(m Hs / stretch) (the actual integer hop dA between consecutive
// frames is used, so rounding never becomes a phase error). Per bin k,
// w_k = 2 pi k / N:
//   heterodyned advance  D = princarg(phi_m - phi_{m-1} - w_k dA)
//   instantaneous freq   w = w_k + D / dA
//   synthesis phase      psi_m = psi_{m-1} + w Hs
// With identity phase locking only spectral PEAKS (a bin larger than its
// two neighbours on each side) are propagated that way; every other bin
// keeps its analysis phase offset from the peak of its region (nearest
// peak): psi_k = psi_p + (phi_k - phi_p), which keeps each partial's
// neighbouring bins coherent (less "phasiness"). Output = overlap-add of
// the windowed inverse transforms divided by the exact sum of squared
// windows at every sample (floored at 1e-3 of its maximum, which only
// touches the first/last frame's tails). At stretch = 1 every psi equals
// its phi modulo 2 pi, so the vocoder reconstructs its input to roundoff;
// the tests pin that.
//
// Pitch shift by ratio p = stretch by p, then resample by p (cubic Hermite
// interpolation), so the length is kept. For p > 1 the stretched signal is
// low-passed first (8th-order Butterworth at 0.45 fs / p, D23) so the
// resampler does not alias. A vocoder shift moves EVERYTHING by p,
// including formants (the spectral envelope) — the "chipmunk" effect.
//
// TD-PSOLA (Moulines & Charpentier, Speech Communication 9, 1990):
//   1. f0 track by pYIN (D35; YIN in D28's first version), frames of 2048,
//      hop 256; the period T(t) is
//      interpolated between frames; unvoiced frames use a fixed 10 ms
//      pseudo-period and are not pitch-shifted (no false periodicity);
//   2. analysis pitch marks one period apart, each aligned to the largest
//      sample within +/- T/4 of its prediction (epochs on waveform peaks);
//   3. synthesis marks spaced by T / pitchRatio (voiced) over the stretched
//      time axis; each takes the analysis mark nearest to s / timeRatio;
//   4. a Hann grain of 2T samples centred on that analysis mark is added at
//      the synthesis mark, scaled by (synthesis spacing / T) so the average
//      overlap — and hence the level — is independent of the ratio (Hann
//      grains at spacing T sum to exactly 1).
// PSOLA keeps each period's waveform, so the spectral envelope (formants)
// stays where it was while the harmonics move: the tests show a fixed
// 1 kHz resonance staying put under PSOLA and moving under the vocoder.
// Limits, stated: offline only; PSOLA assumes one monophonic periodic
// source (it is not meant for chords or mixes); grain spacing larger than
// the grain (pitch ratios below 0.5) is refused.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "../analysis/fft.h"
#include "../analysis/pitch.h"
#include "../analysis/pyin.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "biquad.h"
#include "filters.h"

namespace mmathen {

struct PhaseVocoderOptions {
  std::size_t frameSize = 2048;  // power of two, 64..65536
  bool phaseLocking = true;
};

namespace detail {
inline double princarg(double a) {
  return a - 2.0 * std::numbers::pi *
                 std::floor((a + std::numbers::pi) / (2.0 * std::numbers::pi));
}
}  // namespace detail

// Stretch time by `stretch` in [0.25, 4]; output length round(stretch * n).
[[nodiscard]] inline Result<std::vector<double>, QuantityError>
phaseVocoderStretch(const std::vector<double>& x, double stretch,
                    const PhaseVocoderOptions& o = {}) {
  using R = Result<std::vector<double>, QuantityError>;
  const std::size_t n = o.frameSize;
  if (!std::isfinite(stretch) || stretch < 0.25 || stretch > 4.0 || n < 64 ||
      n > 65536 || !isPowerOfTwo(n) || x.empty())
    return R::failure(QuantityError::InvalidArgument);
  for (double v : x)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t hs = n / 4;
  const std::size_t outLen =
      static_cast<std::size_t>(std::llround(stretch * static_cast<double>(x.size())));
  // Analysis input padded by N/2 zeros at the front (frame 0 centred on
  // sample 0) and N at the back.
  std::vector<double> in(x.size() + n / 2 + n, 0.0);
  std::copy(x.begin(), x.end(), in.begin() + static_cast<std::ptrdiff_t>(n / 2));
  std::vector<double> win(n);
  for (std::size_t i = 0; i < n; ++i)
    win[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi *
                                  static_cast<double>(i) / static_cast<double>(n));
  const std::size_t frames = outLen / hs + 2;
  std::vector<double> acc(frames * hs + n, 0.0), norm(acc.size(), 0.0);
  const std::size_t half = n / 2;
  std::vector<double> phiPrev(half + 1, 0.0), psi(half + 1, 0.0),
      mag(half + 1), phi(half + 1);
  std::vector<std::complex<double>> buf(n);
  std::vector<std::size_t> peaks;
  std::size_t prevStart = 0;
  for (std::size_t m = 0; m < frames; ++m) {
    const auto start = static_cast<std::size_t>(
        std::llround(static_cast<double>(m * hs) / stretch));
    if (start + n > in.size()) break;
    for (std::size_t i = 0; i < n; ++i)
      buf[i] = std::complex<double>(in[start + i] * win[i], 0.0);
    detail::fftInPlace(buf, false);
    for (std::size_t k = 0; k <= half; ++k) {
      mag[k] = std::abs(buf[k]);
      phi[k] = std::arg(buf[k]);
    }
    if (m == 0) {
      psi = phi;
    } else {
      const double dA = static_cast<double>(start - prevStart);
      auto advance = [&](std::size_t k) {
        const double wk = 2.0 * std::numbers::pi * static_cast<double>(k) /
                          static_cast<double>(n);
        const double d = detail::princarg(phi[k] - phiPrev[k] - wk * dA);
        const double w = dA > 0.0 ? wk + d / dA : wk;
        return psi[k] + w * static_cast<double>(hs);
      };
      if (!o.phaseLocking) {
        for (std::size_t k = 0; k <= half; ++k) psi[k] = advance(k);
      } else {
        peaks.clear();
        for (std::size_t k = 0; k <= half; ++k) {
          bool peak = true;
          for (std::size_t j = 1; j <= 2 && peak; ++j) {
            if (k >= j && mag[k - j] >= mag[k]) peak = false;
            if (k + j <= half && mag[k + j] > mag[k]) peak = false;
          }
          if (peak) peaks.push_back(k);
        }
        std::vector<double> next(half + 1);
        if (peaks.empty()) {
          for (std::size_t k = 0; k <= half; ++k) next[k] = advance(k);
        } else {
          for (std::size_t p : peaks) next[p] = advance(p);
          // Nearest peak by bin index (the peak's region).
          auto dist = [](std::size_t a, std::size_t b) {
            return a > b ? a - b : b - a;
          };
          std::size_t pi = 0;
          for (std::size_t k = 0; k <= half; ++k) {
            while (pi + 1 < peaks.size() &&
                   dist(peaks[pi + 1], k) <= dist(peaks[pi], k))
              ++pi;
            const std::size_t p = peaks[pi];
            if (k != p) next[k] = next[p] + (phi[k] - phi[p]);
          }
        }
        psi = next;
      }
    }
    phiPrev = phi;
    prevStart = start;
    for (std::size_t k = 0; k <= half; ++k) buf[k] = std::polar(mag[k], psi[k]);
    for (std::size_t k = 1; k < half; ++k) buf[n - k] = std::conj(buf[k]);
    detail::fftInPlace(buf, true);
    const std::size_t at = m * hs;
    for (std::size_t i = 0; i < n; ++i) {
      acc[at + i] += buf[i].real() * win[i];
      norm[at + i] += win[i] * win[i];
    }
  }
  double nmax = 0.0;
  for (double v : norm) nmax = std::max(nmax, v);
  std::vector<double> out(outLen, 0.0);
  for (std::size_t i = 0; i < outLen; ++i) {
    const std::size_t j = i + half;  // undo the front padding (centred frames)
    if (j >= acc.size()) break;
    out[i] = acc[j] / std::max(norm[j], 1e-3 * nmax);
  }
  return R::success(std::move(out));
}

namespace detail {
// Cubic Hermite (Catmull-Rom) read at fractional position t.
inline double hermiteAt(const std::vector<double>& y, double t) {
  const auto i = static_cast<std::ptrdiff_t>(std::floor(t));
  const double f = t - static_cast<double>(i);
  auto at = [&](std::ptrdiff_t k) {
    return k < 0 || k >= static_cast<std::ptrdiff_t>(y.size())
               ? 0.0
               : y[static_cast<std::size_t>(k)];
  };
  const double y0 = at(i - 1), y1 = at(i), y2 = at(i + 1), y3 = at(i + 2);
  const double c1 = 0.5 * (y2 - y0);
  const double c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3;
  const double c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
  return ((c3 * f + c2) * f + c1) * f + y1;
}
}  // namespace detail

// Shift pitch by `ratio` in [0.25, 4] keeping the length (see header).
[[nodiscard]] inline Result<std::vector<double>, QuantityError>
phaseVocoderPitchShift(const std::vector<double>& x, SampleRateHz rate,
                       double ratio, const PhaseVocoderOptions& o = {}) {
  using R = Result<std::vector<double>, QuantityError>;
  if (!std::isfinite(ratio) || ratio < 0.25 || ratio > 4.0)
    return R::failure(QuantityError::InvalidArgument);
  auto st = phaseVocoderStretch(x, ratio, o);
  if (!st.ok()) return R::failure(st.error());
  std::vector<double> y = std::move(st).value();
  if (ratio > 1.0) {
    auto lp = designButterworth(false, 8, FrequencyHz{0.45 * rate.value() / ratio},
                                rate);
    if (!lp.ok()) return R::failure(lp.error());
    for (const auto& k : lp.value()) {
      Biquad b{k};
      b.processBlock(y.data(), y.data(),
                     FrameCount{static_cast<std::uint64_t>(y.size())});
    }
  }
  std::vector<double> out(x.size());
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = detail::hermiteAt(y, static_cast<double>(i) * ratio);
  return R::success(std::move(out));
}

struct PsolaOptions {
  double fMinHz = 60.0;
  double fMaxHz = 800.0;
  double unvoicedPeriodSeconds = 0.01;
};

// TD-PSOLA: pitchRatio in [0.5, 2], timeRatio in [0.25, 4]; output length
// round(timeRatio * n).
[[nodiscard]] inline Result<std::vector<double>, QuantityError> psola(
    const std::vector<double>& x, SampleRateHz rate, double pitchRatio,
    double timeRatio, const PsolaOptions& o = {}) {
  using R = Result<std::vector<double>, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(pitchRatio) || pitchRatio < 0.5 || pitchRatio > 2.0 ||
      !std::isfinite(timeRatio) || timeRatio < 0.25 || timeRatio > 4.0 ||
      !std::isfinite(o.unvoicedPeriodSeconds) ||
      !(o.unvoicedPeriodSeconds > 0.0) || x.size() < 4096)
    return R::failure(QuantityError::InvalidArgument);
  PitchSearch s;
  s.fMinHz = o.fMinHz;
  s.fMaxHz = o.fMaxHz;
  // D35: pYIN marks voicing more reliably than frame-wise YIN (no octave
  // jumps, silence gated), which is what PSOLA's marks depend on.
  PyinOptions po;
  po.fMinHz = s.fMinHz;
  po.fMaxHz = s.fMaxHz;
  auto track = pyinTrack(x, rate, 2048, 256, po);
  if (!track.ok()) return R::failure(track.error());
  const auto& tr = track.value();
  const double tU = o.unvoicedPeriodSeconds * fs;
  // Period (samples) and voicing at time t (samples), linear between frames.
  auto periodAt = [&](double t, bool& voiced) {
    const double sec = t / fs;
    std::size_t i = 0;
    while (i + 1 < tr.size() && tr[i + 1].timeSeconds <= sec) ++i;
    const auto& a = tr[i];
    const auto& b = tr[std::min(i + 1, tr.size() - 1)];
    voiced = a.estimate.voiced;
    if (!a.estimate.voiced) return tU;
    double pa = a.estimate.periodSamples, pb = pa;
    if (b.estimate.voiced) pb = b.estimate.periodSamples;
    const double span = b.timeSeconds - a.timeSeconds;
    const double u =
        span > 0.0 ? std::clamp((sec - a.timeSeconds) / span, 0.0, 1.0) : 0.0;
    return pa + (pb - pa) * u;
  };
  // 2. Analysis marks.
  std::vector<double> marks, periods;
  std::vector<bool> voicedMark;
  {
    bool v = false;
    double t = 0.0;
    const double t0 = periodAt(0.0, v);
    std::size_t best = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(t0) && i < x.size(); ++i)
      if (x[i] > x[best]) best = i;
    t = static_cast<double>(best);
    while (t < static_cast<double>(x.size())) {
      const double p = periodAt(t, v);
      marks.push_back(t);
      periods.push_back(p);
      voicedMark.push_back(v);
      double next = t + p;
      if (v) {
        const double lo = std::max(t + 0.5 * p, next - 0.25 * p);
        const double hi = next + 0.25 * p;
        std::size_t bi = static_cast<std::size_t>(std::ceil(lo));
        for (std::size_t i = bi; static_cast<double>(i) <= hi && i < x.size(); ++i)
          if (x[i] > x[bi]) bi = i;
        if (bi < x.size()) next = static_cast<double>(bi);
      }
      t = next;
    }
  }
  if (marks.empty()) return R::failure(QuantityError::DomainError);
  // 3-4. Synthesis.
  const std::size_t outLen =
      static_cast<std::size_t>(std::llround(timeRatio * static_cast<double>(x.size())));
  std::vector<double> out(outLen + 1, 0.0);
  std::size_t mi = 0;
  double sm = marks.front() * timeRatio;
  while (sm < static_cast<double>(outLen)) {
    const double a = sm / timeRatio;
    while (mi + 1 < marks.size() &&
           std::fabs(marks[mi + 1] - a) <= std::fabs(marks[mi] - a))
      ++mi;
    const double t = periods[mi];
    const double spacing = voicedMark[mi] ? t / pitchRatio : t;
    const double scale = spacing / t;
    const auto half = static_cast<std::ptrdiff_t>(std::floor(t));
    const auto c = static_cast<std::ptrdiff_t>(std::llround(marks[mi]));
    const auto sc = static_cast<std::ptrdiff_t>(std::llround(sm));
    for (std::ptrdiff_t k = -half; k <= half; ++k) {
      const std::ptrdiff_t src = c + k, dst = sc + k;
      if (src < 0 || src >= static_cast<std::ptrdiff_t>(x.size()) || dst < 0 ||
          dst >= static_cast<std::ptrdiff_t>(outLen))
        continue;
      const double w =
          0.5 + 0.5 * std::cos(std::numbers::pi * static_cast<double>(k) / t);
      out[static_cast<std::size_t>(dst)] +=
          scale * w * x[static_cast<std::size_t>(src)];
    }
    sm += spacing;
  }
  out.resize(outLen);
  return R::success(std::move(out));
}

}  // namespace mmathen
