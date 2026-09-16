#pragma once

// D5 — bandlimited classic waveforms via 2-point PolyBLEP (Tale, after
// Kleimola et al. SMC2010; residual verified verbatim against the
// martinfinke port).
//
// ARCHITECTURAL POSITION, READ HONESTLY: bandlimited correction is
// inherently sample-rate-bound (the polynomial spans samples), so these
// are PURE FUNCTIONS OF INTEGER FRAME INDEX, not continuous-time Signals:
// value = f(n, phaseIncrement, ...). They share the statelessness contract
// (no hidden state, repeatable, seekable) but not the at(t) interface;
// sampled-source renderer integration is scoped follow-up work, and the
// demo renders through an explicit per-n loop. PTR (Kleimola-Valimaki 2012)
// is deferred with reason: Faust's PTR formulation is feedback-stateful,
// and a pure-function port would be our own unverified derivation.
// Quantitative alias proof awaits the FFT milestone (stated, not claimed);
// here: agreement with naive away from discontinuities, bounded documented
// deviation inside transition regions, zero-crossing pitch intact.

#include <cmath>
#include <cstdint>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

namespace detail {
// 2-point PolyBLEP residual on unit phase; dt = cycles per sample in
// (0, 0.5) — i.e. f strictly below Nyquist, enforced by callers.
inline double polyblepResidual(double phase01, double dt01) {
  if (phase01 < dt01) {
    const double u = phase01 / dt01 - 1.0;
    return -u * u;
  }
  if (phase01 > 1.0 - dt01) {
    const double u = (phase01 - 1.0) / dt01 + 1.0;
    return u * u;
  }
  return 0.0;
}
}  // namespace detail

// Naive (aliasing) references for comparison tests.
[[nodiscard]] inline Result<double, QuantityError> naiveSawValue(
    double phase01) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(phase01) || phase01 < 0.0 || phase01 >= 1.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(2.0 * phase01 - 1.0);
}

[[nodiscard]] inline Result<double, QuantityError> naiveSquareValue(
    double phase01) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(phase01) || phase01 < 0.0 || phase01 >= 1.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(phase01 < 0.5 ? 1.0 : -1.0);
}

// PolyBLEP saw: naive saw minus the residual at the wrap discontinuity.
[[nodiscard]] inline Result<double, QuantityError> polyblepSawValue(
    double phase01, double cyclesPerSample) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(phase01) || phase01 < 0.0 || phase01 >= 1.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(cyclesPerSample) || !(cyclesPerSample > 0.0) ||
      !(cyclesPerSample < 0.5))
    return R::failure(QuantityError::InvalidArgument);
  const double y = 2.0 * phase01 - 1.0;
  return R::success(y - detail::polyblepResidual(phase01, cyclesPerSample));
}

// PolyBLEP square: residual added at 0, subtracted at the half-cycle.
[[nodiscard]] inline Result<double, QuantityError> polyblepSquareValue(
    double phase01, double cyclesPerSample) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(phase01) || phase01 < 0.0 || phase01 >= 1.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(cyclesPerSample) || !(cyclesPerSample > 0.0) ||
      !(cyclesPerSample < 0.5))
    return R::failure(QuantityError::InvalidArgument);
  double half = phase01 + 0.5;
  if (half >= 1.0) half -= 1.0;
  const double y = phase01 < 0.5 ? 1.0 : -1.0;
  return R::success(y + detail::polyblepResidual(phase01, cyclesPerSample) -
                    detail::polyblepResidual(half, cyclesPerSample));
}

// Frame phase for constant frequency: frac(n * f / fs + phase0).
// f must be strictly below Nyquist (bandwidth contract); very large n lose
// integer precision in double (same caveat as timeAt, documented there).
[[nodiscard]] inline Result<double, QuantityError> framePhase(
    FrameIndex n, FrequencyHz frequency, SampleRateHz rate, double phase0) {
  using R = Result<double, QuantityError>;
  const double f = frequency.value();
  const double fs = rate.value();
  if (!std::isfinite(f) || !std::isfinite(fs) || !std::isfinite(phase0))
    return R::failure(QuantityError::InvalidArgument);
  if (!(f > 0.0) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (f >= fs / 2.0) return R::failure(QuantityError::DomainError);
  double p = static_cast<double>(n.value()) * f / fs + phase0;
  p -= std::floor(p);
  if (p < 0.0) p += 1.0;
  if (!(p >= 0.0) || !(p < 1.0)) return R::failure(QuantityError::RangeError);
  return R::success(p);
}

}  // namespace mmathen
