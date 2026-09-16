#pragma once

// D1 — stiff-string spectra: f_n = n*f_0*sqrt(1 + B*n^2) (Fletcher 1964,
// JASA 36; Smith PASP section 10.31; Gracia/Sanz-Perela 2016 for the
// boundary-condition derivation). Real strings are never ideal: bending
// stiffness stretches every partial sharp, audibly so on pianos
// (typical B: 5e-5..7e-4 lower half per KTH measurements, ~1e-3 per
// Gracia). B is dimensionless data, validated at creation; the just-noticeable
// threshold helper follows Smith (PASP Eq. 10.31) and is an approximation,
// not a perceptual claim of this engine.

#include <cmath>
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

struct StiffSpectrum {
  FrequencyHz f0;
  double inharmonicity;  // B >= 0
  int partialCount;      // >= 1

  bool operator==(const StiffSpectrum&) const = default;
};

[[nodiscard]] inline Result<StiffSpectrum, QuantityError> makeStiffSpectrum(
    FrequencyHz f0, double b, int partialCount) {
  using R = Result<StiffSpectrum, QuantityError>;
  if (!std::isfinite(f0.value()) || !(f0.value() > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(b) || b < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (partialCount < 1)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(StiffSpectrum{f0, b, partialCount});
}

// n-th partial, 1-based, within the spectrum's declared count.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> partialFrequency(
    const StiffSpectrum& spectrum, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1 || n > spectrum.partialCount)
    return R::failure(QuantityError::InvalidArgument);
  const double nn = static_cast<double>(n);
  const double f =
      nn * spectrum.f0.value() * std::sqrt(1.0 + spectrum.inharmonicity * nn * nn);
  if (!std::isfinite(f) || !(f > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// Audibility threshold approximation (Smith PASP Eq. 10.31):
// B_thresh(f0) = exp(2.54*ln(f0) - 24.6). Below this, stiffness is
// perceptually negligible; above, it colors tuning (Railsback mechanism).
[[nodiscard]] inline Result<double, QuantityError> audibilityThresholdB(
    FrequencyHz f0) {
  using R = Result<double, QuantityError>;
  const double f = f0.value();
  if (!std::isfinite(f) || !(f > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double b = std::exp(2.54 * std::log(f) - 24.6);
  if (!std::isfinite(b)) return R::failure(QuantityError::RangeError);
  return R::success(b);
}

// One sine per stiff partial, mixed through the existing operator.
// Amplitude is shared (per-partial spectral envelopes stay deferred).
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError>
stiffAdditive(StiffSpectrum spectrum, Function1D<AmplitudeLinear> amp) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  auto first = partialFrequency(spectrum, 1);
  if (!first.ok()) return R::failure(first.error());
  Signal<SignedAudioValue> mix =
      sine(std::move(first).value(), amp);
  for (int n = 2; n <= spectrum.partialCount; ++n) {
    auto f = partialFrequency(spectrum, n);
    if (!f.ok()) return R::failure(f.error());
    mix = std::move(mix) + sine(std::move(f).value(), amp);
  }
  return R::success(std::move(mix));
}

// B from two measured partials (cf. Rasch 1985): with g_n = (f_n/n)^2,
// g_m*(1+B*n^2) = g_n*(1+B*m^2), so B = (g_m-g_n)/(m^2*g_n-n^2*g_m).
// Partial numbers must differ and be >= 1; frequencies finite and > 0.
// A finite estimate is returned as-is (negative flags bad data; the caller
// judges physicality) — undefined cases are DomainError, never defaults.
[[nodiscard]] inline Result<double, QuantityError> estimateB(
    FrequencyHz fa, int na, FrequencyHz fb, int nb) {
  using R = Result<double, QuantityError>;
  if (na < 1 || nb < 1 || na == nb)
    return R::failure(QuantityError::InvalidArgument);
  const double fav = fa.value();
  const double fbv = fb.value();
  if (!std::isfinite(fav) || !std::isfinite(fbv) || !(fav > 0.0) ||
      !(fbv > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double na2 =
      static_cast<double>(na) * static_cast<double>(na);
  const double nb2 =
      static_cast<double>(nb) * static_cast<double>(nb);
  const double ga = (fav / na) * (fav / na);
  const double gb = (fbv / nb) * (fbv / nb);
  const double denom = nb2 * ga - na2 * gb;
  if (!std::isfinite(denom) || denom == 0.0)
    return R::failure(QuantityError::DomainError);
  const double b = (gb - ga) / denom;
  if (!std::isfinite(b)) return R::failure(QuantityError::RangeError);
  return R::success(b);
}

}  // namespace mmathen
