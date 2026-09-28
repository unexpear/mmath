// D40 GMM and D41 HMM (Catch2).
//
// GMM: one component is the closed-form maximum-likelihood fit; a two-
// component 2-D mixture is recovered within 4-5 standard errors from
// 20,000 samples; EM's log-likelihood never decreases; BIC picks the true
// K; posteriors sum to 1; far outliers stay finite; fits are reproducible.
// HMM: the Wikipedia "Healthy/Fever" example decodes to H H F with
// probability 0.01512 (the published value); on random models, forward,
// posteriors and Viterbi equal brute-force enumeration over all 3^7 paths;
// 200,000-step sequences don't underflow; forbidden transitions are never
// used; Baum-Welch never decreases the likelihood and recovers discrete
// and GMM-HMM models from sampled data.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include <mmathen/stats/gmm.h>
#include <mmathen/stats/hmm.h>
#include <mmathen/stats/random.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
Gmm trueMixture() {
  Gmm g;
  g.weights = {0.3, 0.7};
  g.means = {{-2.0, 1.0}, {3.0, -1.0}};
  g.variances = {{0.5, 1.0}, {1.5, 0.25}};
  return g;
}
std::vector<std::vector<double>> sampleMany(const Gmm& g, std::size_t n, std::uint64_t seed) {
  SplitMix64 rng{seed};
  std::vector<std::vector<double>> d;
  for (std::size_t i = 0; i < n; ++i) d.push_back(g.sample(rng));
  return d;
}
double lse(std::vector<double> v) { return logSumExp(v); }
}  // namespace

TEST_CASE("HMM: trainers refuse empty and malformed models", "[d41hmm][review-regression]") {
  CHECK(baumWelchDiscrete(DiscreteHmm{}, {{0}}).error() == QuantityError::InvalidArgument);
  CHECK(baumWelchGmm(GmmHmm{}, {{{0.0}}}).error() == QuantityError::InvalidArgument);

  const auto discrete = unwrap(makeDiscreteHmm({1.0, 0.0}, {{1.0, 0.0}, {0.0, 1.0}},
                                               {{0.5, 0.5}, {0.25, 0.75}}));
  Gmm gaussian{{1.0}, {{0.0}}, {{1.0}}};
  const GmmHmm continuous{discrete.hmm, {gaussian, gaussian}};
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  std::vector<Hmm> badModels;
  for (int defect = 0; defect < 6; ++defect) {
    Hmm h = discrete.hmm;
    switch (defect) {
      case 0: h.logInitial.clear(); break;
      case 1: h.logTransition.pop_back(); break;
      case 2: h.logTransition[0].pop_back(); break;
      case 3: h.logInitial[0] = nan; break;
      case 4: h.logTransition[0][0] = inf; break;
      case 5: h.logTransition[0] = {-inf, -inf}; break;
    }
    badModels.push_back(std::move(h));
  }
  for (const auto& h : badModels) {
    auto d = discrete;
    auto g = continuous;
    d.hmm = h;
    g.hmm = h;
    CHECK(baumWelchDiscrete(d, {{0}}).error() == QuantityError::InvalidArgument);
    CHECK(baumWelchGmm(g, {{{0.0}}}).error() == QuantityError::InvalidArgument);
    CHECK(forwardLogLikelihood(h, {{0.0, 0.0}}).error() == QuantityError::InvalidArgument);
    CHECK(forwardBackward(h, {{0.0, 0.0}}).error() == QuantityError::InvalidArgument);
    CHECK(viterbi(h, {{0.0, 0.0}}).error() == QuantityError::InvalidArgument);
  }
  for (const auto& emissions : std::vector<LogMatrix>{
           {}, {{0.0}}, {{}, {}}, {{0.0}, {0.0, 0.0}}, {{nan}, {0.0}}, {{-inf}, {0.0}}}) {
    auto d = discrete;
    d.logEmission = emissions;
    CHECK(baumWelchDiscrete(d, {{0}}).error() == QuantityError::InvalidArgument);
  }
  auto g = continuous;
  g.emission.pop_back();
  CHECK(baumWelchGmm(g, {{{0.0}}}).error() == QuantityError::InvalidArgument);
  g = continuous;
  g.emission[0].variances.clear();
  CHECK(baumWelchGmm(g, {{{0.0}}}).error() == QuantityError::InvalidArgument);
  CHECK(baumWelchGmm(continuous, {{{nan}}}).error() == QuantityError::InvalidArgument);

  BaumWelchOptions discreteOptions;
  discreteOptions.maxIterations = 1;
  GmmHmmOptions continuousOptions;
  continuousOptions.maxIterations = 1;
  // Valid zero-probability states/transitions remain supported by both trainers.
  CHECK(baumWelchDiscrete(discrete, {{0, 1}}, discreteOptions).ok());
  CHECK(baumWelchGmm(continuous, {{{0.0}, {1.0}}}, continuousOptions).ok());
  discreteOptions.pseudoCount = inf;
  CHECK(baumWelchDiscrete(discrete, {{0}}, discreteOptions).error() == QuantityError::InvalidArgument);
  continuousOptions.varianceFloor = inf;
  CHECK(baumWelchGmm(continuous, {{{0.0}}}, continuousOptions).error() == QuantityError::InvalidArgument);
}

TEST_CASE("GMM: K=1 is the closed-form MLE; posteriors; outliers", "[d40gmm]") {
  const auto data = sampleMany(trueMixture(), 5000, 7);
  GmmTrainOptions o;
  o.components = 1;
  const auto fit = unwrap(trainGmm(data, o));
  std::vector<double> mean(2, 0.0), var(2, 0.0);
  for (const auto& x : data)
    for (int j = 0; j < 2; ++j) mean[j] += x[j] / 5000.0;
  for (const auto& x : data)
    for (int j = 0; j < 2; ++j) var[j] += (x[j] - mean[j]) * (x[j] - mean[j]) / 5000.0;
  for (int j = 0; j < 2; ++j) {
    CHECK(std::fabs(fit.model.means[0][j] - mean[j]) < 1e-12);
    CHECK(std::fabs(fit.model.variances[0][j] / var[j] - 1.0) < 1e-12);
  }
  CHECK(fit.model.weights[0] == 1.0);
  const auto g = trueMixture();
  const auto p = g.posteriors({0.5, 0.0});
  CHECK(std::fabs(p[0] + p[1] - 1.0) < 1e-15);
  CHECK(std::isfinite(g.logLikelihood(std::vector<double>{1e6, -1e6})));
  // Log-sum-exp of the closed form for a single Gaussian.
  const double ll = g.logLikelihood(std::vector<double>{-2.0, 1.0});
  const double direct = std::log(0.3 / (2.0 * 3.141592653589793 * std::sqrt(0.5 * 1.0)) +
                                 0.7 / (2.0 * 3.141592653589793 * std::sqrt(1.5 * 0.25)) *
                                     std::exp(-0.5 * (25.0 / 1.5 + 4.0 / 0.25)));
  CHECK(std::fabs(ll - direct) < 1e-12);
}

TEST_CASE("GMM: EM recovers a 2-D mixture; monotone; BIC; reproducible", "[d40gmm]") {
  const auto truth = trueMixture();
  const std::size_t n = 20000;
  const auto data = sampleMany(truth, n, 42);
  GmmTrainOptions o;
  o.components = 2;
  const auto fit = unwrap(trainGmm(data, o));
  CHECK(fit.converged);
  CHECK_FALSE(fit.floorApplied);
  for (std::size_t i = 1; i < fit.meanLogLikelihood.size(); ++i)
    CHECK(fit.meanLogLikelihood[i] >= fit.meanLogLikelihood[i - 1] - 1e-12);
  const auto& m = fit.model;
  const std::size_t a = m.means[0][0] < m.means[1][0] ? 0 : 1, b = 1 - a;
  const std::size_t map[2] = {a, b};
  for (std::size_t k = 0; k < 2; ++k) {
    const std::size_t e = map[k];
    const double nk = truth.weights[k] * static_cast<double>(n);
    CHECK(std::fabs(m.weights[e] - truth.weights[k]) <
          4.0 * std::sqrt(truth.weights[k] * (1 - truth.weights[k]) / static_cast<double>(n)));
    for (std::size_t j = 0; j < 2; ++j) {
      const double v = truth.variances[k][j];
      CHECK(std::fabs(m.means[e][j] - truth.means[k][j]) < 4.0 * std::sqrt(v / nk));
      CHECK(std::fabs(m.variances[e][j] - v) < 5.0 * v * std::sqrt(2.0 / nk));
    }
  }
  // BIC: K = 2 beats 1 and 3.
  double bic[4] = {0, 0, 0, 0};
  for (std::size_t k = 1; k <= 3; ++k) {
    GmmTrainOptions ok;
    ok.components = k;
    bic[k] = gmmBic(unwrap(trainGmm(data, ok)).model, data);
  }
  CHECK(bic[2] < bic[1]);
  CHECK(bic[2] < bic[3]);
  // Same data, same seed: bit-identical model.
  const auto again = unwrap(trainGmm(data, o));
  CHECK(again.model.means == fit.model.means);
  CHECK(again.model.variances == fit.model.variances);
  CHECK(trainGmm({}, o).error() == QuantityError::InvalidArgument);
  CHECK(trainGmm({{1.0}, {2.0, 3.0}}, o).error() == QuantityError::InvalidArgument);
}

TEST_CASE("HMM: the Wikipedia Healthy/Fever example", "[d41hmm]") {
  // https://en.wikipedia.org/wiki/Viterbi_algorithm#Example
  const auto m = unwrap(makeDiscreteHmm({0.6, 0.4}, {{0.7, 0.3}, {0.4, 0.6}},
                                        {{0.5, 0.4, 0.1}, {0.1, 0.3, 0.6}}));
  const std::vector<std::size_t> obs = {0, 1, 2};  // normal, cold, dizzy
  const auto v = unwrap(viterbi(m.hmm, m.emissionsFor(obs)));
  CHECK(v.states == std::vector<std::size_t>{0, 0, 1});
  CHECK(std::fabs(std::exp(v.logProbability) - 0.01512) < 1e-12);
  // Forward: the sum over all 8 paths.
  double total = 0.0;
  const double pi[2] = {0.6, 0.4}, a[2][2] = {{0.7, 0.3}, {0.4, 0.6}},
               e[2][3] = {{0.5, 0.4, 0.1}, {0.1, 0.3, 0.6}};
  for (int s0 = 0; s0 < 2; ++s0)
    for (int s1 = 0; s1 < 2; ++s1)
      for (int s2 = 0; s2 < 2; ++s2)
        total += pi[s0] * e[s0][0] * a[s0][s1] * e[s1][1] * a[s1][s2] * e[s2][2];
  CHECK(std::fabs(std::exp(unwrap(forwardLogLikelihood(m.hmm, m.emissionsFor(obs)))) - total) < 1e-15);
}

TEST_CASE("HMM: forward, posteriors and Viterbi equal brute force", "[d41hmm]") {
  SplitMix64 rng{2024};
  auto randomRow = [&](std::size_t n) {
    std::vector<double> r(n);
    double s = 0.0;
    for (auto& v : r) s += (v = 0.05 + rng.uniform());
    for (auto& v : r) v /= s;
    return r;
  };
  for (int trial = 0; trial < 5; ++trial) {
    const std::size_t n = 3, t = 7;
    std::vector<std::vector<double>> a, e;
    for (std::size_t i = 0; i < n; ++i) a.push_back(randomRow(n)), e.push_back(randomRow(4));
    const auto m = unwrap(makeDiscreteHmm(randomRow(n), a, e));
    std::vector<std::size_t> obs;
    for (std::size_t i = 0; i < t; ++i) obs.push_back(static_cast<std::size_t>(rng.uniform() * 4.0));
    const auto b = m.emissionsFor(obs);
    std::vector<double> all;
    std::vector<std::vector<double>> marg(t, std::vector<double>(n, 0.0));
    std::vector<std::vector<std::size_t>> paths;
    std::vector<std::size_t> path(t, 0);
    std::function<void(std::size_t)> rec = [&](std::size_t d) {
      if (d == t) {
        all.push_back(pathLogProbability(m.hmm, b, path));
        paths.push_back(path);
        return;
      }
      for (std::size_t s = 0; s < n; ++s) path[d] = s, rec(d + 1);
    };
    rec(0);
    REQUIRE(all.size() == 2187);
    const double logZ = lse(all);
    for (std::size_t p = 0; p < all.size(); ++p)
      for (std::size_t tt = 0; tt < t; ++tt) marg[tt][paths[p][tt]] += std::exp(all[p] - logZ);
    const auto fb = unwrap(forwardBackward(m.hmm, b));
    CHECK(std::fabs(fb.logLikelihood - logZ) < 1e-12);
    for (std::size_t tt = 0; tt < t; ++tt) {
      double rs = 0.0;
      for (std::size_t s = 0; s < n; ++s) {
        CHECK(std::fabs(fb.posteriors[tt][s] - marg[tt][s]) < 1e-12);
        rs += fb.posteriors[tt][s];
      }
      CHECK(std::fabs(rs - 1.0) < 1e-12);
    }
    const auto v = unwrap(viterbi(m.hmm, b));
    const auto best = static_cast<std::size_t>(std::max_element(all.begin(), all.end()) - all.begin());
    CHECK(std::fabs(v.logProbability - all[best]) < 1e-12);
    CHECK(std::fabs(pathLogProbability(m.hmm, b, v.states) - all[best]) < 1e-12);
  }
}

TEST_CASE("HMM: long sequences; forbidden transitions; refusals", "[d41hmm]") {
  const auto m = unwrap(makeDiscreteHmm({0.5, 0.5}, {{0.9, 0.1}, {0.2, 0.8}},
                                        {{0.7, 0.2, 0.1}, {0.1, 0.3, 0.6}}));
  SplitMix64 rng{5};
  const auto s = sampleDiscreteHmm(m, 200000, rng);
  const auto b = m.emissionsFor(s.symbols);
  const double ll = unwrap(forwardLogLikelihood(m.hmm, b));
  CHECK(std::isfinite(ll));
  CHECK(ll < -100000.0);  // far below double's underflow if done linearly
  const auto fb = unwrap(forwardBackward(m.hmm, b));
  CHECK(std::fabs(fb.logLikelihood - ll) < 1e-9 * std::fabs(ll));
  const auto v = unwrap(viterbi(m.hmm, b));
  CHECK(v.logProbability <= ll);
  std::size_t agree = 0;
  for (std::size_t t = 0; t < s.states.size(); ++t) agree += v.states[t] == s.states[t];
  CHECK(static_cast<double>(agree) / 200000.0 > 0.8);
  // Left-to-right model: no decoded path goes backwards, even when the
  // emissions pull that way.
  const auto lr = unwrap(makeDiscreteHmm({1.0, 0.0, 0.0}, {{0.5, 0.5, 0.0}, {0.0, 0.5, 0.5}, {0.0, 0.0, 1.0}},
                                         {{0.8, 0.1, 0.1}, {0.1, 0.8, 0.1}, {0.1, 0.1, 0.8}}));
  const auto vp = unwrap(viterbi(lr.hmm, lr.emissionsFor({0, 2, 0, 1, 2, 0, 2})));
  for (std::size_t t = 1; t < vp.states.size(); ++t) CHECK(vp.states[t] >= vp.states[t - 1]);
  CHECK(vp.states[0] == 0);
  // An impossible observation sequence is a DomainError, not NaN.
  const auto z = unwrap(makeDiscreteHmm({1.0, 0.0}, {{1.0, 0.0}, {0.0, 1.0}}, {{1.0, 0.0}, {0.0, 1.0}}));
  CHECK(forwardLogLikelihood(z.hmm, z.emissionsFor({0, 1})).error() == QuantityError::DomainError);
  CHECK(viterbi(z.hmm, z.emissionsFor({1})).error() == QuantityError::DomainError);
  CHECK(makeHmm({0.5, 0.6}, {{1.0, 0.0}, {0.0, 1.0}}).error() == QuantityError::InvalidArgument);
  CHECK(viterbi(m.hmm, LogMatrix{}).error() == QuantityError::InvalidArgument);
}

TEST_CASE("Baum-Welch (discrete): monotone and recovers the model", "[d41hmm]") {
  const auto truth = unwrap(makeDiscreteHmm({0.6, 0.4}, {{0.85, 0.15}, {0.25, 0.75}},
                                            {{0.6, 0.3, 0.1}, {0.05, 0.25, 0.7}}));
  SplitMix64 rng{99};
  std::vector<std::vector<std::size_t>> seqs;
  for (int i = 0; i < 60; ++i) seqs.push_back(sampleDiscreteHmm(truth, 500, rng).symbols);
  const auto init = unwrap(makeDiscreteHmm({0.5, 0.5}, {{0.6, 0.4}, {0.4, 0.6}},
                                           {{0.4, 0.35, 0.25}, {0.25, 0.35, 0.4}}));
  const auto fit = unwrap(baumWelchDiscrete(init, seqs));
  CHECK(fit.converged);
  for (std::size_t i = 1; i < fit.logLikelihood.size(); ++i)
    CHECK(fit.logLikelihood[i] >= fit.logLikelihood[i - 1] - 1e-9 * std::fabs(fit.logLikelihood[i]));
  double trueLl = 0.0;
  for (const auto& s : seqs) trueLl += unwrap(forwardLogLikelihood(truth.hmm, truth.emissionsFor(s)));
  CHECK(fit.logLikelihood.back() >= trueLl);  // the ML fit explains the data at least as well
  const auto& h = fit.model;
  for (std::size_t i = 0; i < 2; ++i) {
    for (std::size_t j = 0; j < 2; ++j)
      CHECK(std::fabs(std::exp(h.hmm.logTransition[i][j]) - std::exp(truth.hmm.logTransition[i][j])) < 0.04);
    for (std::size_t k = 0; k < 3; ++k)
      CHECK(std::fabs(std::exp(h.logEmission[i][k]) - std::exp(truth.logEmission[i][k])) < 0.04);
  }
}

TEST_CASE("Baum-Welch (GMM-HMM): monotone, decodes the states", "[d41hmm]") {
  // Two states; each emits from a 2-component 1-D mixture.
  GmmHmm truth;
  truth.hmm = unwrap(makeHmm({0.5, 0.5}, {{0.95, 0.05}, {0.1, 0.9}}));
  truth.emission.resize(2);
  truth.emission[0].weights = {0.5, 0.5};
  truth.emission[0].means = {{-3.0}, {-1.0}};
  truth.emission[0].variances = {{0.3}, {0.3}};
  truth.emission[1].weights = {0.4, 0.6};
  truth.emission[1].means = {{1.5}, {3.5}};
  truth.emission[1].variances = {{0.4}, {0.2}};
  SplitMix64 rng{11};
  std::vector<std::vector<std::vector<double>>> seqs;
  std::vector<std::vector<std::size_t>> states;
  for (int s = 0; s < 20; ++s) {
    std::vector<std::vector<double>> x;
    std::vector<std::size_t> st;
    std::size_t cur = rng.categorical({0.5, 0.5});
    for (int t = 0; t < 300; ++t) {
      if (t > 0) {
        std::vector<double> row(2);
        for (std::size_t j = 0; j < 2; ++j) row[j] = std::exp(truth.hmm.logTransition[cur][j]);
        cur = rng.categorical(row);
      }
      st.push_back(cur);
      x.push_back(truth.emission[cur].sample(rng));
    }
    seqs.push_back(std::move(x));
    states.push_back(std::move(st));
  }
  GmmHmm init = truth;
  init.hmm = unwrap(makeHmm({0.5, 0.5}, {{0.7, 0.3}, {0.3, 0.7}}));
  init.emission[0].means = {{-2.0}, {-0.5}};
  init.emission[0].variances = {{1.0}, {1.0}};
  init.emission[1].weights = {0.5, 0.5};
  init.emission[1].means = {{1.0}, {2.5}};
  init.emission[1].variances = {{1.0}, {1.0}};
  const auto fit = unwrap(baumWelchGmm(init, seqs));
  for (std::size_t i = 1; i < fit.logLikelihood.size(); ++i)
    CHECK(fit.logLikelihood[i] >= fit.logLikelihood[i - 1] - 1e-9 * std::fabs(fit.logLikelihood[i]));
  std::size_t right = 0, total = 0;
  for (std::size_t s = 0; s < seqs.size(); ++s) {
    const auto v = unwrap(viterbi(fit.model.hmm, fit.model.emissionsFor(seqs[s])));
    for (std::size_t t = 0; t < v.states.size(); ++t) right += v.states[t] == states[s][t], ++total;
  }
  CHECK(static_cast<double>(right) / static_cast<double>(total) > 0.97);
  CHECK(std::fabs(std::exp(fit.model.hmm.logTransition[0][0]) - 0.95) < 0.03);
}
