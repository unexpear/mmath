#pragma once
// Performance-rule dynamics: a per-note dynamic in [0, 1] computed from the
// score, deterministic except for a small smoothed random term.
//
// Rules (KTH performance rule system: Friberg, Bresin & Sundberg, "Overview
// of the KTH rule system for musical performance", Adv. Cogn. Psychol. 2,
// 2006; Sundberg, Friberg & Fryden 1991):
//  - High loud: level rises with pitch (KTH "high loud").
//  - Phrase arch: crescendo to a point late in the phrase, then decrescendo,
//    with phrases ended at score boundaries (a note much longer than its
//    neighbours, or a rest), not at a fixed note count (KTH "phrase arch",
//    Friberg 1995; measured arch shapes in pianists: Repp, JASA 1992 / 1999).
//  - Accent: the score's velocity (notated accents / first-of-group).
//  - Small random variation, smoothed over notes, then a zero-phase moving
//    mean of the whole sequence is removed (flatten) so there is no
//    clip-long drift; the mean dynamic is restored afterwards.
// Weights (dB-equivalent in dynamic units) are UNSOURCED / TUNED; the KTH
// papers give rule shapes, and the default quantities here are VERIFY.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mmathen {

struct DynamicsNote {
  double onsetSeconds, ioiSeconds;
  int midi, velocity;
};

struct PerformanceDynamicsParams {
  double base = 0.45;              // mean dynamic
  double highLoudPerOctave = 0.12; // UNSOURCED (KTH shape)
  double archDepth = 0.12;         // UNSOURCED (KTH shape), peak-to-ends
  double archPeak = 0.6;           // fraction of phrase at the arch peak (Friberg 1995 VERIFY)
  double boundaryRatio = 1.8;      // ioi > ratio x local median ends a phrase (UNSOURCED)
  double accentPerVelocity = 0.0175;  // per velocity step re 81 (TUNED)
  double randomSd = 0.03, randomTauSeconds = 1.0;  // small, smoothed (TUNED)
  double flattenSeconds = 3.0;     // zero-phase moving mean removed (0 = off)
  // Metrical emphasis: lean toward strong beats, do not accent weak ones
  // (pedagogy: Galamian 1962; Fischer, Basics 1997; KTH 'beat stress').
  // beatSeconds 0 = off; weight UNSOURCED.
  double beatSeconds = 0.0, strongBeatWeight = 0.04;
  // Repetition variation: each phrase gets its own level (players vary a
  // repeated phrase rather than copy it: Repp, JASA 92, 1992, repeat
  // variation; terraced/echo dynamics). Seeded, constant within a phrase,
  // sd UNSOURCED. 0 = off.
  double phraseLevelSd = 0.0;
  // Arch-shape variation per repetition: each phrase draws its own arch depth
  // (x (1 + sd g)) and peak position (+/- sd/2 g, clamped 0.2..0.85), since a
  // repeated phrase is shaped differently each time, not just re-levelled
  // (Repp, JASA 92, 1992: timing/dynamic profiles of repeats differ;
  // Repp, JASA 105, 1999 dynamics). Stops a fixed-period arch cycle. sd
  // UNSOURCED. 0 = off (no extra random draws, old seeds unchanged).
  double archShapeSd = 0.0;
  // String crossing: a note on a different string from the previous one is
  // played through an arm/elbow level change, so the bow arrives with less
  // settled weight and the first note on the new string is lighter
  // (Galamian 1962, string crossings from the arm level; Fischer, Basics
  // 1997, string-crossing exercises; Rolland, Prelude to String Playing).
  // String from first position (G3 55, D4 62, A4 69, E5 76; lowest string
  // that can reach, VERIFY vs fingering). Dip per crossing, scaled by the
  // number of strings crossed, capped at 2. Weight UNSOURCED (no measured
  // crossing data found). 0 = off.
  double stringCrossingDip = 0.0;
};

[[nodiscard]] inline std::vector<double> performanceDynamics(const std::vector<DynamicsNote>& notes,
                                                             PerformanceDynamicsParams p = {},
                                                             std::uint64_t seed = 1) {
  const std::size_t n = notes.size();
  std::vector<double> out(n, p.base);
  if (n == 0) return out;
  // Median pitch for high-loud.
  std::vector<int> m(n);
  for (std::size_t i = 0; i < n; ++i) m[i] = notes[i].midi;
  std::vector<int> ms = m;
  std::nth_element(ms.begin(), ms.begin() + n / 2, ms.end());
  const double med = ms[n / 2];
  // Phrase boundaries from long notes.
  std::vector<std::size_t> starts{0};
  for (std::size_t i = 1; i + 1 < n; ++i) {
    std::vector<double> w;
    for (std::size_t j = (i > 4 ? i - 4 : 0); j < std::min(n, i + 5); ++j) w.push_back(notes[j].ioiSeconds);
    std::nth_element(w.begin(), w.begin() + w.size() / 2, w.end());
    if (notes[i].ioiSeconds > p.boundaryRatio * w[w.size() / 2]) starts.push_back(i + 1);
  }
  starts.push_back(n);
  std::vector<double> raw(n);
  std::uint64_t s = seed ? seed : 1;
  auto un = [&] { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (static_cast<double>(s >> 11) + 0.5) / 9007199254740992.0; };
  auto gauss = [&] { return std::sqrt(-2.0 * std::log(un())) * std::cos(6.283185307179586 * un()); };
  for (std::size_t k = 0; k + 1 < starts.size(); ++k) {
    const std::size_t a = starts[k], b = starts[k + 1];
    const double lvl = p.phraseLevelSd * gauss();
    double depth = p.archDepth, peak = p.archPeak;
    if (p.archShapeSd > 0.0) {
      depth *= std::max(0.0, 1.0 + p.archShapeSd * gauss());
      peak = std::clamp(peak + 0.5 * p.archShapeSd * gauss(), 0.2, 0.85);
    }
    const double t0 = notes[a].onsetSeconds, t1 = notes[b - 1].onsetSeconds + notes[b - 1].ioiSeconds;
    for (std::size_t i = a; i < b; ++i) {
      const double x = (t1 > t0) ? (notes[i].onsetSeconds - t0) / (t1 - t0) : 0.5;
      const double arch = x < peak ? x / peak : (1.0 - x) / (1.0 - peak);
      raw[i] = depth * (arch - 0.5) + lvl;
    }
  }
  double r = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    const double a = std::exp(-notes[i].ioiSeconds / p.randomTauSeconds);
    r = a * r + std::sqrt(1.0 - a * a) * p.randomSd * std::sqrt(-2.0 * std::log(un())) * std::cos(6.283185307179586 * un());
    if (p.beatSeconds > 0.0) {
      const double ph = notes[i].onsetSeconds / p.beatSeconds;
      raw[i] += std::fabs(ph - std::round(ph)) < 0.08 ? p.strongBeatWeight : -0.25 * p.strongBeatWeight;
    }
    if (p.stringCrossingDip > 0.0 && i > 0) {
      auto str = [](int midi) { return midi >= 76 ? 3 : midi >= 69 ? 2 : midi >= 62 ? 1 : 0; };
      const int ds = std::abs(str(m[i]) - str(m[i - 1]));
      raw[i] -= p.stringCrossingDip * std::min(ds, 2);
    }
    raw[i] += p.highLoudPerOctave * (m[i] - med) / 12.0 + p.accentPerVelocity * (notes[i].velocity - 81) + r;
  }
  for (std::size_t i = 0; i < n; ++i) {
    double mean = 0.0; int c = 0;
    if (p.flattenSeconds > 0.0)
      for (std::size_t j = 0; j < n; ++j)
        if (std::fabs(notes[j].onsetSeconds - notes[i].onsetSeconds) <= p.flattenSeconds / 2) { mean += raw[j]; ++c; }
    out[i] = std::clamp(p.base + raw[i] - (c ? mean / c : 0.0), 0.0, 1.0);
  }
  return out;
}

}  // namespace mmathen
