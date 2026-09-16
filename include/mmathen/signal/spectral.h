#pragma once

// D5 — time-varying additive spectra: per-partial envelopes.
//
// M3's additive() voices every partial through one shared amplitude
// function. Real onsets differ per partial (attack transients are
// brighter: high partials peak early and decay fast). This builder takes
// one envelope Function1D per partial and mixes the shaped voices through
// the existing operator. Envelopes stay pure time functions (ADSR or
// custom); no state is introduced.

#include <cstddef>
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

struct SpectralVoice {
  FrequencyRatio ratio;
  Function1D<AmplitudeLinear> envelope;
};

[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError>
timeVaryingAdditive(FrequencyHz f0, std::vector<SpectralVoice> voices) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  const double base = f0.value();
  if (!std::isfinite(base) || !(base > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (voices.empty())
    return R::failure(QuantityError::InvalidArgument);
  for (const auto& v : voices) {
    if (!std::isfinite(v.ratio.value()) || !(v.ratio.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
  }
  const double firstRatio = voices.front().ratio.value();
  const double firstFreq = base * firstRatio;
  if (!std::isfinite(firstFreq) || !(firstFreq > 0.0))
    return R::failure(QuantityError::RangeError);
  Signal<SignedAudioValue> mix =
      sine(FrequencyHz{firstFreq}, std::move(voices.front().envelope));
  for (std::size_t i = 1; i < voices.size(); ++i) {
    const double freq = base * voices[i].ratio.value();
    if (!std::isfinite(freq) || !(freq > 0.0))
      return R::failure(QuantityError::RangeError);
    mix = std::move(mix) +
          sine(FrequencyHz{freq}, std::move(voices[i].envelope));
  }
  return R::success(std::move(mix));
}

}  // namespace mmathen
