#pragma once

// D4 — voice-leading cost model (simplified, honestly labeled).
//
// In the lineage of Tymoczko's geometric voice-leading work, but deliberately
// smaller: cost = total semitone motion (L1) plus a fixed parallel-fifth /
// parallel-octave penalty. Weights below are OURS (documented, not quoted
// from any paper): parallelPenalty = 12 (one octave of motion — a parallel
// fifth costs as much as moving every voice an octave). No claim is made
// that this models perception; it is a deterministic comparator for
// choosing among candidate voicings. MIDI range 0..127 enforced.

#include <cstdlib>
#include <vector>

#include "../core/result.h"

namespace mmathen {

inline constexpr int kParallelPenalty = 12;

struct VoiceLeading {
  int totalMotion = 0;       // sum |Δ| in semitones, exact
  int parallelFifths = 0;    // count of offending voice pairs
  int parallelOctaves = 0;   // count of offending voice pairs
  int cost = 0;              // totalMotion + penalty*(fifths+octaves)

  bool operator==(const VoiceLeading&) const = default;
};

namespace detail {
// Unordered interval class 0..6 (fifth/fourth = 5, octave/unison = 0),
// independent of voice order: with voices (60,67) or (67,60) the pair is
// ic5 either way. Directed mod-12 differences would flip between 5 and 7
// with order — that bug class is why this normalization exists.
inline int intervalClass(int a, int b) {
  const int d = std::abs(a - b) % 12;
  return d <= 6 ? d : 12 - d;
}
inline bool movesParallel(const std::vector<int>& a, const std::vector<int>& b,
                          std::size_t i, std::size_t j, int ic) {
  if (intervalClass(a[i], a[j]) != ic) return false;
  if (intervalClass(b[i], b[j]) != ic) return false;
  const int di = b[i] - a[i];
  const int dj = b[j] - a[j];
  if (di == 0 && dj == 0) return false;  // oblique/static is not parallel
  return (di > 0 && dj > 0) || (di < 0 && dj < 0);
}
}  // namespace detail

// Both voicings: equal-length MIDI arrays in [0,127]. Motion must be
// voice-matched (voice i -> voice i); permutation search is deferred.
[[nodiscard]] inline Result<VoiceLeading, QuantityError> voiceLeadingCost(
    const std::vector<int>& from, const std::vector<int>& to) {
  using R = Result<VoiceLeading, QuantityError>;
  if (from.size() != to.size() || from.empty())
    return R::failure(QuantityError::InvalidArgument);
  for (int m : from) {
    if (m < 0 || m > 127) return R::failure(QuantityError::InvalidArgument);
  }
  for (int m : to) {
    if (m < 0 || m > 127) return R::failure(QuantityError::InvalidArgument);
  }
  VoiceLeading out;
  for (std::size_t i = 0; i < from.size(); ++i)
    out.totalMotion += std::abs(to[i] - from[i]);
  for (std::size_t i = 0; i < from.size(); ++i) {
    for (std::size_t j = i + 1; j < from.size(); ++j) {
      if (detail::movesParallel(from, to, i, j, 5)) ++out.parallelFifths;
      if (detail::movesParallel(from, to, i, j, 0)) ++out.parallelOctaves;
    }
  }
  out.cost = out.totalMotion +
             kParallelPenalty * (out.parallelFifths + out.parallelOctaves);
  return R::success(out);
}

}  // namespace mmathen
