#pragma once

// D40 — diagonal-covariance Gaussian mixture models, trained by EM.
//
// Density: p(x) = sum_k w_k N(x; mu_k, diag(var_k)), evaluated in the log
// domain with log-sum-exp, so far outliers give a finite log-likelihood
// instead of log(0).
// Training (Dempster, Laird & Rubin 1977; Bishop, PRML sec. 9.2):
//   init  k-means++ seeding (Arthur & Vassilvitskii 2007) with a fixed
//         seed, then Lloyd iterations; weights/variances from the clusters.
//   E     r_nk = w_k N(x_n; k) / sum_j w_j N(x_n; j)
//   M     N_k = sum_n r_nk, w_k = N_k / N, mu_k = sum_n r_nk x_n / N_k,
//         var_k = sum_n r_nk (x_n - mu_k)^2 / N_k, floored.
// Exact EM never decreases the log-likelihood; the variance floor can (it
// is a constraint EM doesn't know about), so the fit reports every
// iteration's log-likelihood and whether the floor was ever applied.
// Deterministic: same data and seed give the same model on every compiler
// (see random.h).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "random.h"

namespace mmathen {

struct Gmm {
  std::vector<double> weights;              // K, sum 1
  std::vector<std::vector<double>> means;   // K x D
  std::vector<std::vector<double>> variances;  // K x D, > 0

  [[nodiscard]] std::size_t components() const noexcept { return weights.size(); }
  [[nodiscard]] std::size_t dimension() const noexcept {
    return means.empty() ? 0 : means[0].size();
  }
  // log(w_k) + log N(x; mu_k, var_k) for each component.
  [[nodiscard]] std::vector<double> componentLogJoint(const double* x) const {
    const std::size_t d = dimension();
    std::vector<double> out(components());
    const double log2pi = std::log(2.0 * std::numbers::pi);
    for (std::size_t k = 0; k < components(); ++k) {
      double s = std::log(weights[k]) - 0.5 * static_cast<double>(d) * log2pi;
      for (std::size_t j = 0; j < d; ++j) {
        const double v = variances[k][j], e = x[j] - means[k][j];
        s -= 0.5 * (std::log(v) + e * e / v);
      }
      out[k] = s;
    }
    return out;
  }
  [[nodiscard]] double logLikelihood(const double* x) const {
    return logSumExp(componentLogJoint(x));
  }
  [[nodiscard]] double logLikelihood(const std::vector<double>& x) const {
    return logLikelihood(x.data());
  }
  // Posterior responsibilities (sum to 1).
  [[nodiscard]] std::vector<double> posteriors(const std::vector<double>& x) const {
    auto l = componentLogJoint(x.data());
    const double z = logSumExp(l);
    for (auto& v : l) v = std::exp(v - z);
    return l;
  }
  // Draw one sample.
  [[nodiscard]] std::vector<double> sample(SplitMix64& rng) const {
    const std::size_t k = rng.categorical(weights);
    std::vector<double> x(dimension());
    for (std::size_t j = 0; j < x.size(); ++j)
      x[j] = means[k][j] + std::sqrt(variances[k][j]) * rng.normal();
    return x;
  }
  // Free parameters: (K-1) weights + 2 K D.
  [[nodiscard]] std::size_t parameterCount() const noexcept {
    return components() - 1 + 2 * components() * dimension();
  }
};

// Validates shapes, weights (positive, sum 1 within 1e-9) and variances.
[[nodiscard]] inline bool isValidGmm(const Gmm& g) {
  const std::size_t k = g.components(), d = g.dimension();
  if (k == 0 || d == 0 || g.means.size() != k || g.variances.size() != k) return false;
  double s = 0.0;
  for (std::size_t i = 0; i < k; ++i) {
    if (!(g.weights[i] > 0.0) || g.means[i].size() != d || g.variances[i].size() != d) return false;
    for (std::size_t j = 0; j < d; ++j)
      if (!std::isfinite(g.means[i][j]) || !(g.variances[i][j] > 0.0) ||
          !std::isfinite(g.variances[i][j]))
        return false;
    s += g.weights[i];
  }
  return std::fabs(s - 1.0) < 1e-9;
}

struct GmmTrainOptions {
  std::size_t components = 2;
  std::size_t maxIterations = 300;
  double tolerance = 1e-9;       // stop when the mean log-lik gain < tol
  double varianceFloor = 1e-6;   // relative to each dimension's data variance
  std::size_t kmeansIterations = 10;
  std::uint64_t seed = 1;
};

struct GmmFit {
  Gmm model;
  std::vector<double> meanLogLikelihood;  // per EM iteration (after each M step)
  std::size_t iterations = 0;
  bool converged = false;
  bool floorApplied = false;
};

namespace detail {
inline double sqDist(const std::vector<double>& a, const std::vector<double>& b) {
  double s = 0.0;
  for (std::size_t j = 0; j < a.size(); ++j) s += (a[j] - b[j]) * (a[j] - b[j]);
  return s;
}
inline double meanLogLik(const Gmm& g, const std::vector<std::vector<double>>& data) {
  double s = 0.0;
  for (const auto& x : data) s += g.logLikelihood(x);
  return s / static_cast<double>(data.size());
}
}  // namespace detail

// One EM step from `g` (E then M); returns the updated model.
// `floorApplied` is set when a variance hit the floor.
[[nodiscard]] inline Gmm gmmEmStep(const Gmm& g, const std::vector<std::vector<double>>& data,
                                   const std::vector<double>& floorVar, bool& floorApplied) {
  const std::size_t k = g.components(), d = g.dimension(), n = data.size();
  std::vector<double> nk(k, 0.0);
  std::vector<std::vector<double>> sx(k, std::vector<double>(d, 0.0));
  std::vector<std::vector<double>> sxx(k, std::vector<double>(d, 0.0));
  std::vector<std::vector<double>> resp(n);
  for (std::size_t i = 0; i < n; ++i) {
    resp[i] = g.posteriors(data[i]);
    for (std::size_t c = 0; c < k; ++c) {
      nk[c] += resp[i][c];
      for (std::size_t j = 0; j < d; ++j) sx[c][j] += resp[i][c] * data[i][j];
    }
  }
  Gmm out = g;
  for (std::size_t c = 0; c < k; ++c) {
    if (!(nk[c] > 0.0)) continue;  // an empty component keeps its parameters
    for (std::size_t j = 0; j < d; ++j) out.means[c][j] = sx[c][j] / nk[c];
  }
  // Two-pass variance (about the new mean): no cancellation.
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t c = 0; c < k; ++c)
      for (std::size_t j = 0; j < d; ++j) {
        const double e = data[i][j] - out.means[c][j];
        sxx[c][j] += resp[i][c] * e * e;
      }
  double wsum = 0.0;
  for (std::size_t c = 0; c < k; ++c) {
    out.weights[c] = std::max(nk[c], std::numeric_limits<double>::min()) / static_cast<double>(n);
    wsum += out.weights[c];
    if (!(nk[c] > 0.0)) continue;
    for (std::size_t j = 0; j < d; ++j) {
      double v = sxx[c][j] / nk[c];
      if (v < floorVar[j]) {
        v = floorVar[j];
        floorApplied = true;
      }
      out.variances[c][j] = v;
    }
  }
  for (auto& w : out.weights) w /= wsum;
  return out;
}

// Fit K components to `data` (N x D, N >= K).
[[nodiscard]] inline Result<GmmFit, QuantityError> trainGmm(
    const std::vector<std::vector<double>>& data, const GmmTrainOptions& o = {}) {
  using R = Result<GmmFit, QuantityError>;
  const std::size_t n = data.size(), k = o.components;
  if (k < 1 || n < k || data[0].empty() || !(o.varianceFloor > 0.0) || !(o.tolerance >= 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const std::size_t d = data[0].size();
  for (const auto& x : data) {
    if (x.size() != d) return R::failure(QuantityError::InvalidArgument);
    for (double v : x)
      if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  }
  // Global statistics (for the floor, and for degenerate clusters).
  std::vector<double> gm(d, 0.0), gv(d, 0.0);
  for (const auto& x : data)
    for (std::size_t j = 0; j < d; ++j) gm[j] += x[j];
  for (auto& v : gm) v /= static_cast<double>(n);
  for (const auto& x : data)
    for (std::size_t j = 0; j < d; ++j) gv[j] += (x[j] - gm[j]) * (x[j] - gm[j]);
  std::vector<double> floorVar(d);
  for (std::size_t j = 0; j < d; ++j) {
    gv[j] /= static_cast<double>(n);
    floorVar[j] = std::max(o.varianceFloor * gv[j], 1e-300);
    if (!(gv[j] > 0.0)) gv[j] = floorVar[j];
  }
  // k-means++ seeding.
  SplitMix64 rng{o.seed};
  std::vector<std::vector<double>> centres;
  centres.push_back(data[static_cast<std::size_t>(rng.uniform() * static_cast<double>(n)) % n]);
  std::vector<double> dist(n, std::numeric_limits<double>::infinity());
  while (centres.size() < k) {
    for (std::size_t i = 0; i < n; ++i) dist[i] = std::min(dist[i], detail::sqDist(data[i], centres.back()));
    centres.push_back(data[rng.categorical(dist)]);
  }
  // Lloyd iterations.
  std::vector<std::size_t> label(n, 0);
  for (std::size_t it = 0; it <= o.kmeansIterations; ++it) {
    for (std::size_t i = 0; i < n; ++i) {
      double best = std::numeric_limits<double>::infinity();
      for (std::size_t c = 0; c < k; ++c) {
        const double dd = detail::sqDist(data[i], centres[c]);
        if (dd < best) best = dd, label[i] = c;
      }
    }
    if (it == o.kmeansIterations) break;
    std::vector<std::vector<double>> sum(k, std::vector<double>(d, 0.0));
    std::vector<std::size_t> cnt(k, 0);
    for (std::size_t i = 0; i < n; ++i) {
      ++cnt[label[i]];
      for (std::size_t j = 0; j < d; ++j) sum[label[i]][j] += data[i][j];
    }
    for (std::size_t c = 0; c < k; ++c)
      if (cnt[c] > 0)
        for (std::size_t j = 0; j < d; ++j) centres[c][j] = sum[c][j] / static_cast<double>(cnt[c]);
  }
  GmmFit fit;
  Gmm& g = fit.model;
  g.weights.assign(k, 0.0);
  g.means = centres;
  g.variances.assign(k, std::vector<double>(d, 0.0));
  std::vector<std::size_t> cnt(k, 0);
  for (std::size_t i = 0; i < n; ++i) {
    ++cnt[label[i]];
    for (std::size_t j = 0; j < d; ++j) {
      const double e = data[i][j] - centres[label[i]][j];
      g.variances[label[i]][j] += e * e;
    }
  }
  for (std::size_t c = 0; c < k; ++c) {
    g.weights[c] = std::max<double>(static_cast<double>(cnt[c]), 0.5);
    for (std::size_t j = 0; j < d; ++j)
      g.variances[c][j] = cnt[c] > 1 ? std::max(g.variances[c][j] / static_cast<double>(cnt[c]), floorVar[j])
                                     : gv[j];
  }
  double ws = 0.0;
  for (double w : g.weights) ws += w;
  for (auto& w : g.weights) w /= ws;
  // EM.
  double prev = detail::meanLogLik(g, data);
  for (std::size_t it = 0; it < o.maxIterations; ++it) {
    g = gmmEmStep(g, data, floorVar, fit.floorApplied);
    const double cur = detail::meanLogLik(g, data);
    fit.meanLogLikelihood.push_back(cur);
    fit.iterations = it + 1;
    if (cur - prev < o.tolerance * std::max(1.0, std::fabs(cur))) {
      fit.converged = true;
      break;
    }
    prev = cur;
  }
  return R::success(std::move(fit));
}

// Bayesian information criterion: -2 ln L + p ln N (lower is better).
[[nodiscard]] inline double gmmBic(const Gmm& g, const std::vector<std::vector<double>>& data) {
  return -2.0 * detail::meanLogLik(g, data) * static_cast<double>(data.size()) +
         static_cast<double>(g.parameterCount()) * std::log(static_cast<double>(data.size()));
}

}  // namespace mmathen
