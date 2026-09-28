// D42 — Markov chains and n-gram language models (Catch2).
//
// Markov: the two-state chain's stationary distribution, n-step law and
// entropy rate equal their closed forms; periods (1, 2, 3) and
// reducibility are classified; a non-unique stationary law is refused;
// sampling and fitting agree with the chain within CLT bounds.
// n-grams: every conditional distribution sums to 1 for all three
// smoothings (seen and unseen contexts); MLE and add-k equal their
// formulas; Kneser-Ney equals a hand calculation on a 3-sentence corpus;
// and, as an independent accuracy check, a KN trigram trained on 200,000
// tokens from a known second-order source reaches the source's true
// entropy rate (computed exactly from the pair chain's stationary law)
// on held-out data, while a bigram can't. With little data, MLE assigns
// zero probability (infinite perplexity) and KN beats add-one.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include <mmathen/stats/markov.h>
#include <mmathen/stats/ngram.h>
#include <mmathen/stats/random.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
double h2(double p) { return -(p * std::log(p) + (1 - p) * std::log(1 - p)); }
}  // namespace

TEST_CASE("Markov fitting refuses invalid pseudocounts and overflowing totals", "[d42markov][review-regression]") {
  for (double bad : {std::numeric_limits<double>::quiet_NaN(),
                     std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity(), -1.0})
    CHECK(fitMarkovChain({{0, 1}}, 2, bad).error() == QuantityError::InvalidArgument);
  CHECK(fitMarkovChain({{0, 1}}, 2, std::numeric_limits<double>::max()).error() ==
        QuantityError::RangeError);
  const auto mle = unwrap(fitMarkovChain({{0, 1}}, 2));
  CHECK(mle.p == std::vector<std::vector<double>>{{0.0, 1.0}, {0.5, 0.5}});
  const auto smoothed = unwrap(fitMarkovChain({{0, 1}}, 2, 0.5));
  CHECK(smoothed.p == std::vector<std::vector<double>>{{0.25, 0.75}, {0.5, 0.5}});
}

TEST_CASE("Markov chain: closed forms for two states", "[d42markov]") {
  const double p = 0.3, q = 0.1;
  const auto c = unwrap(makeMarkovChain({{1 - p, p}, {q, 1 - q}}));
  const auto pi = unwrap(stationaryDistribution(c));
  CHECK(std::fabs(pi[0] - q / (p + q)) < 1e-15);
  CHECK(std::fabs(pi[1] - p / (p + q)) < 1e-15);
  CHECK(std::fabs(entropyRate(c, pi) - (q * h2(p) + p * h2(q)) / (p + q)) < 1e-15);
  for (std::size_t n : {1u, 2u, 5u, 40u}) {
    const auto d = distributionAfter(c, {1.0, 0.0}, n);
    CHECK(std::fabs(d[0] - (q / (p + q) + p / (p + q) * std::pow(1 - p - q, static_cast<double>(n)))) < 1e-14);
  }
  CHECK(unwrap(chainPeriod(c)) == 1);
}

TEST_CASE("Markov chain: periods, reducibility, sampling, fitting", "[d42markov]") {
  const auto cyc = unwrap(makeMarkovChain({{0, 1, 0}, {0, 0, 1}, {1, 0, 0}}));
  CHECK(unwrap(chainPeriod(cyc)) == 3);
  const auto bip = unwrap(makeMarkovChain({{0, 0.5, 0, 0.5}, {0.5, 0, 0.5, 0}, {0, 0.5, 0, 0.5}, {0.5, 0, 0.5, 0}}));
  CHECK(unwrap(chainPeriod(bip)) == 2);
  const auto cycPi = unwrap(stationaryDistribution(cyc));
  for (double v : cycPi) CHECK(std::fabs(v - 1.0 / 3.0) < 1e-15);
  // Two closed classes: not unique.
  const auto two = unwrap(makeMarkovChain({{1, 0, 0}, {0, 1, 0}, {0.5, 0.25, 0.25}}));
  CHECK_FALSE(isIrreducible(two));
  CHECK(chainPeriod(two).error() == QuantityError::DomainError);
  CHECK(stationaryDistribution(two).error() == QuantityError::DomainError);
  // One closed class plus a transient state: unique, zero on the transient.
  const auto tr = unwrap(makeMarkovChain({{0.5, 0.5, 0}, {0.2, 0.8, 0}, {0.3, 0.3, 0.4}}));
  const auto trPi = unwrap(stationaryDistribution(tr));
  CHECK(trPi[2] == 0.0);
  CHECK(std::fabs(trPi[0] - 2.0 / 7.0) < 1e-15);
  // Sampling: empirical frequencies within 5 sigma (autocorrelated chain:
  // the effective sample size uses the lag-1 eigenvalue 1 - p - q).
  const double p = 0.3, q = 0.1;
  const auto c = unwrap(makeMarkovChain({{1 - p, p}, {q, 1 - q}}));
  SplitMix64 rng{3};
  const auto s = sampleChain(c, 0, 200000, rng);
  double f1 = 0.0;
  for (auto v : s) f1 += static_cast<double>(v);
  f1 /= 200000.0;
  const double lam = 1 - p - q, pi1 = p / (p + q);
  const double sd = std::sqrt(pi1 * (1 - pi1) / 200000.0 * (1 + lam) / (1 - lam));
  CHECK(std::fabs(f1 - pi1) < 5.0 * sd);
  const auto fit = unwrap(fitMarkovChain({s}, 2));
  CHECK(std::fabs(fit.p[0][1] - p) < 0.01);
  CHECK(std::fabs(fit.p[1][0] - q) < 0.005);
  CHECK(makeMarkovChain({{0.5, 0.6}, {1, 0}}).error() == QuantityError::InvalidArgument);
  CHECK(fitMarkovChain({{0, 5}}, 2).error() == QuantityError::InvalidArgument);
}

TEST_CASE("n-gram: every distribution sums to 1; MLE and add-k formulas", "[d42ngram]") {
  Vocabulary v;
  auto sent = [&](std::initializer_list<const char*> w) {
    std::vector<Token> s;
    for (const char* x : w) s.push_back(v.add(x));
    return s;
  };
  const std::vector<std::vector<Token>> corpus = {
      sent({"the", "cat", "sat"}), sent({"the", "dog", "sat"}), sent({"a", "cat", "ran"}),
      sent({"the", "cat", "ran", "fast"}), sent({"a", "dog", "sat", "down"})};
  const std::size_t vs = v.size();
  for (const auto sm : {Smoothing::MaximumLikelihood, Smoothing::AddK, Smoothing::KneserNey})
    for (std::size_t order : {1u, 2u, 3u}) {
      NgramOptions o;
      o.order = order;
      o.smoothing = sm;
      o.addK = 0.5;
      const auto m = unwrap(NgramModel::train(corpus, vs, o));
      std::vector<std::vector<Token>> contexts;
      for (std::size_t k = 1; k <= order; ++k)
        for (const auto& [h, st] : m.table(k)) contexts.push_back(h);
      contexts.push_back({v.lookup("dog"), v.lookup("fast")});  // unseen
      contexts.push_back({kUnkToken});
      for (const auto& h : contexts) {
        double s = 0.0;
        for (Token w = 0; w < vs; ++w) s += m.prob(h, w);
        CHECK(std::fabs(s - 1.0) < 1e-12);
        CHECK(m.prob(h, kBosToken) == 0.0);
      }
    }
  NgramOptions mle;
  mle.order = 2;
  mle.smoothing = Smoothing::MaximumLikelihood;
  const auto m = unwrap(NgramModel::train(corpus, vs, mle));
  // "the" is followed by cat, dog, cat: P(cat | the) = 2/3.
  CHECK(std::fabs(m.prob({v.lookup("the")}, v.lookup("cat")) - 2.0 / 3.0) < 1e-15);
  CHECK(m.prob({v.lookup("the")}, v.lookup("ran")) == 0.0);
  NgramOptions ak = mle;
  ak.smoothing = Smoothing::AddK;
  ak.addK = 1.0;
  const auto a = unwrap(NgramModel::train(corpus, vs, ak));
  CHECK(std::fabs(a.prob({v.lookup("the")}, v.lookup("cat")) - 3.0 / (3.0 + static_cast<double>(vs - 1))) < 1e-15);
  // A huge k is the uniform model: perplexity = V - 1.
  ak.addK = 1e12;
  const auto u = unwrap(NgramModel::train(corpus, vs, ak));
  CHECK(std::fabs(u.perplexity(corpus) - static_cast<double>(vs - 1)) < 1e-6);
  CHECK(NgramModel::train(corpus, vs, NgramOptions{0}).error() == QuantityError::InvalidArgument);
  CHECK(NgramModel::train({{kBosToken}}, vs).error() == QuantityError::InvalidArgument);
}

TEST_CASE("n-gram: Kneser-Ney equals a hand calculation", "[d42ngram]") {
  // Tokens: a=3, b=4, c=5 (V = 6, 5 predictable). Corpus: [a b], [a c],
  // [b c]; bigram KN. Too few counts-of-counts for the modified discounts,
  // so each order uses the single discount D = n1 / (n1 + 2 n2).
  // Order 1 (continuation counts a 1, b 2, c 2, </s> 2; total 7):
  //   n1 = 1, n2 = 3 -> D = 1/7; gamma = (4/7)(1/7) = 4/49;
  //   P1(a) = (6/7)/7 + (4/49)/5 = 34/245; P1(b,c,</s>) = 69/245; P1(unk) = 4/245.
  // Order 2 (raw bigrams; n1 = 5, n2 = 2 -> D = 5/9):
  //   P(c | a)   = (1 - 5/9)/2 + (5/9)(69/245)          = 2/9 + 23/147
  //   P(a | <s>) = (2 - 5/9)/3 + ((5/9)(2)/3)(34/245)   = 13/27 + 68/1323
  //   P(unk | a) = (5/9)(4/245)                          = 4/441
  NgramOptions o;
  o.order = 2;
  const auto m = unwrap(NgramModel::train({{3, 4}, {3, 5}, {4, 5}}, 6, o));
  CHECK(std::fabs(m.prob({3}, 5) - (2.0 / 9.0 + 23.0 / 147.0)) < 1e-15);
  CHECK(std::fabs(m.prob({kBosToken}, 3) - (13.0 / 27.0 + 68.0 / 1323.0)) < 1e-15);
  CHECK(std::fabs(m.prob({3}, kUnkToken) - 4.0 / 441.0) < 1e-15);
  CHECK(std::fabs(m.discounts(2)[0] - 5.0 / 9.0) < 1e-15);
  CHECK(std::fabs(m.discounts(1)[0] - 1.0 / 7.0) < 1e-15);
}

TEST_CASE("n-gram: KN trigram reaches a known source's entropy rate", "[d42ngram]") {
  // Second-order source over 6 symbols (tokens 3..8): P(w | u, v) drawn
  // once from a fixed seed, deliberately uneven.
  const std::size_t k = 6;
  SplitMix64 gen{77};
  std::vector<std::vector<double>> cond(k * k, std::vector<double>(k));
  for (auto& row : cond) {
    double s = 0.0;
    for (auto& p : row) s += (p = std::pow(gen.uniform(), 3.0) + 0.01);
    for (auto& p : row) p /= s;
  }
  // Exact entropy rate from the pair chain (u,v) -> (v,w).
  std::vector<std::vector<double>> pair(k * k, std::vector<double>(k * k, 0.0));
  for (std::size_t u = 0; u < k; ++u)
    for (std::size_t v = 0; v < k; ++v)
      for (std::size_t w = 0; w < k; ++w) pair[u * k + v][v * k + w] = cond[u * k + v][w];
  const auto chain = unwrap(makeMarkovChain(pair));
  const auto pi = unwrap(stationaryDistribution(chain));
  const double hTrue = entropyRate(chain, pi);
  auto draw = [&](std::size_t sentences, std::size_t len, std::uint64_t seed) {
    SplitMix64 r{seed};
    const auto pairs = sampleChain(chain, 0, sentences * len + 1000, r);
    std::vector<std::vector<Token>> out;
    std::size_t t = 1000;  // burn-in
    for (std::size_t s = 0; s < sentences; ++s) {
      std::vector<Token> sent;
      for (std::size_t i = 0; i < len; ++i) sent.push_back(static_cast<Token>(3 + pairs[t++] % k));
      out.push_back(std::move(sent));
    }
    return out;
  };
  // Held-out cross-entropy per token, skipping each sentence's first two
  // tokens (padded contexts) and </s>, so it measures the stationary rate.
  auto crossEntropy = [&](const NgramModel& m, const std::vector<std::vector<Token>>& test) {
    double lp = 0.0, n = 0.0;
    for (const auto& s : test)
      for (std::size_t i = 2; i < s.size(); ++i) {
        lp -= std::log(m.prob({s[i - 2], s[i - 1]}, s[i]));
        n += 1.0;
      }
    return lp / n;
  };
  const auto train = draw(50, 4000, 1), test = draw(10, 4000, 2);
  NgramOptions o3;
  const auto kn3 = unwrap(NgramModel::train(train, 9, o3));
  const double h3 = crossEntropy(kn3, test);
  // Held-out estimate of the true rate on the same test data.
  double hRef = 0.0, nRef = 0.0;
  for (const auto& s : test)
    for (std::size_t i = 2; i < s.size(); ++i)
      hRef -= std::log(cond[(s[i - 2] - 3) * k + (s[i - 1] - 3)][s[i] - 3]), nRef += 1.0;
  hRef /= nRef;
  CHECK(std::fabs(hRef / hTrue - 1.0) < 0.01);  // the sample is representative
  CHECK(h3 >= hRef - 1e-3);                      // no model beats the truth (up to noise)
  CHECK(h3 / hRef - 1.0 < 0.005);                // KN trigram within 0.5 % of it
  NgramOptions o2;
  o2.order = 2;
  const double h2b = crossEntropy(unwrap(NgramModel::train(train, 9, o2)), test);
  CHECK(h2b > h3 * 1.02);  // a bigram cannot capture a second-order source
  // Little data: MLE hits unseen trigrams, KN beats add-one.
  const auto small = draw(2, 500, 3);
  NgramOptions mle = o3;
  mle.smoothing = Smoothing::MaximumLikelihood;
  CHECK(std::isinf(unwrap(NgramModel::train(small, 9, mle)).perplexity(test)));
  NgramOptions add1 = o3;
  add1.smoothing = Smoothing::AddK;
  const double pKn = unwrap(NgramModel::train(small, 9, o3)).perplexity(test);
  const double pAdd = unwrap(NgramModel::train(small, 9, add1)).perplexity(test);
  CHECK(pKn < pAdd);
  // Generation: sentences of valid tokens that end.
  SplitMix64 rng{8};
  const auto g = kn3.generate(rng, 50);
  for (Token t : g) CHECK((t >= 3 && t <= 8));
}
