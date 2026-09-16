#pragma once

// Phase 0 — cross-quantity conversions.
//
// Every conversion is fallible and returns Result<T, QuantityError>:
// validated inputs do not guarantee a valid result, so each operation checks
// for overflow/underflow (RangeError) as well as bad inputs. Unlisted
// conversions stay unavailable — in particular there is NO
// Frequency x Duration -> Phase overload (that would silently drop the 2*pi
// conversion); phase comes from AngularFrequency x Duration or toAngular().
//
// Canonical representations in, canonical representations out; alternative
// units enter only through named conversions (durationFromMilliseconds,
// frequencyFromKilohertz). A general runtime unit registry is deferred.

#include <cmath>

#include "../core/result.h"
#include "../math/constants.h"
#include "music_units.h"
#include "physical_units.h"

namespace mmathen {

// T = 1/f. DomainError for f <= 0 or non-finite; RangeError if 1/f overflows.
template <typename F>
  requires FrequencyQuantity<F>
[[nodiscard]] Result<DurationSeconds, QuantityError> period(F f) {
  using R = Result<DurationSeconds, QuantityError>;
  const double v = f.value();
  if (!std::isfinite(v) || !(v > 0.0))
    return R::failure(QuantityError::DomainError);
  const double t = 1.0 / v;
  if (!std::isfinite(t)) return R::failure(QuantityError::RangeError);
  return R::success(DurationSeconds{t});
}

// Explicit cycles -> radians: w = 2*pi*f. InvalidArgument for non-finite
// input; RangeError on overflow.
[[nodiscard]] inline Result<AngularFrequencyRadPerSec, QuantityError> toAngular(
    FrequencyHz f) {
  using R = Result<AngularFrequencyRadPerSec, QuantityError>;
  const double v = f.value();
  if (!std::isfinite(v))
    return R::failure(QuantityError::InvalidArgument);
  const double w = constants::twoPi * v;
  if (!std::isfinite(w)) return R::failure(QuantityError::RangeError);
  return R::success(AngularFrequencyRadPerSec{w});
}

// Operator table: Frequency / Frequency -> FrequencyRatio. Non-finite inputs
// are InvalidArgument; a non-positive denominator or a non-positive or
// non-finite quotient is DomainError (a non-positive ratio has no
// real-valued cents).
template <typename A, typename B>
  requires FrequencyQuantity<A> && FrequencyQuantity<B>
[[nodiscard]] Result<FrequencyRatio, QuantityError> operator/(A a, B b) {
  using R = Result<FrequencyRatio, QuantityError>;
  const double x = a.value();
  const double y = b.value();
  if (!std::isfinite(x) || !std::isfinite(y))
    return R::failure(QuantityError::InvalidArgument);
  if (!(y > 0.0)) return R::failure(QuantityError::DomainError);
  const double r = x / y;
  if (!(r > 0.0) || !std::isfinite(r))
    return R::failure(QuantityError::DomainError);
  return R::success(FrequencyRatio{r});
}

// Operator table: AngularFrequency x Duration -> Phase. InvalidArgument for
// non-finite inputs; RangeError on overflow.
[[nodiscard]] inline Result<PhaseRadians, QuantityError> operator*(
    AngularFrequencyRadPerSec w, DurationSeconds d) {
  using R = Result<PhaseRadians, QuantityError>;
  const double x = w.value();
  const double y = d.value();
  if (!std::isfinite(x) || !std::isfinite(y))
    return R::failure(QuantityError::InvalidArgument);
  const double p = x * y;
  if (!std::isfinite(p)) return R::failure(QuantityError::RangeError);
  return R::success(PhaseRadians{p});
}

[[nodiscard]] inline Result<PhaseRadians, QuantityError> operator*(
    DurationSeconds d, AngularFrequencyRadPerSec w) {
  return w * d;
}

// Pure checked conversion: t_n = t_0 + n/fs. No clocks, no renderer loop.
// InvalidArgument for non-finite t_0 or invalid fs; RangeError on overflow.
// NOTE: FrameIndex -> double is approximate for very large indices (binary64
// cannot distinguish all uint64 values), so the counter maximum is NOT the
// maximum accurately supported render length.
[[nodiscard]] inline Result<TimePointSeconds, QuantityError> timeAt(
    TimePointSeconds t0, FrameIndex n, SampleRateHz fs) {
  using R = Result<TimePointSeconds, QuantityError>;
  const double t = t0.value();
  const double rate = fs.value();
  if (!std::isfinite(t))
    return R::failure(QuantityError::InvalidArgument);
  if (!(rate > 0.0) || !std::isfinite(rate))
    return R::failure(QuantityError::InvalidArgument);
  const double out = t + static_cast<double>(n.value()) / rate;
  if (!std::isfinite(out)) return R::failure(QuantityError::RangeError);
  return R::success(TimePointSeconds{out});
}

// Duration -> frame count. Rounding policy: round to nearest, half away from
// zero (std::llround). Negative durations are DomainError (a separate render
// request may additionally reject them); RangeError past LLONG_MAX frames.
[[nodiscard]] inline Result<FrameCount, QuantityError> frameCountForDuration(
    DurationSeconds d, SampleRateHz fs) {
  using R = Result<FrameCount, QuantityError>;
  const double dur = d.value();
  const double rate = fs.value();
  if (!std::isfinite(dur))
    return R::failure(QuantityError::InvalidArgument);
  if (dur < 0.0) return R::failure(QuantityError::DomainError);
  if (!(rate > 0.0) || !std::isfinite(rate))
    return R::failure(QuantityError::InvalidArgument);
  const double frames = dur * rate;
  if (!std::isfinite(frames) || frames >= 9.223372036854776e18)
    return R::failure(QuantityError::RangeError);
  return R::success(
      FrameCount{static_cast<std::uint64_t>(std::llround(frames))});
}

// cents(r) = 1200*log2(r). DomainError for r <= 0 or non-finite;
// RangeError if the product overflows.
[[nodiscard]] inline Result<Cents, QuantityError> cents(FrequencyRatio r) {
  using R = Result<Cents, QuantityError>;
  const double v = r.value();
  if (!std::isfinite(v) || !(v > 0.0))
    return R::failure(QuantityError::DomainError);
  const double c = 1200.0 * std::log2(v);
  if (!std::isfinite(c)) return R::failure(QuantityError::RangeError);
  return R::success(Cents{c});
}

// ratio(c) = 2^(c/1200). InvalidArgument for non-finite cents; RangeError if
// the result is non-positive (underflow to zero) or non-finite (overflow).
[[nodiscard]] inline Result<FrequencyRatio, QuantityError> ratio(Cents c) {
  using R = Result<FrequencyRatio, QuantityError>;
  const double v = c.value();
  if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const double r = std::pow(2.0, v / 1200.0);
  if (!(r > 0.0) || !std::isfinite(r))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyRatio{r});
}

// duration = 60*B/BPM. InvalidArgument for non-finite beats or invalid tempo;
// RangeError on overflow. Duration stays signed: whether a render request
// accepts a negative duration is a separate validation rule.
[[nodiscard]] inline Result<DurationSeconds, QuantityError> beatDuration(
    Beat beats, TempoBPM bpm) {
  using R = Result<DurationSeconds, QuantityError>;
  const double b = beats.value();
  const double t = bpm.value();
  if (!std::isfinite(b)) return R::failure(QuantityError::InvalidArgument);
  if (!(t > 0.0) || !std::isfinite(t))
    return R::failure(QuantityError::InvalidArgument);
  const double d = 60.0 * b / t;
  if (!std::isfinite(d)) return R::failure(QuantityError::RangeError);
  return R::success(DurationSeconds{d});
}

// Named alternative-unit conversions into canonical storage.
[[nodiscard]] inline Result<DurationSeconds, QuantityError>
durationFromMilliseconds(double ms) {
  using R = Result<DurationSeconds, QuantityError>;
  if (!std::isfinite(ms)) return R::failure(QuantityError::InvalidArgument);
  const double s = ms / 1000.0;
  if (!std::isfinite(s)) return R::failure(QuantityError::RangeError);
  return R::success(DurationSeconds{s});
}

[[nodiscard]] inline Result<FrequencyHz, QuantityError> frequencyFromKilohertz(
    double khz) {
  using R = Result<FrequencyHz, QuantityError>;
  if (!std::isfinite(khz)) return R::failure(QuantityError::InvalidArgument);
  const double hz = khz * 1000.0;
  if (!std::isfinite(hz)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{hz});
}

}  // namespace mmathen
