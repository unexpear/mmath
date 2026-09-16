#pragma once

// M3 — additive synthesis from harmonic recipes.
//
// A Harmonic is data (ratio + level), a HarmonicSeries is a validated table,
// and additive() builds one sine voice per partial through the existing
// oscillator + mix operator. No FM-stack or closed timbre systems in the
// kernel: recipes live in tests/examples, the builder is generic. The same
// series can be baked into a single-cycle table (wavetable.h, D6). Levels may exceed 1.0 (headroom is an export-boundary concern);
// negative or non-finite levels are rejected at creation.

#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "oscillator.h"
#include "signal.h"

namespace mmathen {

struct Harmonic {
  FrequencyRatio ratio;
  AmplitudeLinear level;

  bool operator==(const Harmonic&) const = default;
};

class HarmonicSeries {
 public:
  [[nodiscard]] static Result<HarmonicSeries, QuantityError> create(
      std::vector<Harmonic> partials) {
    using R = Result<HarmonicSeries, QuantityError>;
    if (partials.empty())
      return R::failure(QuantityError::InvalidArgument);
    for (const auto& h : partials) {
      if (!std::isfinite(h.ratio.value()) || !(h.ratio.value() > 0.0))
        return R::failure(QuantityError::InvalidArgument);
      if (!std::isfinite(h.level.value()) || h.level.value() < 0.0)
        return R::failure(QuantityError::InvalidArgument);
    }
    // Count before the move: argument evaluation order is unspecified.
    const std::size_t count = partials.size();
    return R::success(HarmonicSeries{
        std::move(partials),
        "series[" + std::to_string(count) + " partials]"});
  }

  [[nodiscard]] const std::vector<Harmonic>& partials() const noexcept {
    return partials_;
  }
  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }

 private:
  HarmonicSeries(std::vector<Harmonic> partials, std::string formula)
      : partials_(std::move(partials)), formula_(std::move(formula)) {}
  std::vector<Harmonic> partials_;
  std::string formula_;
};

// One sine per partial at f0*ratio, amplitude scaled by level, all mixed.
// f0 must be finite and > 0; partial products must stay finite (RangeError).
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError> additive(
    FrequencyHz f0, Function1D<AmplitudeLinear> amp, HarmonicSeries series) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  const double base = f0.value();
  if (!std::isfinite(base) || !(base > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const auto& partials = series.partials();
  const double lvl0 = partials.front().level.value();
  const double freq0 = base * partials.front().ratio.value();
  if (!std::isfinite(freq0) || !(freq0 > 0.0))
    return R::failure(QuantityError::RangeError);
  Function1D<AmplitudeLinear> amp0 = wrap<AmplitudeLinear>(
      [amp, lvl0](TimePointSeconds t) {
        // Deliberate numeric boundary (documented IEEE behavior on
        // overflow; caught downstream by tryAt()/render stats).
        return AmplitudeLinear{amp.at(t).value() * lvl0};
      },
      "(" + amp.formula() + " * level)");
  Signal<SignedAudioValue> mix = sine(FrequencyHz{freq0}, std::move(amp0));
  for (std::size_t i = 1; i < partials.size(); ++i) {
    const double freq = base * partials[i].ratio.value();
    if (!std::isfinite(freq) || !(freq > 0.0))
      return R::failure(QuantityError::RangeError);
    const double lvl = partials[i].level.value();
    Function1D<AmplitudeLinear> scaled = wrap<AmplitudeLinear>(
        [amp, lvl](TimePointSeconds t) {
          return AmplitudeLinear{amp.at(t).value() * lvl};
        },
        "(" + amp.formula() + " * level)");
    mix = std::move(mix) + sine(FrequencyHz{freq}, std::move(scaled));
  }
  return R::success(std::move(mix));
}

}  // namespace mmathen
