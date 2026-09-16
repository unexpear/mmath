#pragma once

// M4 — tempo curves: musical time <-> wall-clock time, both directions.
//
// A tempo curve maps absolute time to cumulative beats (from an arbitrary
// beat-zero reference) and back. Constant tempo is exact; the linear ramp
// integrates to a quadratic with a closed-form inverse. Curves extend
// constantly outside their ramp span (documented, not clamped silently —
// the extension IS the definition). All tempos are strictly positive, so
// beats(t) is strictly increasing and the inverse always exists.
// Piecewise/multi-segment maps are deferred; these two cover the M4 vocabulary.

#include <cmath>
#include <string>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

template <typename T>
concept TempoCurve = requires(const T& c, TimePointSeconds t, Beat b) {
  { c.beatsAt(t) } -> std::same_as<Result<Beat, QuantityError>>;
  { c.secondsAt(b) } -> std::same_as<Result<TimePointSeconds, QuantityError>>;
};

class ConstantTempo {
 public:
  [[nodiscard]] static Result<ConstantTempo, QuantityError> create(
      TempoBPM bpm, TimePointSeconds beatZero = TimePointSeconds{0.0}) {
    using R = Result<ConstantTempo, QuantityError>;
    if (!std::isfinite(bpm.value()) || !(bpm.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(beatZero.value()))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(ConstantTempo{bpm, beatZero});
  }

  // beats(t) = (t - t0) * bpm / 60.
  [[nodiscard]] Result<Beat, QuantityError> beatsAt(TimePointSeconds t) const {
    using R = Result<Beat, QuantityError>;
    const double b = (t.value() - t0_.value()) * bpm_.value() / 60.0;
    if (!std::isfinite(b)) return R::failure(QuantityError::RangeError);
    return R::success(Beat{b});
  }

  // t(b) = t0 + b * 60 / bpm. Exact inverse (same arithmetic, reversed).
  [[nodiscard]] Result<TimePointSeconds, QuantityError> secondsAt(
      Beat b) const {
    using R = Result<TimePointSeconds, QuantityError>;
    if (!std::isfinite(b.value()))
      return R::failure(QuantityError::InvalidArgument);
    const double t = t0_.value() + b.value() * 60.0 / bpm_.value();
    if (!std::isfinite(t)) return R::failure(QuantityError::RangeError);
    return R::success(TimePointSeconds{t});
  }

  [[nodiscard]] TempoBPM bpm() const noexcept { return bpm_; }

 private:
  ConstantTempo(TempoBPM bpm, TimePointSeconds beatZero)
      : bpm_(bpm), t0_(beatZero) {}
  TempoBPM bpm_;
  TimePointSeconds t0_;
};

class LinearRampTempo {
 public:
  // bpm(t) ramps from `from` to `to` over [t0, t0+span], constant outside.
  [[nodiscard]] static Result<LinearRampTempo, QuantityError> create(
      TempoBPM from, TempoBPM to, TimePointSeconds t0, DurationSeconds span) {
    using R = Result<LinearRampTempo, QuantityError>;
    if (!std::isfinite(from.value()) || !(from.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(to.value()) || !(to.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(t0.value()))
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(span.value()) || !(span.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(LinearRampTempo{from, to, t0, span});
  }

  [[nodiscard]] Result<Beat, QuantityError> beatsAt(TimePointSeconds t) const {
    using R = Result<Beat, QuantityError>;
    const double f = from_.value();
    const double dt = t.value() - t0_.value();
    if (!std::isfinite(dt)) return R::failure(QuantityError::InvalidArgument);
    const double s = span_.value();
    double b = 0.0;
    if (dt <= 0.0) {
      b = f * dt / 60.0;
    } else if (dt >= s) {
      b = beatsAtSpan() + to_.value() * (dt - s) / 60.0;
    } else {
      b = f * dt / 60.0 +
          (to_.value() - f) * dt * dt / (2.0 * s * 60.0);
    }
    if (!std::isfinite(b)) return R::failure(QuantityError::RangeError);
    return R::success(Beat{b});
  }

  [[nodiscard]] Result<TimePointSeconds, QuantityError> secondsAt(
      Beat b) const {
    using R = Result<TimePointSeconds, QuantityError>;
    const double target = b.value();
    if (!std::isfinite(target))
      return R::failure(QuantityError::InvalidArgument);
    const double f = from_.value();
    const double s = span_.value();
    const double spanBeats = beatsAtSpan();
    double dt = 0.0;
    if (target <= 0.0) {
      dt = target * 60.0 / f;
    } else if (target >= spanBeats) {
      dt = s + (target - spanBeats) * 60.0 / to_.value();
    } else {
      // a*dt^2 + c*dt - target = 0; strictly increasing, one root in [0, s].
      const double a = (to_.value() - f) / (2.0 * s * 60.0);
      const double c = f / 60.0;
      if (a == 0.0) {
        dt = target / c;
      } else {
        const double disc = c * c + 4.0 * a * target;
        if (!(disc >= 0.0) || !std::isfinite(disc))
          return R::failure(QuantityError::DomainError);
        const double root = std::sqrt(disc);
        const double r1 = (-c + root) / (2.0 * a);
        const double r2 = (-c - root) / (2.0 * a);
        if (r1 >= 0.0 && r1 <= s) {
          dt = r1;
        } else if (r2 >= 0.0 && r2 <= s) {
          dt = r2;
        } else {
          return R::failure(QuantityError::DomainError);
        }
      }
    }
    const double t = t0_.value() + dt;
    if (!std::isfinite(t)) return R::failure(QuantityError::RangeError);
    return R::success(TimePointSeconds{t});
  }

 private:
  LinearRampTempo(TempoBPM from, TempoBPM to, TimePointSeconds t0,
                  DurationSeconds span)
      : from_(from), to_(to), t0_(t0), span_(span) {}

  double beatsAtSpan() const {
    const double f = from_.value();
    const double s = span_.value();
    return f * s / 60.0 + (to_.value() - f) * s / (2.0 * 60.0);
  }

  TempoBPM from_;
  TempoBPM to_;
  TimePointSeconds t0_;
  DurationSeconds span_;
};

}  // namespace mmathen
