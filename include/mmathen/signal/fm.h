#pragma once

// D5 — Chowning FM with modulation index and Bessel sideband tables.
//
// x(t) = A*sin(2*pi*fc*t + I*sin(2*pi*fm*t)) with I = d/f_m (Chowning 1973,
// JAES 21; primary PDF verified). Realized phase-exactly through sineFM
// with deviation d = I*fm — no new integrator, no naive product. Sidebands
// at fc ± k*fm with amplitudes J_k(I) (upper) and (-1)^k J_k(I) (lower),
// via std::cyl_bessel_j (C++17). Negative-frequency components are listed
// raw with their signs (Chowning reflection folding explicitly deferred).
// Carson bandwidth 2*fm*(I+1) is the documented rule of thumb, not a bound.

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "oscillator.h"
#include "phase.h"
#include "signal.h"

namespace mmathen {

struct Sideband {
  FrequencyHz frequency;  // may be <= 0 for low k (unfolded, documented)
  double amplitude;       // signed Bessel amplitude (phase-carrying)

  bool operator==(const Sideband&) const = default;
};

// FM tone: carrier fc, modulator fm, index I >= 0, peak deviation d = I*fm.
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError> fmTone(
    FrequencyHz carrier, FrequencyHz modulatorRate, double index,
    Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
    PhaseRadians phase0 = PhaseRadians{0.0}) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  const double fc = carrier.value();
  const double fm = modulatorRate.value();
  if (!std::isfinite(fc) || !std::isfinite(fm))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fc > 0.0) || !(fm > 0.0))
    return R::failure(QuantityError::DomainError);
  if (!std::isfinite(index) || index < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double deviation = index * fm;
  if (!std::isfinite(deviation))
    return R::failure(QuantityError::RangeError);
  return R::success(sineFM(carrier, FrequencyHz{deviation}, modulatorRate,
                           std::move(amp), phase0));
}

// Sideband table: k = 0..ceil(I)+2 per Carson's rule (upper fc+k*fm with
// J_k(I); lower fc-k*fm with (-1)^k J_k(I)). Bessel evaluation failures
// (non-finite) are RangeError.
[[nodiscard]] inline Result<std::vector<Sideband>, QuantityError>
sidebandTable(FrequencyHz carrier, FrequencyHz modulatorRate, double index) {
  using R = Result<std::vector<Sideband>, QuantityError>;
  const double fc = carrier.value();
  const double fm = modulatorRate.value();
  if (!std::isfinite(fc) || !std::isfinite(fm))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fc > 0.0) || !(fm > 0.0))
    return R::failure(QuantityError::DomainError);
  if (!std::isfinite(index) || index < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const int kmax = static_cast<int>(std::ceil(index)) + 2;
  std::vector<Sideband> out;
  out.reserve(static_cast<std::size_t>(2 * kmax + 1));
  for (int k = 0; k <= kmax; ++k) {
    const double jk = std::cyl_bessel_j(k, index);
    if (!std::isfinite(jk)) return R::failure(QuantityError::RangeError);
    const double lower = (k % 2 == 0) ? jk : -jk;
    out.push_back(Sideband{FrequencyHz{fc + k * fm}, jk});
    if (k > 0) out.push_back(Sideband{FrequencyHz{fc - k * fm}, lower});
  }
  return R::success(std::move(out));
}

// Carson's rule-of-thumb bandwidth: 2*fm*(I+1).
[[nodiscard]] inline Result<FrequencyHz, QuantityError> carsonBandwidth(
    FrequencyHz modulatorRate, double index) {
  using R = Result<FrequencyHz, QuantityError>;
  const double fm = modulatorRate.value();
  if (!std::isfinite(fm) || !(fm > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(index) || index < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double bw = 2.0 * fm * (index + 1.0);
  if (!std::isfinite(bw)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{bw});
}

}  // namespace mmathen
