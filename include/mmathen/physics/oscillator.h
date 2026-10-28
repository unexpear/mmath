#pragma once

// Exact sample of q'' + 2 σ q' + ω² q = f, with f held constant over dt.
//
// ω is the undamped angular frequency and σ is the amplitude decay rate.
// The advance is the state transition of that linear oscillator, not a
// polynomial envelope. Underdamped motion (σ < ω), including σ = 0, is
// the only regime: a critical or overdamped step is DomainError.
// No complex division is used.

#include <cmath>

#include "../core/result.h"

namespace mmathen {

struct OscillatorState {
  double displacement = 0.0;
  double velocity = 0.0;
};

// InvalidArgument: a non-finite state, a non-positive ω or dt, a
// negative σ, or a non-finite force. DomainError: σ >= ω.
// RangeError: the updated state is non-finite.
[[nodiscard]] inline Result<OscillatorState, QuantityError>
stepConstantForceOscillator(OscillatorState state, double omega, double sigma,
                            double force, double dt) {
  using R = Result<OscillatorState, QuantityError>;
  if (!std::isfinite(state.displacement) || !std::isfinite(state.velocity) ||
      !std::isfinite(omega) || !(omega > 0.0) || !std::isfinite(sigma) ||
      sigma < 0.0 || !std::isfinite(force) || !std::isfinite(dt) || !(dt > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!(sigma < omega)) return R::failure(QuantityError::DomainError);
  const double omega2 = omega * omega;
  if (!std::isfinite(omega2) || !(omega2 > 0.0))
    return R::failure(QuantityError::RangeError);
  const double particular = force / omega2;
  if (!std::isfinite(particular)) return R::failure(QuantityError::RangeError);
  const double u0 = state.displacement - particular;
  const double v0 = state.velocity;
  const double wd2 = omega2 - sigma * sigma;
  if (!(wd2 > 0.0)) return R::failure(QuantityError::DomainError);
  const double wd = std::sqrt(wd2);
  const double decay = std::exp(-sigma * dt);
  const double cosine = std::cos(wd * dt);
  const double sine = std::sin(wd * dt);
  const double swing = (v0 + sigma * u0) / wd;
  const double drag = (sigma * v0 + omega2 * u0) / wd;
  const double u = decay * (u0 * cosine + swing * sine);
  const double v = decay * (v0 * cosine - drag * sine);
  const double displacement = u + particular;
  if (!std::isfinite(displacement) || !std::isfinite(v))
    return R::failure(QuantityError::RangeError);
  return R::success(OscillatorState{displacement, v});
}

}  // namespace mmathen
