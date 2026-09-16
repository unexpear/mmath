#pragma once

// D3 — commas as exact rational arithmetic.
//
// A comma is a small just interval stored as a REDUCED numerator/denominator
// pair (int64), never a float: syntonic/pythagorean relations then hold
// EXACTLY (e.g. pythagorean / syntonic == schisma, verified to all 50
// mpmath digits). Arithmetic is overflow-checked (RangeError); construction
// validates positivity. Prime limits and Tenney heights come from trial
// division — O(sqrt(n)), instant for musical ratios, documented cost.
// Named constants and their cents values cross-checked against Wikipedia /
// Xenharmonic Wiki / Duffin (MTO): syntonic 81/80 = 21.5062895967,
// pythagorean 531441/524288 = 23.4600103846, schisma 32805/32768 =
// 1.9537207879, diaschisma 2048/2025 = 19.5525688088, lesser diesis
// 128/125 = 41.0588584055, greater diesis 648/625 = 62.5651480022,
// septimal comma 64/63 = 27.2640918001.

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "../core/result.h"
#include "../units/music_units.h"

namespace mmathen {

struct Comma {
  std::int64_t num = 1;  // > 0, reduced against den
  std::int64_t den = 1;  // > 0

  bool operator==(const Comma&) const = default;
};

namespace detail {
inline std::int64_t gcd64(std::int64_t a, std::int64_t b) {
  while (b != 0) {
    const std::int64_t r = a % b;
    a = b;
    b = r;
  }
  return a < 0 ? -a : a;
}
// True on overflow of a*b for non-negative int64 (division check: portable,
// no __int128 so MSVC stays supported).
inline bool mulOverflows(std::int64_t a, std::int64_t b) {
  if (a == 0 || b == 0) return false;
  return a > (std::numeric_limits<std::int64_t>::max)() / b;
}
}  // namespace detail

[[nodiscard]] inline Result<Comma, QuantityError> makeComma(std::int64_t num,
                                                            std::int64_t den) {
  using R = Result<Comma, QuantityError>;
  if (num <= 0 || den <= 0)
    return R::failure(QuantityError::InvalidArgument);
  const std::int64_t g = detail::gcd64(num, den);
  return R::success(Comma{num / g, den / g});
}

// Exact product (reduced); overflow is RangeError, never wraparound.
[[nodiscard]] inline Result<Comma, QuantityError> multiplyComma(const Comma& a,
                                                                const Comma& b) {
  using R = Result<Comma, QuantityError>;
  if (detail::mulOverflows(a.num, b.num) ||
      detail::mulOverflows(a.den, b.den))
    return R::failure(QuantityError::RangeError);
  return makeComma(a.num * b.num, a.den * b.den);
}

// Exact quotient (reduced); same overflow policy.
[[nodiscard]] inline Result<Comma, QuantityError> divideComma(const Comma& a,
                                                              const Comma& b) {
  using R = Result<Comma, QuantityError>;
  if (detail::mulOverflows(a.num, b.den) ||
      detail::mulOverflows(a.den, b.num))
    return R::failure(QuantityError::RangeError);
  return makeComma(a.num * b.den, a.den * b.num);
}

// cents = 1200*log2(num/den). The ratio of int64 values is always finite,
// so this reports RangeError only defensively.
[[nodiscard]] inline Result<Cents, QuantityError> commaCents(const Comma& c) {
  using R = Result<Cents, QuantityError>;
  const double v =
      std::log2(static_cast<double>(c.num) / static_cast<double>(c.den));
  if (!std::isfinite(v)) return R::failure(QuantityError::RangeError);
  return R::success(Cents{1200.0 * v});
}

// Largest prime dividing num or den (the prime limit). Trial division with
// a 6k±1 wheel, O(sqrt(n)) worst case — microseconds for musical ratios;
// documented cost for the rest. Unison (1/1) reports 2 by vacuous-limit
// convention (the unison lives in every limit).
[[nodiscard]] inline Result<int, QuantityError> primeLimit(const Comma& c) {
  using R = Result<int, QuantityError>;
  long long best = 2;
  // Both sides: the limit of 81/80 is 5 (from the denominator).
  for (std::int64_t v : {c.num, c.den}) {
    std::int64_t rest = v;
    auto strip = [&](long long p) {
      if (rest % p == 0) {
        if (p > best) best = p;
        do {
          rest /= p;
        } while (rest % p == 0);
      }
    };
    strip(2);
    strip(3);
    for (long long p = 5; p <= rest / p; p += 6) {
      strip(p);
      strip(p + 2);
    }
    if (rest > 1) {
      if (rest > 2147483647LL) return R::failure(QuantityError::RangeError);
      if (rest > best) best = rest;
    }
  }
  return R::success(static_cast<int>(best));
}

// Tenney harmonic distance: log2(num*den), via log-sum (no product overflow).
[[nodiscard]] inline Result<double, QuantityError> tenneyHeight(
    const Comma& c) {
  using R = Result<double, QuantityError>;
  const double h =
      std::log2(static_cast<double>(c.num)) + std::log2(static_cast<double>(c.den));
  if (!std::isfinite(h)) return R::failure(QuantityError::RangeError);
  return R::success(h);
}

namespace commas {
inline Comma syntonic() { return Comma{81, 80}; }            // 21.5063c
inline Comma pythagorean() { return Comma{531441, 524288}; }  // 23.4600c
inline Comma schisma() { return Comma{32805, 32768}; }        // 1.9537c
inline Comma diaschisma() { return Comma{2048, 2025}; }       // 19.5526c
inline Comma lesserDiesis() { return Comma{128, 125}; }       // 41.0589c
inline Comma greaterDiesis() { return Comma{648, 625}; }      // 62.5651c
inline Comma septimal() { return Comma{64, 63}; }             // 27.2641c
}  // namespace commas

}  // namespace mmathen
