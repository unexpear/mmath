#pragma once

// D43 — chord recognition: FFT -> chroma -> template emissions -> HMM
// (optionally with an n-gram chord "language model") -> Viterbi.
//
// The classical pipeline (Fujishima 1999 pitch-class profiles; Sheh &
// Ellis 2003 HMM decoding; Cho & Bello 2014 on the value of each stage):
//   chroma   each FFT bin in [fMin, fMax] adds its power to the pitch class
//            of its frequency (A4 = tuningHz); log compression
//            log(1 + g c) (Mueller, FMP sec. 3.1), then unit L2 norm.
//   emission 24 major/minor triad templates plus "N" (no chord), unit L2;
//            score = cosine similarity; log b_j = sharpness * (cos_j - 1),
//            a tempered score used as a log-likelihood (stated: it is not
//            a fitted density). Frames below the energy gate score N = 1.
//   HMM      stay with probability p; on a change, go to chord j with the
//            probability of the chord bigram (ngram.h) or uniformly.
//   decode   Viterbi (hmm.h); frame-wise argmax is also returned so the
//            HMM's contribution can be measured.
// Chord indices: 0-11 major on C..B, 12-23 minor on C..B, 24 = N.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../stats/hmm.h"
#include "../units/physical_units.h"
#include "fft.h"

namespace mmathen {

inline constexpr std::size_t kChordCount = 25;
inline constexpr std::size_t kNoChord = 24;

[[nodiscard]] inline std::string chordName(std::size_t c) {
  static const char* const names[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  if (c >= kNoChord) return "N";
  return std::string(names[c % 12]) + (c >= 12 ? "m" : "");
}

// Pitch classes of chord c (root, third, fifth); empty for N.
[[nodiscard]] inline std::vector<int> chordPitchClasses(std::size_t c) {
  if (c >= kNoChord) return {};
  const int root = static_cast<int>(c % 12);
  const int third = c >= 12 ? 3 : 4;
  return {root, (root + third) % 12, (root + 7) % 12};
}

struct ChromaOptions {
  std::size_t frameSize = 8192;
  std::size_t hop = 2048;
  double fMinHz = 80.0;
  double fMaxHz = 5000.0;
  double tuningHz = 440.0;
  double compression = 100.0;  // g in log(1 + g c); 0 disables
  double energyGateDb = -60.0; // in-band power (dBFS, full-scale sine = 0) below which N
};

struct Chromagram {
  std::vector<std::array<double, 12>> frames;  // unit L2 (or zero when gated)
  std::vector<bool> gated;
  double hopSeconds = 0.0;
};

[[nodiscard]] inline Result<Chromagram, QuantityError> chromagram(const std::vector<double>& x,
                                                                  SampleRateHz rate,
                                                                  const ChromaOptions& o = {}) {
  using R = Result<Chromagram, QuantityError>;
  const double fs = rate.value();
  if (x.empty() || !isPowerOfTwo(o.frameSize) || o.frameSize < 256 || o.hop < 1 ||
      !(o.fMinHz > 0.0) || !(o.fMaxHz > o.fMinHz) || !(o.fMaxHz < fs / 2.0) ||
      !(o.tuningHz > 0.0) || !(o.compression >= 0.0))
    return R::failure(QuantityError::InvalidArgument);
  for (double v : x)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t n = o.frameSize;
  std::vector<double> win(n);
  double windowEnergy = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    win[i] = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n));
    windowEnergy += win[i] * win[i];
  }
  // Bin -> pitch class map.
  std::vector<int> pc(n / 2 + 1, -1);
  for (std::size_t k = 1; k <= n / 2; ++k) {
    const double f = static_cast<double>(k) * fs / static_cast<double>(n);
    if (f < o.fMinHz || f > o.fMaxHz) continue;
    const double midi = 69.0 + 12.0 * std::log2(f / o.tuningHz);
    pc[k] = ((static_cast<int>(std::lround(midi)) % 12) + 12) % 12;
  }
  Chromagram cg;
  cg.hopSeconds = static_cast<double>(o.hop) / fs;
  const std::size_t frames = x.size() <= n ? 1 : (x.size() - n) / o.hop + 1;
  std::vector<std::complex<double>> buf(n);
  // Sum of positive-frequency bin powers for a full-scale in-band sine:
  // Parseval gives N * sum(w^2) / 2 over the full DFT, half at positive bins.
  // Use window energy, not peak-bin amplitude gain, when summing powers.
  const double ref = static_cast<double>(n) * windowEnergy / 4.0;
  for (std::size_t f = 0; f < frames; ++f) {
    const std::size_t s = f * o.hop;
    for (std::size_t i = 0; i < n; ++i) buf[i] = {s + i < x.size() ? x[s + i] * win[i] : 0.0, 0.0};
    detail::fftInPlace(buf, false);
    std::array<double, 12> c{};
    double total = 0.0;
    for (std::size_t k = 1; k <= n / 2; ++k) {
      if (pc[k] < 0) continue;
      const double p = std::norm(buf[k]) / ref;
      c[static_cast<std::size_t>(pc[k])] += p;
      total += p;
    }
    const bool gate = !(total > 0.0) || 10.0 * std::log10(total) < o.energyGateDb;
    if (!gate && o.compression > 0.0) {
      double mx = 0.0;
      for (double v : c) mx = std::max(mx, v);
      for (auto& v : c) v = std::log1p(o.compression * v / mx);
    }
    double norm = 0.0;
    for (double v : c) norm += v * v;
    norm = std::sqrt(norm);
    for (auto& v : c) v = gate || !(norm > 0.0) ? 0.0 : v / norm;
    cg.frames.push_back(c);
    cg.gated.push_back(gate);
  }
  return R::success(std::move(cg));
}

struct ChordOptions {
  ChromaOptions chroma;
  double sharpness = 10.0;       // log b = sharpness * (cos - 1)
  double noChordPenalty = 0.2;   // subtracted from N's cosine on ungated frames (measured choice)
  double selfTransition = 0.9;   // per frame
  // Optional chord bigram: P(next = j | current = i) over the 25 chords,
  // rows summing to 1 (e.g. from an NgramModel). Empty = uniform changes.
  std::vector<std::vector<double>> changeModel;
};

struct ChordRecognition {
  std::vector<std::size_t> framewise;  // argmax of the emission per frame
  std::vector<std::size_t> viterbi;    // HMM decoding
  LogMatrix logEmissions;              // frames x 25
  double hopSeconds = 0.0;
};

// Emission scores of one chroma frame for the 25 chords.
[[nodiscard]] inline std::array<double, kChordCount> chordScores(const std::array<double, 12>& c,
                                                                 bool gated, double noChordPenalty = 0.0) {
  std::array<double, kChordCount> s{};
  if (gated) {
    s.fill(0.0);
    s[kNoChord] = 1.0;
    return s;
  }
  for (std::size_t j = 0; j < kNoChord; ++j) {
    double d = 0.0;
    for (int p : chordPitchClasses(j)) d += c[static_cast<std::size_t>(p)];
    s[j] = d / std::sqrt(3.0);
  }
  double flat = 0.0;
  for (double v : c) flat += v;
  s[kNoChord] = flat / std::sqrt(12.0) - noChordPenalty;
  return s;
}

[[nodiscard]] inline Result<ChordRecognition, QuantityError> recognizeChords(
    const std::vector<double>& x, SampleRateHz rate, const ChordOptions& o = {}) {
  using R = Result<ChordRecognition, QuantityError>;
  if (!(o.sharpness > 0.0) || !(o.selfTransition > 0.0) || !(o.selfTransition < 1.0))
    return R::failure(QuantityError::InvalidArgument);
  auto cgr = chromagram(x, rate, o.chroma);
  if (!cgr.ok()) return R::failure(cgr.error());
  const auto cg = std::move(cgr).value();
  // Transition matrix.
  std::vector<std::vector<double>> a(kChordCount, std::vector<double>(kChordCount, 0.0));
  for (std::size_t i = 0; i < kChordCount; ++i) {
    std::vector<double> change(kChordCount, 1.0);
    if (!o.changeModel.empty()) {
      if (o.changeModel.size() != kChordCount || o.changeModel[i].size() != kChordCount)
        return R::failure(QuantityError::InvalidArgument);
      change = o.changeModel[i];
    }
    change[i] = 0.0;
    double s = 0.0;
    for (double v : change) {
      if (!(v >= 0.0)) return R::failure(QuantityError::InvalidArgument);
      s += v;
    }
    if (!(s > 0.0)) return R::failure(QuantityError::InvalidArgument);
    for (std::size_t j = 0; j < kChordCount; ++j)
      a[i][j] = i == j ? o.selfTransition : (1.0 - o.selfTransition) * change[j] / s;
  }
  auto hr = makeHmm(std::vector<double>(kChordCount, 1.0 / static_cast<double>(kChordCount)), a);
  if (!hr.ok()) return R::failure(hr.error());
  ChordRecognition res;
  res.hopSeconds = cg.hopSeconds;
  for (std::size_t f = 0; f < cg.frames.size(); ++f) {
    const auto sc = chordScores(cg.frames[f], cg.gated[f], o.noChordPenalty);
    std::vector<double> lb(kChordCount);
    std::size_t best = 0;
    for (std::size_t j = 0; j < kChordCount; ++j) {
      lb[j] = o.sharpness * (sc[j] - 1.0);
      if (sc[j] > sc[best]) best = j;
    }
    res.framewise.push_back(best);
    res.logEmissions.push_back(std::move(lb));
  }
  auto v = viterbi(hr.value(), res.logEmissions);
  if (!v.ok()) return R::failure(v.error());
  res.viterbi = std::move(v).value().states;
  return R::success(std::move(res));
}

struct ChordSegment {
  std::size_t chord = kNoChord;
  double startSeconds = 0.0, endSeconds = 0.0;
};
// Merge a frame labelling into segments (frame f covers
// [f hop, (f+1) hop), the analysis window's leading edge convention).
[[nodiscard]] inline std::vector<ChordSegment> chordSegments(const std::vector<std::size_t>& labels,
                                                             double hopSeconds) {
  std::vector<ChordSegment> seg;
  for (std::size_t f = 0; f < labels.size(); ++f) {
    const double t0 = static_cast<double>(f) * hopSeconds;
    if (seg.empty() || seg.back().chord != labels[f]) seg.push_back({labels[f], t0, t0 + hopSeconds});
    else seg.back().endSeconds = t0 + hopSeconds;
  }
  return seg;
}

}  // namespace mmathen
