#pragma once

// D4 — Euclidean rhythms (Bjorklund/Toussaint) + triplet swing map.
//
// euclideanRhythm(pulses, steps, rotation): distributes `pulses` onsets as
// evenly as possible over `steps` slots (Bjorklund's algorithm, cf.
// Toussaint 2005 "The Euclidean algorithm generates traditional musical
// rhythms"). E(3,8) = tresillo 10010010; rotation shifts the cycle.
// swingDelay: triplet-feel microtiming — off-beats (odd eighth slots) lag
// by amount * eighth/3 (amount 0 = straight, 1 = full triplet swing where
// the off-beat lands on the last triplet third). On-beats never move.

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

namespace detail {
// Recursive Bjorklund construction (Toussaint 2005): counts/remainders
// follow the Euclidean algorithm; build() assembles onset/rest groups.
// Result starts with an onset in all verified cases; the caller rotates to
// the first onset defensively (canonical form always begins with one).
inline void bjorklundBuild(int level, const std::vector<int>& counts,
                           const std::vector<int>& remainders,
                           std::vector<int>& out) {
  if (level == -1) {
    out.push_back(0);
  } else if (level == -2) {
    out.push_back(1);
  } else {
    for (int i = 0; i < counts[static_cast<std::size_t>(level)]; ++i)
      bjorklundBuild(level - 1, counts, remainders, out);
    if (remainders[static_cast<std::size_t>(level)] != 0)
      bjorklundBuild(level - 2, counts, remainders, out);
  }
}
}  // namespace detail

[[nodiscard]] inline Result<std::vector<int>, QuantityError> euclideanRhythm(
    int pulses, int steps, int rotation = 0) {
  using R = Result<std::vector<int>, QuantityError>;
  if (pulses < 1 || steps < 1 || pulses > steps)
    return R::failure(QuantityError::InvalidArgument);
  std::vector<int> counts;
  std::vector<int> remainders;
  int divisor = steps - pulses;
  remainders.push_back(pulses);
  int level = 0;
  for (;;) {
    counts.push_back(divisor / remainders[static_cast<std::size_t>(level)]);
    remainders.push_back(divisor % remainders[static_cast<std::size_t>(level)]);
    divisor = remainders[static_cast<std::size_t>(level)];
    ++level;
    if (remainders[static_cast<std::size_t>(level)] <= 1) break;
  }
  counts.push_back(divisor);
  std::vector<int> pattern;
  pattern.reserve(static_cast<std::size_t>(steps));
  detail::bjorklundBuild(level, counts, remainders, pattern);
  // Canonical form begins with an onset.
  std::size_t first = 0;
  while (first < pattern.size() && pattern[first] == 0) ++first;
  if (first < pattern.size() && first > 0)
    std::rotate(pattern.begin(), pattern.begin() + first, pattern.end());
  if (rotation != 0 && !pattern.empty()) {
    const int n = static_cast<int>(pattern.size());
    const int r = ((rotation % n) + n) % n;
    std::rotate(pattern.begin(), pattern.begin() + r, pattern.end());
  }
  return R::success(std::move(pattern));
}

// Lag for an eighth-note slot: 0 on-beats, amount*eighth/3 on off-beats.
// Amount in [0,1]; eighth must be finite and >= 0.
[[nodiscard]] inline Result<DurationSeconds, QuantityError> swingDelay(
    bool offbeat, DurationSeconds eighth, double amount) {
  using R = Result<DurationSeconds, QuantityError>;
  if (!std::isfinite(eighth.value()) || eighth.value() < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(amount) || amount < 0.0 || amount > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!offbeat) return R::success(DurationSeconds{0.0});
  return R::success(DurationSeconds{amount * eighth.value() / 3.0});
}

}  // namespace mmathen
