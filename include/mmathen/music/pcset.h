#pragma once

// D4 — pitch-class sets: Forte prime forms, interval vectors, equivalence.
//
// A PitchClassSet is sorted unique pcs in [0,11]. Normal order follows
// Straus (rotations to zero, smallest span, tie-break most packed to the
// left); prime form follows Forte (packed-left winner of normal vs
// inverted-normalized, both transposed to zero), verified 1:1 against
// music21 for the whole catalogue (see docs/18-d4). Variant labels follow
// Forte: A = prime side, B = inversion side; symmetric sets carry the bare
// number. Rahn spellings are intentionally NOT used; the 17 differing
// classes are documented in forte_table.h.

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "forte_table.h"

namespace mmathen {

struct PitchClassSet {
  std::vector<int> pcs;  // sorted, unique, each in [0,11]

  bool operator==(const PitchClassSet&) const = default;
};

[[nodiscard]] inline Result<PitchClassSet, QuantityError> makePitchClassSet(
    std::vector<int> pcs) {
  using R = Result<PitchClassSet, QuantityError>;
  for (int p : pcs) {
    if (p < 0 || p > 11) return R::failure(QuantityError::InvalidArgument);
  }
  std::sort(pcs.begin(), pcs.end());
  pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
  return R::success(PitchClassSet{std::move(pcs)});
}

namespace pcset_detail {
inline int mod12(int x) {
  int r = x % 12;
  return r < 0 ? r + 12 : r;
}
inline std::vector<int> transposeToZero(const std::vector<int>& pcs) {
  std::vector<int> out;
  out.reserve(pcs.size());
  for (int p : pcs) out.push_back(mod12(p - pcs.front()));
  return out;
}
}  // namespace pcset_detail

[[nodiscard]] inline PitchClassSet transposePcSet(const PitchClassSet& s,
                                                  int n) {
  std::vector<int> out;
  out.reserve(s.pcs.size());
  for (int p : s.pcs) out.push_back(pcset_detail::mod12(p + n));
  std::sort(out.begin(), out.end());
  return PitchClassSet{std::move(out)};
}

[[nodiscard]] inline PitchClassSet invertPcSet(const PitchClassSet& s) {
  std::vector<int> out;
  out.reserve(s.pcs.size());
  for (int p : s.pcs) out.push_back(pcset_detail::mod12(-p));
  std::sort(out.begin(), out.end());
  return PitchClassSet{std::move(out)};
}

// Interval-class vector: counts of interval classes 1..6 over all pairs.
[[nodiscard]] inline std::array<int, 6> intervalClassVector(
    const PitchClassSet& s) {
  std::array<int, 6> v = {0, 0, 0, 0, 0, 0};
  for (std::size_t i = 0; i < s.pcs.size(); ++i) {
    for (std::size_t j = i + 1; j < s.pcs.size(); ++j) {
      int d = s.pcs[j] - s.pcs[i];  // sorted: 1..11
      if (d > 6) d = 12 - d;
      v[static_cast<std::size_t>(d - 1)] += 1;
    }
  }
  return v;
}

// Normal order: rotations transposed to zero, smallest span wins; ties go
// to the most packed-left (lexicographically smallest from index 1).
[[nodiscard]] inline std::vector<int> normalOrder(const PitchClassSet& s) {
  if (s.pcs.empty()) return {};
  std::vector<int> best;
  bool haveBest = false;
  for (std::size_t r = 0; r < s.pcs.size(); ++r) {
    std::vector<int> rot;
    rot.reserve(s.pcs.size());
    for (std::size_t i = 0; i < s.pcs.size(); ++i)
      rot.push_back(s.pcs[(r + i) % s.pcs.size()]);
    auto cand = pcset_detail::transposeToZero(rot);
    // Smallest span first; ties go packed-left (lexicographic from index 1;
    // index 0 is always 0). Plain lexicographic compare would be WRONG here:
    // e.g. [0,1,2,7] (span 7) sorts before [0,2,3,4] (span 4).
    const bool better =
        !haveBest || cand.back() < best.back() ||
        (cand.back() == best.back() &&
         std::lexicographical_compare(cand.begin() + 1, cand.end(),
                                       best.begin() + 1, best.end()));
    if (better) {
      best = std::move(cand);
      haveBest = true;
    }
  }
  return best;
}

// Prime form (Forte): packed-left winner of normal vs inverted-normalized.
[[nodiscard]] inline std::vector<int> primeForm(const PitchClassSet& s) {
  if (s.pcs.empty()) return {};
  const std::vector<int> n0 = normalOrder(s);
  PitchClassSet inv{{}};
  inv.pcs.reserve(n0.size());
  for (int p : n0) inv.pcs.push_back(pcset_detail::mod12(-p));
  std::sort(inv.pcs.begin(), inv.pcs.end());
  const std::vector<int> i0 = normalOrder(inv);
  const std::vector<int> ni0 = pcset_detail::transposeToZero(i0);
  return ni0 < n0 ? ni0 : n0;
}

namespace pcset_detail {
inline std::string pcsToKey(const std::vector<int>& pcs) {
  std::string key;
  for (int p : pcs) {
    if (p < 10) {
      key += static_cast<char>('0' + p);
    } else if (p == 10) {
      key += 'T';
    } else {
      key += 'E';
    }
  }
  return key;
}
inline int icvChar(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c == 'T') return 10;
  if (c == 'E') return 11;
  return 12;  // 'C'
}
}  // namespace pcset_detail

// Forte number with variant, e.g. "3-11B" (symmetric sets: bare "3-12").
// The table is complete for 0..12 pcs, so lookup failure is DomainError
// (defensive; indicates a table/algorithm inconsistency worth reporting).
[[nodiscard]] inline Result<std::string, QuantityError> forteNumber(
    const PitchClassSet& s) {
  using R = Result<std::string, QuantityError>;
  const std::string key = pcset_detail::pcsToKey(primeForm(s));
  const ForteEntry* hit = nullptr;
  for (const auto& e : kForteTable) {
    if (e.prime == key) {
      hit = &e;
      break;
    }
  }
  if (hit == nullptr) return R::failure(QuantityError::DomainError);
  std::string number(hit->number);
  if (!number.empty() &&
      (number.back() == 'A' || number.back() == 'B')) {
    // Asymmetric class: the input's normalized spelling decides the side.
    // N0t equals the A prime for prime-side inputs, the B prime otherwise;
    // anything else is an internal inconsistency (DomainError canary).
    const std::string base = number.substr(0, number.size() - 1);
    const std::string n0key =
        pcset_detail::pcsToKey(pcset_detail::transposeToZero(normalOrder(s)));
    std::string aPrime;
    std::string bPrime;
    for (const auto& e : kForteTable) {
      const std::string_view num = e.number;
      if (num.size() == base.size() + 1 && num.substr(0, base.size()) == base) {
        if (num.back() == 'A') aPrime = std::string(e.prime);
        if (num.back() == 'B') bPrime = std::string(e.prime);
      }
    }
    if (n0key == aPrime) {
      number = base + "A";
    } else if (n0key == bPrime) {
      number = base + "B";
    } else {
      return R::failure(QuantityError::DomainError);
    }
  }
  return R::success(number);
}

// Exact relations (not class membership): some transposition / inversion
// maps one set onto the other.
[[nodiscard]] inline bool isTranspositionOf(const PitchClassSet& a,
                                            const PitchClassSet& b) {
  if (a.pcs.size() != b.pcs.size()) return false;
  for (int n = 0; n < 12; ++n) {
    if (transposePcSet(a, n) == b) return true;
  }
  return false;
}

[[nodiscard]] inline bool isInversionOf(const PitchClassSet& a,
                                        const PitchClassSet& b) {
  return isTranspositionOf(invertPcSet(a), b);
}

[[nodiscard]] inline bool sameSetClass(const PitchClassSet& a,
                                       const PitchClassSet& b) {
  return primeForm(a) == primeForm(b);
}

}  // namespace mmathen
