#pragma once

// D42 — finite discrete-time Markov chains.
//
// P is row-stochastic: P_ij = Pr(X_{t+1} = j | X_t = i).
//   stationary pi:  pi P = pi, sum pi = 1 (solved directly, not by power
//                   iteration; unique iff the chain has one closed class,
//                   otherwise DomainError).
//   irreducible:    every state reaches every state (graph search).
//   period:         gcd over edges u->v of level(u) + 1 - level(v), BFS
//                   levels from state 0 (irreducible chains).
//   entropy rate:   H = -sum_i pi_i sum_j P_ij ln P_ij  (nats per step).
//   fit:            P_ij = (n_ij + a) / (sum_k n_ik + a N) from one or more
//                   observed sequences (a = 0: maximum likelihood).
// Higher-order chains are the n-gram models of ngram.h (context = last
// n-1 symbols).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numeric>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "random.h"

namespace mmathen {

struct MarkovChain {
  std::vector<std::vector<double>> p;  // N x N, rows sum to 1
  [[nodiscard]] std::size_t states() const noexcept { return p.size(); }
};

namespace detail {
// Solve A x = y by Gaussian elimination with partial pivoting; false if
// numerically singular (pivot below `tiny`).
inline bool solveLinearSystem(std::vector<std::vector<double>> a, std::vector<double> y,
                              std::vector<double>& x, double tiny = 1e-13) {
  const std::size_t n = y.size();
  for (std::size_t c = 0; c < n; ++c) {
    std::size_t piv = c;
    for (std::size_t r = c + 1; r < n; ++r)
      if (std::fabs(a[r][c]) > std::fabs(a[piv][c])) piv = r;
    if (!(std::fabs(a[piv][c]) > tiny)) return false;
    std::swap(a[piv], a[c]);
    std::swap(y[piv], y[c]);
    for (std::size_t r = c + 1; r < n; ++r) {
      const double f = a[r][c] / a[c][c];
      if (f == 0.0) continue;
      for (std::size_t k = c; k < n; ++k) a[r][k] -= f * a[c][k];
      y[r] -= f * y[c];
    }
  }
  x.assign(n, 0.0);
  for (std::size_t c = n; c-- > 0;) {
    double s = y[c];
    for (std::size_t k = c + 1; k < n; ++k) s -= a[c][k] * x[k];
    x[c] = s / a[c][c];
  }
  return true;
}
inline bool rowStochastic(const std::vector<double>& r) {
  double s = 0.0;
  for (double v : r) {
    if (!(v >= 0.0) || !std::isfinite(v)) return false;
    s += v;
  }
  return std::fabs(s - 1.0) < 1e-9;
}
}  // namespace detail

[[nodiscard]] inline Result<MarkovChain, QuantityError> makeMarkovChain(
    std::vector<std::vector<double>> p) {
  using R = Result<MarkovChain, QuantityError>;
  if (p.empty()) return R::failure(QuantityError::InvalidArgument);
  for (const auto& row : p)
    if (row.size() != p.size() || !detail::rowStochastic(row))
      return R::failure(QuantityError::InvalidArgument);
  return R::success(MarkovChain{std::move(p)});
}

// Distribution after `steps` steps from p0.
[[nodiscard]] inline std::vector<double> distributionAfter(const MarkovChain& c,
                                                           std::vector<double> p0,
                                                           std::size_t steps) {
  const std::size_t n = c.states();
  std::vector<double> next(n);
  for (std::size_t s = 0; s < steps; ++s) {
    std::fill(next.begin(), next.end(), 0.0);
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j) next[j] += p0[i] * c.p[i][j];
    std::swap(p0, next);
  }
  return p0;
}

[[nodiscard]] inline bool isIrreducible(const MarkovChain& c) {
  const std::size_t n = c.states();
  for (std::size_t s = 0; s < n; ++s) {
    std::vector<bool> seen(n, false);
    std::vector<std::size_t> stack{s};
    seen[s] = true;
    std::size_t count = 1;
    while (!stack.empty()) {
      const std::size_t u = stack.back();
      stack.pop_back();
      for (std::size_t v = 0; v < n; ++v)
        if (c.p[u][v] > 0.0 && !seen[v]) seen[v] = true, ++count, stack.push_back(v);
    }
    if (count != n) return false;
  }
  return true;
}

// Period of an irreducible chain (1 = aperiodic). DomainError otherwise.
[[nodiscard]] inline Result<std::size_t, QuantityError> chainPeriod(const MarkovChain& c) {
  using R = Result<std::size_t, QuantityError>;
  if (!isIrreducible(c)) return R::failure(QuantityError::DomainError);
  const std::size_t n = c.states();
  std::vector<long long> level(n, -1);
  std::vector<std::size_t> queue{0};
  level[0] = 0;
  for (std::size_t q = 0; q < queue.size(); ++q)
    for (std::size_t v = 0; v < n; ++v)
      if (c.p[queue[q]][v] > 0.0 && level[v] < 0) level[v] = level[queue[q]] + 1, queue.push_back(v);
  long long g = 0;
  for (std::size_t u = 0; u < n; ++u)
    for (std::size_t v = 0; v < n; ++v)
      if (c.p[u][v] > 0.0) g = std::gcd(g, std::llabs(level[u] + 1 - level[v]));
  return R::success(static_cast<std::size_t>(g));
}

// Unique stationary distribution; DomainError if it is not unique.
[[nodiscard]] inline Result<std::vector<double>, QuantityError> stationaryDistribution(
    const MarkovChain& c) {
  using R = Result<std::vector<double>, QuantityError>;
  const std::size_t n = c.states();
  // (P^T - I) pi = 0 with the last equation replaced by sum(pi) = 1.
  std::vector<std::vector<double>> a(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) a[i][j] = c.p[j][i] - (i == j ? 1.0 : 0.0);
  a[n - 1].assign(n, 1.0);
  std::vector<double> y(n, 0.0), pi;
  y[n - 1] = 1.0;
  if (!detail::solveLinearSystem(a, y, pi)) return R::failure(QuantityError::DomainError);
  for (auto& v : pi) {
    if (v < 0.0 && v > -1e-12) v = 0.0;
    if (!(v >= 0.0)) return R::failure(QuantityError::DomainError);
  }
  return R::success(std::move(pi));
}

// Entropy rate in nats per step, given the stationary distribution.
[[nodiscard]] inline double entropyRate(const MarkovChain& c, const std::vector<double>& pi) {
  double h = 0.0;
  for (std::size_t i = 0; i < c.states(); ++i)
    for (double pij : c.p[i])
      if (pij > 0.0) h -= pi[i] * pij * std::log(pij);
  return h;
}

[[nodiscard]] inline std::vector<std::size_t> sampleChain(const MarkovChain& c, std::size_t start,
                                                          std::size_t length, SplitMix64& rng) {
  std::vector<std::size_t> s;
  if (length == 0 || start >= c.states()) return s;
  s.push_back(start);
  while (s.size() < length) s.push_back(rng.categorical(c.p[s.back()]));
  return s;
}

// Fit from sequences over {0..n-1}; rows never left keep a uniform row.
[[nodiscard]] inline Result<MarkovChain, QuantityError> fitMarkovChain(
    const std::vector<std::vector<std::size_t>>& sequences, std::size_t n,
    double pseudoCount = 0.0) {
  using R = Result<MarkovChain, QuantityError>;
  if (n == 0 || !(pseudoCount >= 0.0) || !std::isfinite(pseudoCount))
    return R::failure(QuantityError::InvalidArgument);
  std::vector<std::vector<double>> cnt(n, std::vector<double>(n, pseudoCount));
  for (const auto& s : sequences)
    for (std::size_t t = 0; t < s.size(); ++t) {
      if (s[t] >= n) return R::failure(QuantityError::InvalidArgument);
      if (t > 0) cnt[s[t - 1]][s[t]] += 1.0;
    }
  for (auto& row : cnt) {
    double tot = 0.0;
    for (double v : row) tot += v;
    if (!std::isfinite(tot)) return R::failure(QuantityError::RangeError);
    for (auto& v : row) v = tot > 0.0 ? v / tot : 1.0 / static_cast<double>(n);
  }
  return R::success(MarkovChain{std::move(cnt)});
}

}  // namespace mmathen
