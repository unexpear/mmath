#pragma once

// D42 — n-gram statistical language models (order n = a Markov chain of
// order n-1 over tokens), with MLE, add-k and interpolated modified
// Kneser-Ney smoothing, perplexity and sampling.
//
// Tokens are integers, so the same model serves words, chord symbols,
// pitches or MFCC cluster labels; `Vocabulary` maps strings to ids.
// Reserved ids: 0 <unk>, 1 <s>, 2 </s>. Each training sentence is padded
// with n-1 <s> on the left and one </s> on the right; <s> is context only
// and never predicted, so the predicted vocabulary has V - 1 entries.
//
// Interpolated modified Kneser-Ney (Chen & Goodman 1998, TR-10-98, sec.
// 3.5 and eq. 26), for order k with context h (k-1 tokens):
//   P_k(w|h) = max(c_k(hw) - D_k(c), 0) / c_k(h.)  +  g_k(h) P_{k-1}(w|h')
//   g_k(h)   = (D_k1 N1(h.) + D_k2 N2(h.) + D_k3+ N3+(h.)) / c_k(h.)
//   unseen h: P_k(w|h) = P_{k-1}(w|h');  P_0(w) = 1 / (V - 1).
// c_n are raw counts; lower orders use continuation counts N1+(. hw), the
// number of distinct left extensions, except for n-grams that start with
// <s>, which cannot be extended and keep their raw counts (as KenLM does).
// Discounts per order from its count-of-counts n1..n4:
//   Y = n1 / (n1 + 2 n2), D1 = 1 - 2Y n2/n1, D2 = 2 - 3Y n3/n2,
//   D3+ = 3 - 4Y n4/n3. On data far from Zipfian these can leave (0, i]
//   (measured: D2 <= 0 on 1,000 tokens of a 6-symbol source), and a
//   zero discount leaves unseen words no mass at all; KenLM refuses such
//   data. Here that order falls back to the original single discount
//   D = Y (Ney, Essen & Kneser 1994), and to D = 0.5 / 1.0 / 1.5 when n1
//   or n2 is zero (tiny corpora).
// Every D lies in (0, i], so D(c) <= c: the discounted mass equals g_k(h)
// exactly, every conditional distribution sums to 1 (tested for every
// context), and every seen context gives unseen words positive mass.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "random.h"

namespace mmathen {

using Token = std::uint32_t;
inline constexpr Token kUnkToken = 0, kBosToken = 1, kEosToken = 2;

class Vocabulary {
 public:
  Vocabulary() : names_{"<unk>", "<s>", "</s>"} {
    for (Token i = 0; i < 3; ++i) ids_[names_[i]] = i;
  }
  Token add(const std::string& s) {
    auto it = ids_.find(s);
    if (it != ids_.end()) return it->second;
    const auto id = static_cast<Token>(names_.size());
    names_.push_back(s);
    ids_[s] = id;
    return id;
  }
  [[nodiscard]] Token lookup(const std::string& s) const {
    auto it = ids_.find(s);
    return it == ids_.end() ? kUnkToken : it->second;
  }
  [[nodiscard]] const std::string& name(Token t) const { return names_.at(t); }
  [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }

 private:
  std::vector<std::string> names_;
  std::map<std::string, Token> ids_;
};

enum class Smoothing { MaximumLikelihood, AddK, KneserNey };

struct NgramOptions {
  std::size_t order = 3;
  Smoothing smoothing = Smoothing::KneserNey;
  double addK = 1.0;
};

class NgramModel {
 public:
  struct ContextStats {
    std::map<Token, double> counts;
    double total = 0.0;
    double n1 = 0, n2 = 0, n3 = 0;  // words with count 1, 2, >= 3
  };
  using Table = std::map<std::vector<Token>, ContextStats>;

  // sentences: token ids in [0, vocabSize), without padding; vocabSize
  // includes the three reserved ids.
  [[nodiscard]] static Result<NgramModel, QuantityError> train(
      const std::vector<std::vector<Token>>& sentences, std::size_t vocabSize,
      const NgramOptions& o = {}) {
    using R = Result<NgramModel, QuantityError>;
    if (o.order < 1 || o.order > 10 || vocabSize < 4 || sentences.empty() ||
        !(o.addK > 0.0) || !std::isfinite(o.addK))
      return R::failure(QuantityError::InvalidArgument);
    NgramModel m;
    m.o_ = o;
    m.vocab_ = vocabSize;
    m.tables_.resize(o.order + 1);  // tables_[k]: contexts of length k-1
    // Raw counts at every order (MLE / add-k use raw counts throughout).
    std::map<std::vector<Token>, double> raw;
    for (const auto& s : sentences) {
      std::vector<Token> padded(o.order - 1, kBosToken);
      for (Token t : s) {
        if (t >= vocabSize || t == kBosToken) return R::failure(QuantityError::InvalidArgument);
        padded.push_back(t);
      }
      padded.push_back(kEosToken);
      for (std::size_t i = o.order - 1; i < padded.size(); ++i)
        for (std::size_t k = 1; k <= o.order; ++k)
          raw[std::vector<Token>(padded.begin() + static_cast<std::ptrdiff_t>(i + 1 - k),
                                 padded.begin() + static_cast<std::ptrdiff_t>(i + 1))] += 1.0;
    }
    if (o.smoothing != Smoothing::KneserNey) {
      for (const auto& [g, c] : raw) m.addCount(g, c);
    } else {
      // Highest order: raw. Lower orders: continuation counts, except
      // n-grams starting with <s>.
      std::map<std::vector<Token>, double> cont;
      for (const auto& [g, c] : raw) {
        if (g.size() == o.order) m.addCount(g, c);
        if (g.size() >= 2) cont[std::vector<Token>(g.begin() + 1, g.end())] += 1.0;
      }
      for (const auto& [g, c] : raw)
        if (g.size() < o.order) m.addCount(g, g[0] == kBosToken ? c : cont[g]);
    }
    for (auto& t : m.tables_)
      for (auto& [h, st] : t) {
        st.total = 0.0;
        for (const auto& [w, c] : st.counts) {
          st.total += c;
          if (c == 1.0) ++st.n1;
          else if (c == 2.0) ++st.n2;
          else if (c >= 3.0) ++st.n3;
        }
      }
    // Discounts per order from counts-of-counts of that order's counts.
    m.d_.assign(o.order + 1, {0.5, 1.0, 1.5});
    for (std::size_t k = 1; k <= o.order; ++k) {
      double nn[5] = {0, 0, 0, 0, 0};
      for (const auto& [h, st] : m.tables_[k])
        for (const auto& [w, c] : st.counts)
          if (c >= 1.0 && c <= 4.0 && c == std::floor(c)) nn[static_cast<int>(c)] += 1.0;
      if (nn[1] > 0 && nn[2] > 0) {
        const double y = nn[1] / (nn[1] + 2.0 * nn[2]);
        m.d_[k] = {y, y, y};  // original KN (Ney, Essen & Kneser 1994)
        if (nn[3] > 0 && nn[4] > 0) {
          const std::vector<double> mod = {1.0 - 2.0 * y * nn[2] / nn[1],
                                           2.0 - 3.0 * y * nn[3] / nn[2],
                                           3.0 - 4.0 * y * nn[4] / nn[3]};
          if (mod[0] > 0.0 && mod[0] <= 1.0 && mod[1] > 0.0 && mod[1] <= 2.0 &&
              mod[2] > 0.0 && mod[2] <= 3.0)
            m.d_[k] = mod;
        }
      }
    }
    return R::success(std::move(m));
  }

  // P(w | context); only the last order-1 tokens of `context` are used, and
  // a shorter context is left-padded with <s>.
  [[nodiscard]] double prob(const std::vector<Token>& context, Token w) const {
    if (w == kBosToken || w >= vocab_) return 0.0;
    std::vector<Token> h(o_.order - 1, kBosToken);
    const std::size_t take = std::min(context.size(), o_.order - 1);
    std::copy(context.end() - static_cast<std::ptrdiff_t>(take), context.end(),
              h.end() - static_cast<std::ptrdiff_t>(take));
    return probOrder(h, w, o_.order);
  }

  // Natural-log probability of a sentence including </s>.
  [[nodiscard]] double sentenceLogProb(const std::vector<Token>& s) const {
    std::vector<Token> h(o_.order - 1, kBosToken);
    double lp = 0.0;
    auto step = [&](Token w) {
      lp += std::log(prob(h, w));
      if (!h.empty()) {
        h.erase(h.begin());
        h.push_back(w);
      }
    };
    for (Token t : s) step(t);
    step(kEosToken);
    return lp;
  }
  // exp(-(sum log P) / predicted tokens); +inf if any token has P = 0.
  [[nodiscard]] double perplexity(const std::vector<std::vector<Token>>& sentences) const {
    double lp = 0.0, n = 0.0;
    for (const auto& s : sentences) {
      lp += sentenceLogProb(s);
      n += static_cast<double>(s.size() + 1);
    }
    return std::exp(-lp / n);
  }

  // Sample a sentence (without padding), stopping at </s> or maxLength.
  [[nodiscard]] std::vector<Token> generate(SplitMix64& rng, std::size_t maxLength = 100) const {
    std::vector<Token> out, h(o_.order - 1, kBosToken);
    std::vector<double> p(vocab_);
    while (out.size() < maxLength) {
      for (Token w = 0; w < vocab_; ++w) p[w] = prob(h, w);
      const auto w = static_cast<Token>(rng.categorical(p));
      if (w == kEosToken) break;
      out.push_back(w);
      if (!h.empty()) {
        h.erase(h.begin());
        h.push_back(w);
      }
    }
    return out;
  }

  [[nodiscard]] std::size_t order() const noexcept { return o_.order; }
  [[nodiscard]] std::size_t vocabularySize() const noexcept { return vocab_; }
  [[nodiscard]] const Table& table(std::size_t k) const { return tables_.at(k); }
  [[nodiscard]] std::vector<double> discounts(std::size_t k) const { return d_.at(k); }

 private:
  void addCount(const std::vector<Token>& g, double c) {
    std::vector<Token> h(g.begin(), g.end() - 1);
    tables_[g.size()][h].counts[g.back()] += c;
  }
  [[nodiscard]] double discount(std::size_t k, double c) const {
    if (c <= 0.0) return 0.0;
    if (c == 1.0) return d_[k][0];
    if (c == 2.0) return d_[k][1];
    return d_[k][2];
  }
  [[nodiscard]] double probOrder(const std::vector<Token>& h, Token w, std::size_t k) const {
    const double predicted = static_cast<double>(vocab_ - 1);
    if (k == 0) return 1.0 / predicted;
    const std::vector<Token> lower(h.begin() + (h.empty() ? 0 : 1), h.end());
    auto it = tables_[k].find(h);
    const bool seen = it != tables_[k].end() && it->second.total > 0.0;
    double c = 0.0;
    if (seen) {
      auto wi = it->second.counts.find(w);
      if (wi != it->second.counts.end()) c = wi->second;
    }
    switch (o_.smoothing) {
      case Smoothing::MaximumLikelihood:
        if (!seen) return k > 1 ? probOrder(lower, w, k - 1) : 0.0;
        return c / it->second.total;
      case Smoothing::AddK:
        if (!seen) return 1.0 / predicted;
        return (c + o_.addK) / (it->second.total + o_.addK * predicted);
      case Smoothing::KneserNey:
      default: {
        const double below = probOrder(lower, w, k - 1);
        if (!seen) return below;
        const auto& st = it->second;
        const double gamma =
            (d_[k][0] * st.n1 + d_[k][1] * st.n2 + d_[k][2] * st.n3) / st.total;
        return std::max(c - discount(k, c), 0.0) / st.total + gamma * below;
      }
    }
  }

  NgramOptions o_;
  std::size_t vocab_ = 0;
  std::vector<Table> tables_;
  std::vector<std::vector<double>> d_;
};

}  // namespace mmathen
