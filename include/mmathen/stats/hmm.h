#pragma once

// D41 — hidden Markov models: forward, backward, posteriors, Viterbi,
// Baum-Welch (discrete and GMM emissions), sampling.
//
// Everything runs in the log domain (Rabiner 1989 notation, with log-sum-
// exp in place of scaling), so sequences of any length neither underflow
// nor need rescaling, and forbidden transitions (probability 0 = -inf) are
// exact: no decoded path and no re-estimated model ever uses one.
// Emissions are passed as a T x N matrix of log-likelihoods log b_j(o_t),
// so the same decoder serves discrete symbols, GMMs (the classical GMM-HMM
// of speech recognition), template scores, or pYIN-style candidates.
//
//   forward   log a_t(j) = logsumexp_i(log a_{t-1}(i) + log A_ij) + log b_j(o_t)
//   backward  log B_t(i) = logsumexp_j(log A_ij + log b_j(o_{t+1}) + log B_{t+1}(j))
//   posterior g_t(j) = exp(log a_t(j) + log B_t(j) - log P(O))
//   Viterbi   d_t(j) = max_i(d_{t-1}(i) + log A_ij) + log b_j(o_t), with
//             back-pointers; ties go to the lowest state index.
// Baum-Welch (Baum et al. 1970) re-estimates pi, A and the emissions from
// the posteriors and xi_t(i,j); each iteration never decreases the total
// log-likelihood (EM), which the fits report per iteration.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "gmm.h"
#include "random.h"

namespace mmathen {

using LogMatrix = std::vector<std::vector<double>>;

struct Hmm {
  std::vector<double> logInitial;  // N
  LogMatrix logTransition;         // N x N, row i = from state i
  [[nodiscard]] std::size_t states() const noexcept { return logInitial.size(); }
};

namespace detail {
inline double safeLog(double p) {
  return p > 0.0 ? std::log(p) : -std::numeric_limits<double>::infinity();
}
inline bool isStochastic(const std::vector<double>& p) {
  double s = 0.0;
  for (double v : p) {
    if (!(v >= 0.0) || !std::isfinite(v)) return false;
    s += v;
  }
  return std::fabs(s - 1.0) < 1e-9;
}
inline bool isLogStochastic(const std::vector<double>& row) {
  double sum = 0.0;
  for (double v : row) {
    if (std::isnan(v) || v == std::numeric_limits<double>::infinity()) return false;
    sum += std::exp(v);  // -infinity is a valid zero probability.
  }
  return std::fabs(sum - 1.0) < 1e-9;
}
inline bool isValidHmm(const Hmm& h) {
  const std::size_t n = h.states();
  if (n == 0 || h.logTransition.size() != n || !isLogStochastic(h.logInitial)) return false;
  for (const auto& row : h.logTransition)
    if (row.size() != n || !isLogStochastic(row)) return false;
  return true;
}
inline bool emissionsFit(const Hmm& h, const LogMatrix& logB) {
  if (!isValidHmm(h) || logB.empty()) return false;
  for (const auto& row : logB) {
    if (row.size() != h.states()) return false;
    for (double v : row)
      if (std::isnan(v) || v == std::numeric_limits<double>::infinity()) return false;
  }
  return true;
}
}  // namespace detail

// From probabilities (each row and the initial vector must sum to 1).
[[nodiscard]] inline Result<Hmm, QuantityError> makeHmm(
    const std::vector<double>& initial, const std::vector<std::vector<double>>& transition) {
  using R = Result<Hmm, QuantityError>;
  const std::size_t n = initial.size();
  if (n == 0 || transition.size() != n || !detail::isStochastic(initial))
    return R::failure(QuantityError::InvalidArgument);
  Hmm h;
  for (double p : initial) h.logInitial.push_back(detail::safeLog(p));
  for (const auto& row : transition) {
    if (row.size() != n || !detail::isStochastic(row)) return R::failure(QuantityError::InvalidArgument);
    std::vector<double> lr;
    for (double p : row) lr.push_back(detail::safeLog(p));
    h.logTransition.push_back(std::move(lr));
  }
  return R::success(std::move(h));
}

struct ForwardBackward {
  LogMatrix logAlpha, logBeta;  // T x N
  LogMatrix posteriors;         // T x N, rows sum to 1
  double logLikelihood = 0.0;
};

// Forward pass only: log P(O | model). DomainError if P(O) = 0.
[[nodiscard]] inline Result<double, QuantityError> forwardLogLikelihood(const Hmm& h,
                                                                        const LogMatrix& logB) {
  using R = Result<double, QuantityError>;
  if (!detail::emissionsFit(h, logB)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t n = h.states();
  std::vector<double> a(n), next(n), tmp(n);
  for (std::size_t j = 0; j < n; ++j) a[j] = h.logInitial[j] + logB[0][j];
  for (std::size_t t = 1; t < logB.size(); ++t) {
    for (std::size_t j = 0; j < n; ++j) {
      for (std::size_t i = 0; i < n; ++i) tmp[i] = a[i] + h.logTransition[i][j];
      next[j] = logSumExp(tmp) + logB[t][j];
    }
    std::swap(a, next);
  }
  const double l = logSumExp(a);
  if (!std::isfinite(l)) return R::failure(QuantityError::DomainError);
  return R::success(l);
}

[[nodiscard]] inline Result<ForwardBackward, QuantityError> forwardBackward(const Hmm& h,
                                                                           const LogMatrix& logB) {
  using R = Result<ForwardBackward, QuantityError>;
  if (!detail::emissionsFit(h, logB)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t n = h.states(), tt = logB.size();
  const double ninf = -std::numeric_limits<double>::infinity();
  ForwardBackward fb;
  fb.logAlpha.assign(tt, std::vector<double>(n, ninf));
  fb.logBeta.assign(tt, std::vector<double>(n, 0.0));
  std::vector<double> tmp(n);
  for (std::size_t j = 0; j < n; ++j) fb.logAlpha[0][j] = h.logInitial[j] + logB[0][j];
  for (std::size_t t = 1; t < tt; ++t)
    for (std::size_t j = 0; j < n; ++j) {
      for (std::size_t i = 0; i < n; ++i) tmp[i] = fb.logAlpha[t - 1][i] + h.logTransition[i][j];
      fb.logAlpha[t][j] = logSumExp(tmp) + logB[t][j];
    }
  for (std::size_t t = tt - 1; t-- > 0;)
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j)
        tmp[j] = h.logTransition[i][j] + logB[t + 1][j] + fb.logBeta[t + 1][j];
      fb.logBeta[t][i] = logSumExp(tmp);
    }
  fb.logLikelihood = logSumExp(fb.logAlpha[tt - 1]);
  if (!std::isfinite(fb.logLikelihood)) return R::failure(QuantityError::DomainError);
  fb.posteriors.assign(tt, std::vector<double>(n, 0.0));
  for (std::size_t t = 0; t < tt; ++t)
    for (std::size_t j = 0; j < n; ++j)
      fb.posteriors[t][j] = std::exp(fb.logAlpha[t][j] + fb.logBeta[t][j] - fb.logLikelihood);
  return R::success(std::move(fb));
}

struct ViterbiPath {
  std::vector<std::size_t> states;
  double logProbability = 0.0;  // log P(O, best path)
};

[[nodiscard]] inline Result<ViterbiPath, QuantityError> viterbi(const Hmm& h, const LogMatrix& logB) {
  using R = Result<ViterbiPath, QuantityError>;
  if (!detail::emissionsFit(h, logB)) return R::failure(QuantityError::InvalidArgument);
  const std::size_t n = h.states(), tt = logB.size();
  const double ninf = -std::numeric_limits<double>::infinity();
  std::vector<double> d(n), nd(n);
  std::vector<std::vector<std::size_t>> back(tt, std::vector<std::size_t>(n, 0));
  for (std::size_t j = 0; j < n; ++j) d[j] = h.logInitial[j] + logB[0][j];
  for (std::size_t t = 1; t < tt; ++t) {
    for (std::size_t j = 0; j < n; ++j) {
      double best = ninf;
      std::size_t arg = 0;
      for (std::size_t i = 0; i < n; ++i) {
        const double v = d[i] + h.logTransition[i][j];
        if (v > best) best = v, arg = i;
      }
      nd[j] = best + logB[t][j];
      back[t][j] = arg;
    }
    std::swap(d, nd);
  }
  ViterbiPath p;
  std::size_t s = 0;
  p.logProbability = ninf;
  for (std::size_t j = 0; j < n; ++j)
    if (d[j] > p.logProbability) p.logProbability = d[j], s = j;
  if (!std::isfinite(p.logProbability)) return R::failure(QuantityError::DomainError);
  p.states.assign(tt, 0);
  p.states[tt - 1] = s;
  for (std::size_t t = tt - 1; t > 0; --t) p.states[t - 1] = back[t][p.states[t]];
  return R::success(std::move(p));
}

// Log joint probability of a given state path (for checks and scoring).
[[nodiscard]] inline double pathLogProbability(const Hmm& h, const LogMatrix& logB,
                                               const std::vector<std::size_t>& path) {
  double s = h.logInitial[path[0]] + logB[0][path[0]];
  for (std::size_t t = 1; t < path.size(); ++t)
    s += h.logTransition[path[t - 1]][path[t]] + logB[t][path[t]];
  return s;
}

// ---- Discrete emissions -------------------------------------------------

struct DiscreteHmm {
  Hmm hmm;
  LogMatrix logEmission;  // N x M
  [[nodiscard]] LogMatrix emissionsFor(const std::vector<std::size_t>& obs) const {
    LogMatrix b(obs.size(), std::vector<double>(hmm.states()));
    for (std::size_t t = 0; t < obs.size(); ++t)
      for (std::size_t j = 0; j < hmm.states(); ++j)
        b[t][j] = obs[t] < logEmission[j].size() ? logEmission[j][obs[t]]
                                                 : -std::numeric_limits<double>::infinity();
    return b;
  }
};

[[nodiscard]] inline Result<DiscreteHmm, QuantityError> makeDiscreteHmm(
    const std::vector<double>& initial, const std::vector<std::vector<double>>& transition,
    const std::vector<std::vector<double>>& emission) {
  using R = Result<DiscreteHmm, QuantityError>;
  auto h = makeHmm(initial, transition);
  if (!h.ok()) return R::failure(h.error());
  if (emission.size() != initial.size() || emission[0].empty()) return R::failure(QuantityError::InvalidArgument);
  DiscreteHmm d{std::move(h).value(), {}};
  for (const auto& row : emission) {
    if (row.size() != emission[0].size() || !detail::isStochastic(row))
      return R::failure(QuantityError::InvalidArgument);
    std::vector<double> lr;
    for (double p : row) lr.push_back(detail::safeLog(p));
    d.logEmission.push_back(std::move(lr));
  }
  return R::success(std::move(d));
}

struct HmmSample {
  std::vector<std::size_t> states, symbols;
};
[[nodiscard]] inline HmmSample sampleDiscreteHmm(const DiscreteHmm& m, std::size_t length,
                                                 SplitMix64& rng) {
  HmmSample s;
  auto probs = [](const std::vector<double>& lp) {
    std::vector<double> p;
    for (double v : lp) p.push_back(std::exp(v));
    return p;
  };
  std::size_t st = rng.categorical(probs(m.hmm.logInitial));
  for (std::size_t t = 0; t < length; ++t) {
    if (t > 0) st = rng.categorical(probs(m.hmm.logTransition[st]));
    s.states.push_back(st);
    s.symbols.push_back(rng.categorical(probs(m.logEmission[st])));
  }
  return s;
}

struct BaumWelchOptions {
  std::size_t maxIterations = 200;
  double tolerance = 1e-9;  // stop when the relative log-lik gain < tol
  double pseudoCount = 0.0; // added to every emission count (0: pure ML)
};

template <typename Model>
struct BaumWelchFit {
  Model model;
  std::vector<double> logLikelihood;  // total over sequences, per iteration (before its update)
  std::size_t iterations = 0;
  bool converged = false;
};

namespace detail {
// Accumulates the pi / A statistics of one sequence into (pi, a, from).
inline void accumulateTransitions(const Hmm& h, const LogMatrix& logB, const ForwardBackward& fb,
                                  std::vector<double>& pi, LogMatrix& a, std::vector<double>& from) {
  const std::size_t n = h.states(), tt = logB.size();
  for (std::size_t j = 0; j < n; ++j) pi[j] += fb.posteriors[0][j];
  for (std::size_t t = 0; t + 1 < tt; ++t)
    for (std::size_t i = 0; i < n; ++i) {
      from[i] += fb.posteriors[t][i];
      if (!std::isfinite(fb.logAlpha[t][i])) continue;
      for (std::size_t j = 0; j < n; ++j) {
        const double lx = fb.logAlpha[t][i] + h.logTransition[i][j] + logB[t + 1][j] +
                          fb.logBeta[t + 1][j] - fb.logLikelihood;
        if (std::isfinite(lx)) a[i][j] += std::exp(lx);
      }
    }
}
inline void updateTransitions(Hmm& h, const std::vector<double>& pi, const LogMatrix& a,
                              const std::vector<double>& from, double sequences) {
  const std::size_t n = h.states();
  for (std::size_t j = 0; j < n; ++j) h.logInitial[j] = safeLog(pi[j] / sequences);
  for (std::size_t i = 0; i < n; ++i) {
    if (!(from[i] > 0.0)) continue;  // never visited: keep its row
    double rs = 0.0;
    for (std::size_t j = 0; j < n; ++j) rs += a[i][j];
    for (std::size_t j = 0; j < n; ++j) h.logTransition[i][j] = safeLog(a[i][j] / rs);
  }
}
}  // namespace detail

[[nodiscard]] inline Result<BaumWelchFit<DiscreteHmm>, QuantityError> baumWelchDiscrete(
    DiscreteHmm model, const std::vector<std::vector<std::size_t>>& sequences,
    const BaumWelchOptions& o = {}) {
  using R = Result<BaumWelchFit<DiscreteHmm>, QuantityError>;
  const std::size_t n = model.hmm.states();
  if (sequences.empty() || !detail::isValidHmm(model.hmm) || model.logEmission.size() != n ||
      !(o.pseudoCount >= 0.0) || !std::isfinite(o.pseudoCount) ||
      !(o.tolerance >= 0.0) || !std::isfinite(o.tolerance))
    return R::failure(QuantityError::InvalidArgument);
  const std::size_t m = model.logEmission[0].size();
  if (m == 0) return R::failure(QuantityError::InvalidArgument);
  for (const auto& row : model.logEmission)
    if (row.size() != m || !detail::isLogStochastic(row))
      return R::failure(QuantityError::InvalidArgument);
  for (const auto& s : sequences) {
    if (s.empty()) return R::failure(QuantityError::InvalidArgument);
    for (auto v : s)
      if (v >= m) return R::failure(QuantityError::InvalidArgument);
  }
  BaumWelchFit<DiscreteHmm> fit;
  double prev = -std::numeric_limits<double>::infinity();
  for (std::size_t it = 0; it < o.maxIterations; ++it) {
    std::vector<double> pi(n, 0.0), from(n, 0.0);
    LogMatrix a(n, std::vector<double>(n, 0.0)), e(n, std::vector<double>(m, o.pseudoCount));
    double total = 0.0;
    for (const auto& s : sequences) {
      const auto b = model.emissionsFor(s);
      auto fbr = forwardBackward(model.hmm, b);
      if (!fbr.ok()) return R::failure(fbr.error());
      const auto& fb = fbr.value();
      total += fb.logLikelihood;
      detail::accumulateTransitions(model.hmm, b, fb, pi, a, from);
      for (std::size_t t = 0; t < s.size(); ++t)
        for (std::size_t j = 0; j < n; ++j) e[j][s[t]] += fb.posteriors[t][j];
    }
    fit.logLikelihood.push_back(total);
    fit.iterations = it + 1;
    if (it > 0 && total - prev < o.tolerance * std::fabs(total)) {
      fit.converged = true;
      break;
    }
    prev = total;
    detail::updateTransitions(model.hmm, pi, a, from, static_cast<double>(sequences.size()));
    for (std::size_t j = 0; j < n; ++j) {
      double rs = 0.0;
      for (double v : e[j]) rs += v;
      if (!(rs > 0.0)) continue;
      for (std::size_t k = 0; k < m; ++k) model.logEmission[j][k] = detail::safeLog(e[j][k] / rs);
    }
  }
  fit.model = std::move(model);
  return R::success(std::move(fit));
}

// ---- GMM emissions (the classical GMM-HMM) --------------------------------

struct GmmHmm {
  Hmm hmm;
  std::vector<Gmm> emission;  // one GMM per state
  [[nodiscard]] LogMatrix emissionsFor(const std::vector<std::vector<double>>& x) const {
    LogMatrix b(x.size(), std::vector<double>(hmm.states()));
    for (std::size_t t = 0; t < x.size(); ++t)
      for (std::size_t j = 0; j < hmm.states(); ++j) b[t][j] = emission[j].logLikelihood(x[t]);
    return b;
  }
};

struct GmmHmmOptions : BaumWelchOptions {
  double varianceFloor = 1e-4;  // absolute
};

// Baum-Welch for GMM-HMMs: state posteriors times within-state component
// responsibilities give each component's occupancy (Juang 1985).
[[nodiscard]] inline Result<BaumWelchFit<GmmHmm>, QuantityError> baumWelchGmm(
    GmmHmm model, const std::vector<std::vector<std::vector<double>>>& sequences,
    const GmmHmmOptions& o = {}) {
  using R = Result<BaumWelchFit<GmmHmm>, QuantityError>;
  const std::size_t n = model.hmm.states();
  if (sequences.empty() || !detail::isValidHmm(model.hmm) || model.emission.size() != n ||
      !(o.varianceFloor > 0.0) || !std::isfinite(o.varianceFloor) ||
      !(o.tolerance >= 0.0) || !std::isfinite(o.tolerance))
    return R::failure(QuantityError::InvalidArgument);
  for (const auto& g : model.emission)
    if (!isValidGmm(g) || g.dimension() != model.emission[0].dimension())
      return R::failure(QuantityError::InvalidArgument);
  const std::size_t d = model.emission[0].dimension();
  for (const auto& s : sequences) {
    if (s.empty()) return R::failure(QuantityError::InvalidArgument);
    for (const auto& x : s) {
      if (x.size() != d) return R::failure(QuantityError::InvalidArgument);
      for (double v : x)
        if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
    }
  }
  BaumWelchFit<GmmHmm> fit;
  double prev = -std::numeric_limits<double>::infinity();
  for (std::size_t it = 0; it < o.maxIterations; ++it) {
    std::vector<double> pi(n, 0.0), from(n, 0.0);
    LogMatrix a(n, std::vector<double>(n, 0.0));
    // Per state, per component: occupancy, first and second moments about 0
    // are cancellation-prone, so collect (occupancy, sum x) first and the
    // squared deviations in a second pass over the stored posteriors.
    std::vector<std::vector<double>> occ(n);
    std::vector<std::vector<std::vector<double>>> sx(n), sxx(n);
    for (std::size_t j = 0; j < n; ++j) {
      const std::size_t k = model.emission[j].components();
      occ[j].assign(k, 0.0);
      sx[j].assign(k, std::vector<double>(d, 0.0));
      sxx[j].assign(k, std::vector<double>(d, 0.0));
    }
    double total = 0.0;
    std::vector<LogMatrix> posts;
    for (const auto& s : sequences) {
      const auto b = model.emissionsFor(s);
      auto fbr = forwardBackward(model.hmm, b);
      if (!fbr.ok()) return R::failure(fbr.error());
      auto fb = std::move(fbr).value();
      total += fb.logLikelihood;
      detail::accumulateTransitions(model.hmm, b, fb, pi, a, from);
      for (std::size_t t = 0; t < s.size(); ++t)
        for (std::size_t j = 0; j < n; ++j) {
          const double g = fb.posteriors[t][j];
          if (!(g > 0.0)) continue;
          const auto r = model.emission[j].posteriors(s[t]);
          for (std::size_t c = 0; c < r.size(); ++c) {
            const double w = g * r[c];
            occ[j][c] += w;
            for (std::size_t q = 0; q < d; ++q) sx[j][c][q] += w * s[t][q];
          }
        }
      posts.push_back(std::move(fb.posteriors));
    }
    fit.logLikelihood.push_back(total);
    fit.iterations = it + 1;
    if (it > 0 && total - prev < o.tolerance * std::fabs(total)) {
      fit.converged = true;
      break;
    }
    prev = total;
    std::vector<std::vector<std::vector<double>>> mu(n);
    for (std::size_t j = 0; j < n; ++j) {
      mu[j] = model.emission[j].means;
      for (std::size_t c = 0; c < occ[j].size(); ++c)
        if (occ[j][c] > 0.0)
          for (std::size_t q = 0; q < d; ++q) mu[j][c][q] = sx[j][c][q] / occ[j][c];
    }
    for (std::size_t si = 0; si < sequences.size(); ++si)
      for (std::size_t t = 0; t < sequences[si].size(); ++t)
        for (std::size_t j = 0; j < n; ++j) {
          const double g = posts[si][t][j];
          if (!(g > 0.0)) continue;
          const auto r = model.emission[j].posteriors(sequences[si][t]);
          for (std::size_t c = 0; c < r.size(); ++c)
            for (std::size_t q = 0; q < d; ++q) {
              const double e = sequences[si][t][q] - mu[j][c][q];
              sxx[j][c][q] += g * r[c] * e * e;
            }
        }
    detail::updateTransitions(model.hmm, pi, a, from, static_cast<double>(sequences.size()));
    for (std::size_t j = 0; j < n; ++j) {
      double os = 0.0;
      for (double v : occ[j]) os += v;
      if (!(os > 0.0)) continue;
      Gmm& g = model.emission[j];
      for (std::size_t c = 0; c < occ[j].size(); ++c) {
        g.weights[c] = std::max(occ[j][c] / os, 1e-12);
        if (!(occ[j][c] > 0.0)) continue;
        g.means[c] = mu[j][c];
        for (std::size_t q = 0; q < d; ++q)
          g.variances[c][q] = std::max(sxx[j][c][q] / occ[j][c], o.varianceFloor);
      }
      double ws = 0.0;
      for (double w : g.weights) ws += w;
      for (auto& w : g.weights) w /= ws;
    }
  }
  fit.model = std::move(model);
  return R::success(std::move(fit));
}

}  // namespace mmathen
