#pragma once

// D21 — parametric and graphic equalizers: cascades of the RBJ sections in
// biquad.h, processed in series (one Biquad per band, D5 contract:
// caller buffers, allocation-free block processing, exact reset).
//
// Response: the cascade's magnitude is the product of the sections', so
// responseDb(f) = sum_i 20 log10 |H_i(f)| exactly (computed per section by
// transferMagnitude, not estimated). Rendered sines are checked against it
// in the tests.
//
// Graphic EQ: one peaking section per IEC 61260-1 band (octave or third
// octave, analysis/spectrum.h geometry), centred on the exact band centre,
// with the band-edge Q  Q = sqrt(r)/(r - 1), r = G^(1/b) = upper/lower edge
// ratio (the -3 dB points of the section's bandwidth definition sit at the
// band edges). Adjacent sections overlap, so the response AT a band centre
// is its own gain plus the neighbours' skirts — the graphic EQ's classic
// interaction. It is NOT corrected here; graphicEqCentreErrorDb reports it
// exactly, and the tests state it (octave bands all at +6 dB: +1.6 to
// +2.8 dB extra at interior centres). Iterative gain correction is deferred.
// A band whose centre is at or above 0.45 fs is refused (the bilinear
// transform squeezes it against Nyquist).

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../analysis/spectrum.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "biquad.h"
#include "filters.h"

namespace mmathen {

// D23 adds the filter shapes (one RBJ section each, dsp/filters.h); they
// ignore gainDb. Steeper slopes are cascades: see designButterworth.
enum class EqShape {
  Peaking,
  LowShelf,
  HighShelf,
  LowPass,
  HighPass,
  BandPass,
  Notch,
  AllPass
};

struct EqBand {
  EqShape shape = EqShape::Peaking;
  FrequencyHz frequency{1000.0};
  double gainDb = 0.0;
  double q = 0.7071067811865476;
};

// The one section a band designs to (validation as each designer states;
// filter shapes still require a finite gainDb within +/-60 dB).
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> designEqSection(
    const EqBand& b, SampleRateHz rate) {
  using R = Result<BiquadCoeffs, QuantityError>;
  if (!std::isfinite(b.gainDb) || std::fabs(b.gainDb) > 60.0)
    return R::failure(QuantityError::InvalidArgument);
  switch (b.shape) {
    case EqShape::Peaking:
      return makePeaking(b.frequency, b.gainDb, b.q, rate);
    case EqShape::LowShelf:
      return makeLowShelf(b.frequency, b.gainDb, b.q, rate);
    case EqShape::HighShelf:
      return makeHighShelf(b.frequency, b.gainDb, b.q, rate);
    case EqShape::LowPass:
      return makeLowpass(b.frequency, b.q, rate);
    case EqShape::HighPass:
      return makeHighpass(b.frequency, b.q, rate);
    case EqShape::BandPass:
      return makeBandpass(b.frequency, b.q, rate);
    case EqShape::Notch:
      return makeNotch(b.frequency, b.q, rate);
    case EqShape::AllPass:
      return makeAllpass(b.frequency, b.q, rate);
  }
  return R::failure(QuantityError::InvalidArgument);
}

class ParametricEq {
 public:
  // 1..64 bands, each valid for makePeaking/makeLowShelf/makeHighShelf at
  // rate fs (InvalidArgument otherwise).
  [[nodiscard]] static Result<ParametricEq, QuantityError> create(
      const std::vector<EqBand>& bands, SampleRateHz rate) {
    using R = Result<ParametricEq, QuantityError>;
    if (bands.empty() || bands.size() > 64)
      return R::failure(QuantityError::InvalidArgument);
    std::vector<Biquad> sections;
    std::vector<BiquadCoeffs> coeffs;
    for (const auto& b : bands) {
      auto k = designEqSection(b, rate);
      if (!k.ok()) return R::failure(k.error());
      if (!isStable(k.value())) return R::failure(QuantityError::DomainError);
      sections.emplace_back(k.value());
      coeffs.push_back(k.value());
    }
    return R::success(ParametricEq{std::move(sections), std::move(coeffs),
                                   bands, rate});
  }

  // Series processing into caller buffers (may alias).
  void processBlock(const double* in, double* out, FrameCount n) {
    sections_.front().processBlock(in, out, n);
    for (std::size_t i = 1; i < sections_.size(); ++i)
      sections_[i].processBlock(out, out, n);
  }

  void reset() noexcept {
    for (auto& s : sections_) s.reset();
  }

  // Exact cascade magnitude in dB at f in [0, fs/2].
  [[nodiscard]] Result<double, QuantityError> responseDb(FrequencyHz f) const {
    using R = Result<double, QuantityError>;
    double db = 0.0;
    for (const auto& k : coeffs_) {
      auto m = transferMagnitude(k, f, rate_);
      if (!m.ok()) return R::failure(m.error());
      if (!(m.value() > 0.0)) return R::failure(QuantityError::RangeError);
      db += 20.0 * std::log10(m.value());
    }
    return R::success(db);
  }

  [[nodiscard]] const std::vector<EqBand>& bands() const noexcept {
    return bands_;
  }

  // Replace band i's parameters, allocation-free (safe on an audio thread):
  // the new section is designed and stability-checked first; on any error
  // the EQ is left unchanged. The section keeps its filter state
  // (Biquad::setCoeffs). index >= size: InvalidArgument; design errors as
  // in create().
  [[nodiscard]] Result<EqBand, QuantityError> setBand(std::size_t index,
                                                      const EqBand& b) {
    using R = Result<EqBand, QuantityError>;
    if (index >= sections_.size())
      return R::failure(QuantityError::InvalidArgument);
    auto k = designEqSection(b, rate_);
    if (!k.ok()) return R::failure(k.error());
    if (!isStable(k.value())) return R::failure(QuantityError::DomainError);
    sections_[index].setCoeffs(k.value());
    coeffs_[index] = k.value();
    bands_[index] = b;
    return R::success(b);
  }

  [[nodiscard]] SampleRateHz rate() const noexcept { return rate_; }

 private:
  ParametricEq(std::vector<Biquad> sections, std::vector<BiquadCoeffs> coeffs,
               std::vector<EqBand> bands, SampleRateHz rate)
      : sections_(std::move(sections)),
        coeffs_(std::move(coeffs)),
        bands_(std::move(bands)),
        rate_(rate) {}
  std::vector<Biquad> sections_;
  std::vector<BiquadCoeffs> coeffs_;
  std::vector<EqBand> bands_;
  SampleRateHz rate_;
};

// Q that puts a peaking section's bandwidth edges on the band edges of an
// IEC band with edge ratio r = upper/lower:  Q = sqrt(r)/(r - 1).
[[nodiscard]] inline double bandEdgeQ(const FrequencyBand& band) {
  const double r = band.upper.value() / band.lower.value();
  return std::sqrt(r) / (r - 1.0);
}

// Graphic EQ: one peaking band per IEC band, gains in dB in band order.
// gains.size() must equal bands.size(); every centre must be < 0.45 fs.
[[nodiscard]] inline Result<ParametricEq, QuantityError> makeGraphicEq(
    const std::vector<FrequencyBand>& bands, const std::vector<double>& gainsDb,
    SampleRateHz rate) {
  using R = Result<ParametricEq, QuantityError>;
  if (bands.empty() || bands.size() != gainsDb.size())
    return R::failure(QuantityError::InvalidArgument);
  std::vector<EqBand> eq;
  for (std::size_t i = 0; i < bands.size(); ++i) {
    if (!(bands[i].center.value() < 0.45 * rate.value()))
      return R::failure(QuantityError::DomainError);
    eq.push_back(EqBand{EqShape::Peaking, bands[i].center, gainsDb[i],
                        bandEdgeQ(bands[i])});
  }
  return ParametricEq::create(eq, rate);
}

// Response at each band centre minus that band's set gain: the graphic
// EQ's interaction error, exactly.
[[nodiscard]] inline Result<std::vector<double>, QuantityError>
graphicEqCentreErrorDb(const ParametricEq& eq,
                       const std::vector<FrequencyBand>& bands,
                       const std::vector<double>& gainsDb) {
  using R = Result<std::vector<double>, QuantityError>;
  if (bands.size() != gainsDb.size())
    return R::failure(QuantityError::InvalidArgument);
  std::vector<double> out;
  for (std::size_t i = 0; i < bands.size(); ++i) {
    auto r = eq.responseDb(bands[i].center);
    if (!r.ok()) return R::failure(r.error());
    out.push_back(r.value() - gainsDb[i]);
  }
  return R::success(std::move(out));
}

}  // namespace mmathen
