#pragma once

// D40 — a small, portable pseudo-random source for the statistical models
// (GMM initialisation, Markov/HMM/n-gram sampling).
//
// std::mt19937 is portable, but the std:: distributions are not: libstdc++,
// libc++ and MSVC produce different normal/uniform variates from the same
// engine. Tests that pin sampled results must agree on every compiler, so
// the engine (SplitMix64, Steele, Lea & Flood 2014) and the transforms
// (53-bit uniform, Box-Muller normal, inverse-CDF categorical) are defined
// here.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

namespace mmathen {

class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed = 0x243F6A8885A308D3ULL) noexcept : s_{seed} {}
  std::uint64_t next() noexcept {
    std::uint64_t z = (s_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
  // Uniform on [0, 1), 53 random bits.
  double uniform() noexcept { return static_cast<double>(next() >> 11) * 0x1.0p-53; }
  // Standard normal (Box-Muller; the second variate is kept).
  double normal() noexcept {
    if (hasSpare_) {
      hasSpare_ = false;
      return spare_;
    }
    double u1 = uniform();
    while (u1 <= 0.0) u1 = uniform();
    const double u2 = uniform();
    const double r = std::sqrt(-2.0 * std::log(u1));
    spare_ = r * std::sin(2.0 * std::numbers::pi * u2);
    hasSpare_ = true;
    return r * std::cos(2.0 * std::numbers::pi * u2);
  }
  // Index drawn from non-negative weights (need not sum to 1). Returns
  // weights.size() - 1 for an all-zero vector's degenerate case.
  std::size_t categorical(const std::vector<double>& weights) noexcept {
    double total = 0.0;
    for (double w : weights) total += w > 0.0 ? w : 0.0;
    double u = uniform() * total;
    for (std::size_t i = 0; i < weights.size(); ++i) {
      const double w = weights[i] > 0.0 ? weights[i] : 0.0;
      if (u < w) return i;
      u -= w;
    }
    // Roundoff: return the last index with positive weight.
    for (std::size_t i = weights.size(); i-- > 0;)
      if (weights[i] > 0.0) return i;
    return weights.empty() ? 0 : weights.size() - 1;
  }

 private:
  std::uint64_t s_;
  double spare_ = 0.0;
  bool hasSpare_ = false;
};

// log(sum(exp(v))) without overflow; -inf for an empty or all -inf input.
[[nodiscard]] inline double logSumExp(const double* v, std::size_t n) noexcept {
  double m = -std::numeric_limits<double>::infinity();
  for (std::size_t i = 0; i < n; ++i) m = v[i] > m ? v[i] : m;
  if (!std::isfinite(m)) return m;
  double s = 0.0;
  for (std::size_t i = 0; i < n; ++i) s += std::exp(v[i] - m);
  return m + std::log(s);
}
[[nodiscard]] inline double logSumExp(const std::vector<double>& v) noexcept {
  return logSumExp(v.data(), v.size());
}

}  // namespace mmathen
