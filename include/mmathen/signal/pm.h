#pragma once

// D7 — phase modulation (PM), beside the frequency modulation of fm.h.
//
// Sinusoidal PM puts the modulator directly into the phase:
//   x(t) = A*sin(2*pi*fc*t + phi0 + beta*sin(2*pi*fm*t + theta))
// Its instantaneous frequency is the phase derivative / 2*pi:
//   f(t) = fc + beta*fm*cos(2*pi*fm*t + theta)
// so the peak deviation beta*fm grows with the modulator rate, whereas FM
// fixes the deviation d and lets the index d/fm fall — the textbook
// distinction, pinned in the tests. Three independent identities verify it:
//   1. Jacobi-Anger: x = sum_k J_k(beta)*sin(2*pi*(fc+k*fm)*t + phi0 + k*theta)
//      with J_{-k} = (-1)^k J_k (time-domain Bessel expansion).
//   2. integratePhase (D5 adaptive Simpson) of f(t) reproduces the phase.
//   3. Sinusoidal FM of index I (phase.h: I*(1 - cos)) IS PM with
//      beta = I, theta = -pi/2 and phi0 shifted by +I.
// phaseModulate() generalizes to ANY Signal as modulator (index * m(t) added
// to the carrier phase), which is how operator stacks are built: the output
// of one phaseModulate can modulate the next. Stateless throughout; no
// feedback operators (feedback needs sample-delayed state: deferred).

#include <cmath>
#include <string>
#include <utility>

#include "../core/result.h"
#include "../math/constants.h"
#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "oscillator.h"
#include "phase.h"
#include "signal.h"

namespace mmathen {

struct SinusoidalPM {
  FrequencyHz carrier;          // fc
  double index;                 // beta, peak phase deviation in radians
  FrequencyHz rate;             // fm
  PhaseRadians modulatorPhase;  // theta
  [[nodiscard]] FrequencyHz frequencyAt(TimePointSeconds t) const {
    return FrequencyHz{
        carrier.value() +
        index * rate.value() *
            std::cos(constants::twoPi * rate.value() * t.value() +
                     modulatorPhase.value())};
  }
  [[nodiscard]] PhaseRadians phaseAt(TimePointSeconds t,
                                     PhaseRadians p0) const {
    const double tt = t.value();
    return PhaseRadians{
        p0.value() + constants::twoPi * carrier.value() * tt +
        index * std::sin(constants::twoPi * rate.value() * tt +
                         modulatorPhase.value())};
  }
};

// Sinusoidal PM tone. fc, fm finite and > 0 (non-finite InvalidArgument,
// non-positive DomainError, as fmTone); index finite >= 0; phases finite.
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError> pmTone(
    FrequencyHz carrier, FrequencyHz modulatorRate, double index,
    PhaseRadians modulatorPhase = PhaseRadians{0.0},
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
  if (!std::isfinite(modulatorPhase.value()) || !std::isfinite(phase0.value()))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(detail::sineFromShape(
      SinusoidalPM{carrier, index, modulatorRate, modulatorPhase},
      std::move(amp), phase0, "sine(pm)"));
}

// Peak instantaneous frequency deviation of sinusoidal PM: beta*fm.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> pmPeakDeviation(
    FrequencyHz modulatorRate, double index) {
  using R = Result<FrequencyHz, QuantityError>;
  const double fm = modulatorRate.value();
  if (!std::isfinite(fm) || !(fm > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(index) || index < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double d = index * fm;
  if (!std::isfinite(d)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{d});
}

// General PM: A(t)*sin(2*pi*fc*t + phi0 + index*m(t)) for any modulator
// Signal m. The result is itself a Signal, so operators stack. Validation
// as pmTone; index may be any finite value >= 0.
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError>
phaseModulate(FrequencyHz carrier, Signal<SignedAudioValue> modulator,
              double index,
              Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
              PhaseRadians phase0 = PhaseRadians{0.0}) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  const double fc = carrier.value();
  if (!std::isfinite(fc)) return R::failure(QuantityError::InvalidArgument);
  if (!(fc > 0.0)) return R::failure(QuantityError::DomainError);
  if (!std::isfinite(index) || index < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(phase0.value()))
    return R::failure(QuantityError::InvalidArgument);
  std::string formula = "pm(" + modulator.formula() + ")";
  return R::success(Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [fc, modulator = std::move(modulator), index, amp = std::move(amp),
       phase0](TimePointSeconds t) {
        const double p = phase0.value() +
                         constants::twoPi * fc * t.value() +
                         index * modulator.at(t).value();
        return SignedAudioValue{amp.at(t).value() * std::sin(p)};
      },
      std::move(formula))});
}

}  // namespace mmathen
