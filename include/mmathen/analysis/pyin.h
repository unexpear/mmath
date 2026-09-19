#pragma once

// D35 — pYIN: probabilistic YIN with hidden-Markov smoothing (Mauch &
// Dixon, "pYIN: A fundamental frequency estimator using probabilistic
// threshold distributions", ICASSP 2014).
//
// Stage 1, per frame (the D27 YIN difference d and normalised d'):
//   thresholds s_i = 0.01, 0.02, ..., 1.00 with prior weights from a
//   Beta(a, b) distribution (a = 2, b = 18: mean 0.1, the paper's default
//   family); for each s_i the YIN absolute-threshold rule picks a period
//   (the first dip of d' below s_i, followed to its local minimum). Each
//   distinct period collects the weights of the thresholds that chose it,
//   times 1 (a dip below threshold) — or, when no dip is below s_i, the
//   global minimum collects the weight times p_a = 0.01. The periods are
//   refined by parabolic interpolation on the raw d (as in D27). The mass
//   not given to any period is the frame's probability of being unvoiced.
// Stage 2, over all frames (Viterbi on an HMM):
//   M pitch bins of 1/binsPerSemitone semitone between fMin and fMax, each
//   in a voiced and an unvoiced copy (2M states);
//   emission: voiced bin m = the sum of candidate probabilities falling in
//     bin m; unvoiced bin m = (1 - total candidate probability) / M;
//   transition: pitch moves at most maxJump bins per frame with triangular
//     weights; voicing switches with probability switchProb (0.01).
//   The most likely path gives voicing and a pitch bin; the reported f0 is
//   the refined candidate nearest that bin (within one bin), else the bin
//   centre, so pYIN keeps YIN's sub-cent precision on stable tones.
// Energy gate (an addition, stated): frames below silenceDb RMS get no
// candidates, i.e. are unvoiced with certainty.
// Differences from the published system, stated: the Beta prior is fixed
// to (2, 18); the HMM parameters are this header's defaults, not tuned on
// the paper's data; no note segmentation (the paper's second system).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "pitch.h"

namespace mmathen {

struct PyinOptions {
  double fMinHz = 60.0;
  double fMaxHz = 1000.0;
  double betaA = 2.0, betaB = 18.0;  // threshold prior
  double pAbsolute = 0.01;           // weight when no dip is below s_i
  int binsPerSemitone = 10;          // 10-cent bins
  int maxJumpBins = 25;              // +/- 2.5 semitones per frame
  double switchProb = 0.01;          // voiced <-> unvoiced
  double silenceDb = -60.0;          // frames below this RMS have no pitch
};

struct PitchCandidate {
  double frequencyHz = 0.0;
  double probability = 0.0;
};

namespace detail {
// Stage 1 on one frame: candidates and their probabilities.
inline Result<std::vector<PitchCandidate>, QuantityError> pyinCandidates(
    const std::vector<double>& frame, SampleRateHz rate, const PyinOptions& o) {
  using R = Result<std::vector<PitchCandidate>, QuantityError>;
  PitchSearch s;
  s.fMinHz = o.fMinHz;
  s.fMaxHz = o.fMaxHz;
  LagRange g;
  if (!lagRange(frame.size(), rate.value(), s, g))
    return R::failure(QuantityError::InvalidArgument);
  // Energy gate: a (near-)silent frame has no pitch. Without it, d' is flat
  // on digital silence, the global minimum sits on the same lag every frame
  // and that steady path beats the spread-out unvoiced mass (measured: 30
  // silent frames reported voiced at fMax).
  double energy = 0.0;
  for (double v : frame) energy += v * v;
  const double rmsDb = energy > 0.0
                           ? 10.0 * std::log10(energy / static_cast<double>(frame.size()))
                           : -400.0;
  if (rmsDb < o.silenceDb) return R::success(std::vector<PitchCandidate>{});
  const std::size_t w = g.window;
  std::vector<double> dp(g.hi + 2, 1.0), dr(g.hi + 2, 0.0);
  double running = 0.0;
  for (std::size_t tau = 1; tau <= g.hi + 1; ++tau) {
    double d = 0.0;
    for (std::size_t j = 0; j < w; ++j) {
      const double e = frame[j] - frame[j + tau];
      d += e * e;
    }
    dr[tau] = d;
    running += d;
    dp[tau] = running > 0.0 ? d * static_cast<double>(tau) / running : 1.0;
  }
  // Beta prior over thresholds 0.01..1.00 (normalised).
  std::vector<double> prior(100);
  double z = 0.0;
  for (int i = 0; i < 100; ++i) {
    const double t = 0.01 * (i + 1);
    const double v = t >= 1.0 ? 0.0
                              : std::pow(t, o.betaA - 1.0) * std::pow(1.0 - t, o.betaB - 1.0);
    prior[static_cast<std::size_t>(i)] = v;
    z += v;
  }
  std::vector<double> mass(g.hi + 2, 0.0);
  std::size_t globalMin = g.lo;
  for (std::size_t tau = g.lo; tau <= g.hi; ++tau)
    if (dp[tau] < dp[globalMin]) globalMin = tau;
  for (int i = 0; i < 100; ++i) {
    const double pr = prior[static_cast<std::size_t>(i)] / z;
    if (pr <= 0.0) continue;
    const double thr = 0.01 * (i + 1);
    std::size_t pick = 0;
    for (std::size_t tau = g.lo; tau <= g.hi; ++tau)
      if (dp[tau] < thr) {
        while (tau + 1 <= g.hi && dp[tau + 1] < dp[tau]) ++tau;
        pick = tau;
        break;
      }
    if (pick != 0)
      mass[pick] += pr;
    else
      mass[globalMin] += pr * o.pAbsolute;
  }
  std::vector<PitchCandidate> out;
  for (std::size_t tau = g.lo; tau <= g.hi; ++tau) {
    if (mass[tau] <= 0.0) continue;
    const double off = parabolicOffset(dr[tau - 1], dr[tau], dr[tau + 1]);
    out.push_back({rate.value() / (static_cast<double>(tau) + off), mass[tau]});
  }
  return R::success(std::move(out));
}
}  // namespace detail

// pYIN stage 1 on one frame (public for streaming use).
[[nodiscard]] inline Result<std::vector<PitchCandidate>, QuantityError> pyinCandidates(
    const std::vector<double>& frame, SampleRateHz rate, const PyinOptions& o = {}) {
  return detail::pyinCandidates(frame, rate, o);
}

// pYIN stage 2 alone: Viterbi over per-frame candidates (stage 1, from
// pyinCandidates). Lets a streaming caller (a tuner) keep a window of
// recent frames and re-decode it cheaply. times: frame centres, seconds.
[[nodiscard]] inline Result<std::vector<PitchFrame>, QuantityError> pyinDecode(
    const std::vector<std::vector<PitchCandidate>>& cands, const std::vector<double>& times,
    SampleRateHz rate, const PyinOptions& o = {}) {
  using R = Result<std::vector<PitchFrame>, QuantityError>;
  if (cands.empty() || cands.size() != times.size() || !(o.fMinHz > 0.0) ||
      !(o.fMaxHz > o.fMinHz) || o.binsPerSemitone < 1 || o.binsPerSemitone > 100 ||
      o.maxJumpBins < 1 || !(o.switchProb > 0.0) || !(o.switchProb < 1.0))
    return R::failure(QuantityError::InvalidArgument);
  // Stage 2: HMM over M bins x {voiced, unvoiced}.
  const double semis = 12.0 * std::log2(o.fMaxHz / o.fMinHz);
  const auto m = static_cast<std::size_t>(std::ceil(semis * o.binsPerSemitone)) + 1;
  auto binOf = [&](double f) {
    const double b = 12.0 * std::log2(f / o.fMinHz) * o.binsPerSemitone;
    return static_cast<std::ptrdiff_t>(std::lround(b));
  };
  auto freqOf = [&](std::size_t b) {
    return o.fMinHz * std::pow(2.0, static_cast<double>(b) / (12.0 * o.binsPerSemitone));
  };
  const std::size_t t = cands.size(), ns = 2 * m;
  const auto jump = static_cast<std::ptrdiff_t>(o.maxJumpBins);
  // Triangular transition weights within +/- jump, normalised.
  std::vector<double> tri(static_cast<std::size_t>(2 * jump + 1));
  double tz = 0.0;
  for (std::ptrdiff_t d = -jump; d <= jump; ++d) {
    tri[static_cast<std::size_t>(d + jump)] = static_cast<double>(jump + 1 - std::abs(d));
    tz += tri[static_cast<std::size_t>(d + jump)];
  }
  for (double& v : tri) v = std::log(v / tz);
  const double stay = std::log(1.0 - o.switchProb), sw = std::log(o.switchProb);
  const double neg = -std::numeric_limits<double>::infinity();
  std::vector<double> score(ns, std::log(1.0 / static_cast<double>(ns))), next(ns);
  std::vector<std::vector<std::uint32_t>> back(t, std::vector<std::uint32_t>(ns, 0));
  std::vector<double> emit(ns);
  for (std::size_t f = 0; f < t; ++f) {
    // Emissions (log), with a tiny floor so no state is impossible.
    std::fill(emit.begin(), emit.end(), 0.0);
    double total = 0.0;
    for (const auto& c : cands[f]) {
      const auto b = binOf(c.frequencyHz);
      if (b < 0 || static_cast<std::size_t>(b) >= m) continue;
      emit[static_cast<std::size_t>(b)] += c.probability;
      total += c.probability;
    }
    const double unv = std::max(1.0 - total, 0.0) / static_cast<double>(m);
    for (std::size_t b = 0; b < m; ++b) {
      emit[b] = std::log(std::max(emit[b], 1e-12));
      emit[m + b] = std::log(std::max(unv, 1e-12));
    }
    if (f == 0) {
      for (std::size_t sidx = 0; sidx < ns; ++sidx) score[sidx] += emit[sidx];
      continue;
    }
    for (std::size_t sidx = 0; sidx < ns; ++sidx) {
      const bool voiced = sidx < m;
      const auto b = static_cast<std::ptrdiff_t>(voiced ? sidx : sidx - m);
      double best = neg;
      std::uint32_t arg = 0;
      for (std::ptrdiff_t d = -jump; d <= jump; ++d) {
        const std::ptrdiff_t pb = b + d;
        if (pb < 0 || static_cast<std::size_t>(pb) >= m) continue;
        const double tr = tri[static_cast<std::size_t>(d + jump)];
        const auto same = static_cast<std::size_t>(voiced ? pb : pb + static_cast<std::ptrdiff_t>(m));
        const auto other = static_cast<std::size_t>(voiced ? pb + static_cast<std::ptrdiff_t>(m) : pb);
        const double a = score[same] + tr + stay;
        const double c = score[other] + tr + sw;
        if (a > best) {
          best = a;
          arg = static_cast<std::uint32_t>(same);
        }
        if (c > best) {
          best = c;
          arg = static_cast<std::uint32_t>(other);
        }
      }
      next[sidx] = best + emit[sidx];
      back[f][sidx] = arg;
    }
    // Renormalise to keep scores finite over long tracks.
    const double mx = *std::max_element(next.begin(), next.end());
    for (std::size_t sidx = 0; sidx < ns; ++sidx) score[sidx] = next[sidx] - mx;
  }
  std::vector<std::size_t> path(t);
  path[t - 1] = static_cast<std::size_t>(
      std::max_element(score.begin(), score.end()) - score.begin());
  for (std::size_t f = t - 1; f > 0; --f) path[f - 1] = back[f][path[f]];
  std::vector<PitchFrame> out(t);
  for (std::size_t f = 0; f < t; ++f) {
    const bool voiced = path[f] < m;
    const std::size_t b = voiced ? path[f] : path[f] - m;
    double best = freqOf(b), bestDist = 1.5, prob = 0.0;
    for (const auto& c : cands[f]) {
      const double dist = std::fabs(12.0 * std::log2(c.frequencyHz / freqOf(b)) * o.binsPerSemitone);
      if (dist <= bestDist) {
        bestDist = dist;
        best = c.frequencyHz;
        prob = c.probability;
      }
    }
    PitchEstimate e;
    e.voiced = voiced;
    e.frequencyHz = best;
    e.periodSamples = rate.value() / best;
    e.confidence = prob;
    out[f] = {times[f], e};
  }
  return R::success(std::move(out));
}

// pYIN track: frames of frameSize every hop samples (as trackPitch).
[[nodiscard]] inline Result<std::vector<PitchFrame>, QuantityError> pyinTrack(
    const std::vector<double>& samples, SampleRateHz rate, std::size_t frameSize,
    std::size_t hop, const PyinOptions& o = {}) {
  using R = Result<std::vector<PitchFrame>, QuantityError>;
  if (hop < 1 || frameSize < 16 || samples.size() < frameSize ||
      !(o.fMinHz > 0.0) || !(o.fMaxHz > o.fMinHz) || o.binsPerSemitone < 1 ||
      o.binsPerSemitone > 100 || o.maxJumpBins < 1 || !(o.switchProb > 0.0) ||
      !(o.switchProb < 1.0) || !(o.betaA > 0.0) || !(o.betaB > 0.0) ||
      !(o.pAbsolute >= 0.0))
    return R::failure(QuantityError::InvalidArgument);
  for (double v : samples)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  // Stage 1.
  std::vector<std::vector<PitchCandidate>> cands;
  std::vector<double> times;
  std::vector<double> frame(frameSize);
  for (std::size_t start = 0; start + frameSize <= samples.size(); start += hop) {
    std::copy(samples.begin() + static_cast<std::ptrdiff_t>(start),
              samples.begin() + static_cast<std::ptrdiff_t>(start + frameSize), frame.begin());
    auto c = detail::pyinCandidates(frame, rate, o);
    if (!c.ok()) return R::failure(c.error());
    cands.push_back(std::move(c).value());
    times.push_back((static_cast<double>(start) + 0.5 * static_cast<double>(frameSize)) /
                    rate.value());
  }
  return pyinDecode(cands, times, rate, o);
}

}  // namespace mmathen
