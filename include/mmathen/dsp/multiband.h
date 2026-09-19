#pragma once

// D37 — multiband compressor and dynamic EQ.
//
// Multiband compressor: three bands split by Linkwitz-Riley LR4 crossovers
// (D23) at fLow < fHigh:
//   low  = LP_fLow(x), then through AP_fHigh = LP_fHigh + HP_fHigh
//   mid  = LP_fHigh(HP_fLow(x))
//   high = HP_fHigh(HP_fLow(x))
// For LR4, LP + HP is an all-pass (D23), so mid + high = AP_fHigh(HP_fLow x)
// and the low band's compensation makes the total, with neutral
// compressors, AP_fHigh(AP_fLow(x)): magnitude EXACTLY flat. The
// compensation is built from the very same LP/HP sections in parallel, so
// the identity holds by construction (tested to roundoff against the
// all-pass chain). Each band has its own linked-stereo compressor (D24)
// and can be bypassed.
//
// Dynamic EQ band (downward): a peaking section at (f, Q) whose gain is
//   staticGainDb + g_s,
// g_s <= 0 the smoothed gain reduction of the D24 gain computer applied to
// the level of the band itself (the input through a 0 dB-peak band-pass
// at the same f and Q: D23), attack/release in the log domain as D24. The
// section is re-designed every `controlInterval` samples (32) with
// Biquad::setCoeffs (state kept). Below threshold with a 0 dB static gain
// the section is mathematically the identity (RBJ peaking at 0 dB: b = a),
// so the band is transparent to ROUNDOFF (not to the bit: the direct-form
// sums round, as D21 recorded; measured ~1e-17 here). It is not bypassed,
// because a bypass would leave the filter state stale and click on
// engagement. A de-esser is a dynamic EQ band at 5-8 kHz.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "biquad.h"
#include "dynamics.h"
#include "filters.h"

namespace mmathen {

namespace detail {
// A fixed small cascade of biquads (allocation-free processing).
struct Chain {
  std::vector<Biquad> s;
  void process(const double* in, double* out, std::size_t n) {
    const FrameCount c{static_cast<std::uint64_t>(n)};
    if (s.empty()) {
      if (in != out) std::copy(in, in + n, out);
      return;
    }
    s[0].processBlock(in, out, c);
    for (std::size_t i = 1; i < s.size(); ++i) s[i].processBlock(out, out, c);
  }
  void reset() {
    for (auto& b : s) b.reset();
  }
};
inline Result<Chain, QuantityError> lr4(bool hp, double f, SampleRateHz rate) {
  auto d = designLinkwitzRiley(hp, 4, FrequencyHz{f}, rate);
  if (!d.ok()) return Result<Chain, QuantityError>::failure(d.error());
  Chain c;
  for (const auto& k : d.value()) c.s.emplace_back(k);
  return Result<Chain, QuantityError>::success(std::move(c));
}
}  // namespace detail

struct MultibandParams {
  double lowSplitHz = 200.0;
  double highSplitHz = 2500.0;
  std::array<CompressorParams, 3> band{};
  std::array<bool, 3> bandOn{true, true, true};
};

class MultibandCompressor {
 public:
  static constexpr std::size_t kBlock = 512;

  [[nodiscard]] static Result<MultibandCompressor, QuantityError> create(
      const MultibandParams& p, SampleRateHz rate) {
    using R = Result<MultibandCompressor, QuantityError>;
    const double fs = rate.value();
    if (!(p.lowSplitHz > 20.0) || !(p.highSplitHz > p.lowSplitHz * 1.5) ||
        !(p.highSplitHz < 0.45 * fs))
      return R::failure(QuantityError::InvalidArgument);
    MultibandCompressor m;
    for (int c = 0; c < 2; ++c) {
      auto lp1 = detail::lr4(false, p.lowSplitHz, rate), hp1 = detail::lr4(true, p.lowSplitHz, rate);
      auto lp2 = detail::lr4(false, p.highSplitHz, rate), hp2 = detail::lr4(true, p.highSplitHz, rate);
      auto apl = detail::lr4(false, p.highSplitHz, rate), aph = detail::lr4(true, p.highSplitHz, rate);
      if (!lp1.ok() || !hp1.ok() || !lp2.ok() || !hp2.ok() || !apl.ok() || !aph.ok())
        return R::failure(QuantityError::InvalidArgument);
      auto& ch = m.ch_[static_cast<std::size_t>(c)];
      ch.lp1 = std::move(lp1).value();
      ch.hp1 = std::move(hp1).value();
      ch.lp2 = std::move(lp2).value();
      ch.hp2 = std::move(hp2).value();
      ch.apLp = std::move(apl).value();
      ch.apHp = std::move(aph).value();
    }
    for (std::size_t b = 0; b < 3; ++b) {
      auto c = Compressor::create(p.band[b], rate);
      if (!c.ok()) return R::failure(c.error());
      m.comp_[b] = std::move(c).value();
    }
    m.p_ = p;
    return R::success(std::move(m));
  }

  // Band compressor settings and on/off; allocation-free (splits fixed).
  [[nodiscard]] Result<MultibandParams, QuantityError> setBands(const MultibandParams& p) {
    using R = Result<MultibandParams, QuantityError>;
    for (std::size_t b = 0; b < 3; ++b)
      if (!comp_[b]->setParams(p.band[b]).ok()) return R::failure(QuantityError::InvalidArgument);
    MultibandParams q = p;
    q.lowSplitHz = p_.lowSplitHz;
    q.highSplitHz = p_.highSplitHz;
    p_ = q;
    return R::success(q);
  }

  void processStereo(const double* inL, const double* inR, double* outL, double* outR,
                     FrameCount n) {
    std::size_t done = 0;
    const auto total = static_cast<std::size_t>(n.value());
    while (done < total) {
      const std::size_t k = std::min(kBlock, total - done);
      split(0, inL + done, k);
      split(1, inR + done, k);
      for (std::size_t b = 0; b < 3; ++b)
        if (p_.bandOn[b])
          comp_[b]->processStereo(band_[0][b].data(), band_[1][b].data(), band_[0][b].data(),
                                  band_[1][b].data(), FrameCount{static_cast<std::uint64_t>(k)});
      for (std::size_t i = 0; i < k; ++i) {
        outL[done + i] = band_[0][0][i] + band_[0][1][i] + band_[0][2][i];
        outR[done + i] = band_[1][0][i] + band_[1][1][i] + band_[1][2][i];
      }
      done += k;
    }
  }

  void reset() {
    for (auto& c : ch_) {
      c.lp1.reset(), c.hp1.reset(), c.lp2.reset(), c.hp2.reset(), c.apLp.reset(), c.apHp.reset();
    }
    for (auto& c : comp_) c->reset();
  }

  [[nodiscard]] double gainReductionDb(std::size_t band) const {
    return band < 3 && p_.bandOn[band] ? comp_[band]->gainReductionDb() : 0.0;
  }
  [[nodiscard]] const MultibandParams& params() const noexcept { return p_; }

 private:
  struct Channel {
    detail::Chain lp1, hp1, lp2, hp2, apLp, apHp;
  };
  MultibandCompressor() = default;

  void split(std::size_t c, const double* in, std::size_t k) {
    auto& ch = ch_[c];
    auto& low = band_[c][0];
    auto& mid = band_[c][1];
    auto& high = band_[c][2];
    ch.lp1.process(in, low.data(), k);
    ch.hp1.process(in, rest_.data(), k);
    ch.lp2.process(rest_.data(), mid.data(), k);
    ch.hp2.process(rest_.data(), high.data(), k);
    // Low band all-pass compensation: LP + HP at the high split, in parallel.
    ch.apLp.process(low.data(), tmpA_.data(), k);
    ch.apHp.process(low.data(), tmpB_.data(), k);
    for (std::size_t i = 0; i < k; ++i) low[i] = tmpA_[i] + tmpB_[i];
  }

  MultibandParams p_;
  std::array<Channel, 2> ch_;
  std::array<std::optional<Compressor>, 3> comp_;
  std::array<std::array<std::array<double, kBlock>, 3>, 2> band_{};
  std::array<double, kBlock> rest_{}, tmpA_{}, tmpB_{};
};

struct DynamicEqBand {
  FrequencyHz frequency{6500.0};
  double q = 2.0;
  double staticGainDb = 0.0;
  double thresholdDb = -30.0;
  double ratio = 4.0;
  double kneeDb = 6.0;
  double attackMs = 2.0;
  double releaseMs = 80.0;
  double rangeDb = 12.0;  // the most the band can cut dynamically
};

class DynamicEq {
 public:
  static constexpr std::size_t kControl = 32;

  [[nodiscard]] static Result<DynamicEq, QuantityError> create(const DynamicEqBand& b,
                                                              SampleRateHz rate) {
    using R = Result<DynamicEq, QuantityError>;
    auto bp = makeBandpass(b.frequency, b.q, rate);
    auto pk = makePeaking(b.frequency, b.staticGainDb, b.q, rate);
    CompressorParams cp;
    cp.thresholdDb = b.thresholdDb;
    cp.ratio = b.ratio;
    cp.kneeDb = b.kneeDb;
    cp.attackMs = b.attackMs;
    cp.releaseMs = b.releaseMs;
    auto c = Compressor::create(cp, rate);
    if (!bp.ok() || !pk.ok() || !c.ok() || !std::isfinite(b.rangeDb) || b.rangeDb < 0.0 ||
        b.rangeDb > 48.0)
      return R::failure(QuantityError::InvalidArgument);
    DynamicEq d;
    d.b_ = b;
    d.rate_ = rate;
    d.cp_ = cp;
    d.appliedDb_ = b.staticGainDb;
    for (int c2 = 0; c2 < 2; ++c2) {
      d.side_[static_cast<std::size_t>(c2)] = Biquad{bp.value()};
      d.peak_[static_cast<std::size_t>(c2)] = Biquad{pk.value()};
    }
    return R::success(std::move(d));
  }

  // Linked stereo (max of the two side-chain levels). In/out may alias.
  void processStereo(const double* inL, const double* inR, double* outL, double* outR,
                     FrameCount n) {
    const auto total = static_cast<std::size_t>(n.value());
    const double aA = std::exp(-1.0 / (cp_.attackMs * 1e-3 * rate_.value()));
    const double aR = std::exp(-1.0 / (cp_.releaseMs * 1e-3 * rate_.value()));
    std::size_t done = 0;
    while (done < total) {
      const std::size_t k = std::min(kControl, total - done);
      // Side-chain level over this control block (peak of the band signal).
      double peak = 0.0;
      for (std::size_t i = 0; i < k; ++i) {
        double sl = inL[done + i], sr = inR[done + i];
        side_[0]->processBlock(&sl, &sl, FrameCount{1});
        side_[1]->processBlock(&sr, &sr, FrameCount{1});
        peak = std::max(peak, std::max(std::fabs(sl), std::fabs(sr)));
      }
      const double xDb = peak > 0.0 ? 20.0 * std::log10(peak) : -300.0;
      const double gc = std::max(compressorCurveDb(cp_, xDb) - xDb, -b_.rangeDb);
      // Block-rate one-pole in dB (attack/release per block of k samples).
      const double a = gc < gs_ ? std::pow(aA, static_cast<double>(k))
                                : std::pow(aR, static_cast<double>(k));
      gs_ = a * gs_ + (1.0 - a) * gc;
      const double gainDb = b_.staticGainDb + gs_;
      if (std::fabs(gainDb - appliedDb_) > 1e-9) {
        auto pk = makePeaking(b_.frequency, gainDb, b_.q, rate_);
        if (pk.ok()) {
          peak_[0]->setCoeffs(pk.value());
          peak_[1]->setCoeffs(pk.value());
          appliedDb_ = gainDb;
        }
      }
      const FrameCount c{static_cast<std::uint64_t>(k)};
      peak_[0]->processBlock(inL + done, outL + done, c);
      peak_[1]->processBlock(inR + done, outR + done, c);
      done += k;
    }
  }

  // Retune in place: new band parameters, filter and detector state kept,
  // allocation-free (safe on an audio thread). On error nothing changes.
  [[nodiscard]] Result<DynamicEqBand, QuantityError> setBand(const DynamicEqBand& b) {
    using R = Result<DynamicEqBand, QuantityError>;
    auto bp = makeBandpass(b.frequency, b.q, rate_);
    auto pk = makePeaking(b.frequency, b.staticGainDb + gs_, b.q, rate_);
    CompressorParams cp;
    cp.thresholdDb = b.thresholdDb;
    cp.ratio = b.ratio;
    cp.kneeDb = b.kneeDb;
    cp.attackMs = b.attackMs;
    cp.releaseMs = b.releaseMs;
    if (!bp.ok() || !pk.ok() || !detail::validCompressor(cp) || !std::isfinite(b.rangeDb) ||
        b.rangeDb < 0.0 || b.rangeDb > 48.0)
      return R::failure(QuantityError::InvalidArgument);
    for (int c = 0; c < 2; ++c) {
      side_[static_cast<std::size_t>(c)]->setCoeffs(bp.value());
      peak_[static_cast<std::size_t>(c)]->setCoeffs(pk.value());
    }
    b_ = b;
    cp_ = cp;
    appliedDb_ = b.staticGainDb + gs_;
    return R::success(b);
  }

  [[nodiscard]] double dynamicGainDb() const noexcept { return gs_; }
  void reset() {
    side_[0]->reset(), side_[1]->reset(), peak_[0]->reset(), peak_[1]->reset();
    gs_ = 0.0;
  }

 private:
  DynamicEq() = default;
  DynamicEqBand b_;
  SampleRateHz rate_{48000.0};
  CompressorParams cp_;
  std::array<std::optional<Biquad>, 2> side_, peak_;
  double gs_ = 0.0;
  double appliedDb_ = 0.0;
};

}  // namespace mmathen
