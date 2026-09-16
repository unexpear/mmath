#pragma once

// M3 — ADSR envelope as a pure amplitude function.
//
// adsr() returns Function1D<AmplitudeLinear> over absolute time: silence
// before on, linear attack 0 -> 1, linear decay 1 -> sustain, sustain hold,
// linear release to 0 ending exactly at off + release, silence after. Note-on
// and note-off are evaluation parameters, not stored state — the envelope is
// stateless like every other time function. If off precedes the end of
// attack/decay, release starts from the value at off (documented short-note
// behavior). No other envelope vocabulary lives here: exponential and curved
// multi-segment shapes are segment_envelope.h (D7); anything else arrives as
// a plain lambda via wrap().

#include <string>
#include <utility>

#include "../core/result.h"
#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

struct ADSRSegments {
  DurationSeconds attack{0.0};
  DurationSeconds decay{0.0};
  AmplitudeLinear sustainLevel{0.0};
  DurationSeconds release{0.0};
};

[[nodiscard]] inline Result<ADSRSegments, QuantityError> makeADSR(
    DurationSeconds attack, DurationSeconds decay, AmplitudeLinear sustain,
    DurationSeconds release) {
  using R = Result<ADSRSegments, QuantityError>;
  if (!std::isfinite(attack.value()) || attack.value() < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(decay.value()) || decay.value() < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(sustain.value()) || sustain.value() < 0.0 ||
      sustain.value() > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(release.value()) || release.value() < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(ADSRSegments{attack, decay, sustain, release});
}

[[nodiscard]] inline Result<Function1D<AmplitudeLinear>, QuantityError> adsr(
    ADSRSegments segments, TimePointSeconds on, TimePointSeconds off) {
  using R = Result<Function1D<AmplitudeLinear>, QuantityError>;
  if (!std::isfinite(on.value()) || !std::isfinite(off.value()))
    return R::failure(QuantityError::InvalidArgument);
  if (off.value() < on.value())
    return R::failure(QuantityError::InvalidArgument);
  const double tOn = on.value();
  const double tOff = off.value();
  const double a = segments.attack.value();
  const double d = segments.decay.value();
  const double s = segments.sustainLevel.value();
  const double r = segments.release.value();

  // Attack/decay value at an absolute time (used for the release start when
  // off precedes the end of attack/decay).
  auto strikeValue = [=](double tt) {
    if (a > 0.0 && tt <= tOn + a) return (tt - tOn) / a;
    const double aEnd = tOn + a;  // zero attack: already at full level
    if (d > 0.0 && tt <= aEnd + d) return 1.0 - (1.0 - s) * (tt - aEnd) / d;
    return s;
  };

  return R::success(wrap<AmplitudeLinear>(
      [=](TimePointSeconds t) {
        const double tt = t.value();
        if (tt < tOn) return AmplitudeLinear{0.0};
        if (tt < tOff) return AmplitudeLinear{strikeValue(tt)};
        if (r == 0.0 || tt >= tOff + r) return AmplitudeLinear{0.0};
        const double v0 = strikeValue(tOff);
        return AmplitudeLinear{v0 * (1.0 - (tt - tOff) / r)};
      },
      "adsr(a,d,s,r)"));
}

}  // namespace mmathen
