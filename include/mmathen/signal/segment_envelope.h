#pragma once

// D7 — segment envelopes with linear, exponential and curved shapes.
//
// A SegmentEnvelope starts at a level and moves through segments, each a
// duration, a target level and a curve. With u = elapsed fraction in [0,1]
// of a segment from v0 to v1:
//   Linear       v0 + (v1 - v0)*u
//   Exponential  v0 * (v1/v0)^u           — straight line in dB; both levels
//                                           must be > 0 (a true exponential
//                                           never reaches 0: DomainError)
//   Curved(c)    v0 + (v1 - v0)*expm1(c*u)/expm1(c), c != 0
//                c > 0: slow start, fast finish; c < 0: fast start;
//                c == 0 is exactly Linear. expm1 keeps small |c| accurate.
// Segment ends are exact: at and after a segment's end the value IS its
// target (no interpolation roundoff at boundaries); zero-duration segments
// jump. After the last segment the final level holds.
//
// Like adsr() (envelope.h), envelopes are pure functions of absolute time
// with note-on/off as parameters, never stored state:
//   envelope(shape, on)                silence before on, then the shape
//   gatedEnvelope(strike, release, on, off)
//       strike shape until off, then the release segments starting from the
//       strike value AT off (short-note behavior as adsr). Linear
//       {a -> 1, d -> s} + release {r -> 0} reproduces adsr() (tested).
//   t60Decay(on, T60)                  10^(-3*(t-on)/T60): -60 dB at T60,
//       the room-acoustics/string decay convention.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

enum class SegmentCurve { Linear, Exponential, Curved };

struct EnvelopeSegment {
  DurationSeconds duration{0.0};
  AmplitudeLinear target{0.0};
  SegmentCurve curve = SegmentCurve::Linear;
  double shape = 0.0;  // Curved only: c
};

class SegmentEnvelope {
 public:
  // Levels finite and >= 0, durations finite and >= 0, shapes finite:
  // otherwise InvalidArgument. An Exponential segment whose start or target
  // level is 0 is DomainError.
  [[nodiscard]] static Result<SegmentEnvelope, QuantityError> create(
      AmplitudeLinear start, std::vector<EnvelopeSegment> segments) {
    using R = Result<SegmentEnvelope, QuantityError>;
    const double s = start.value();
    if (!std::isfinite(s) || s < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    double level = s;
    for (const auto& seg : segments) {
      const double d = seg.duration.value();
      const double v = seg.target.value();
      if (!std::isfinite(d) || d < 0.0 || !std::isfinite(v) || v < 0.0 ||
          !std::isfinite(seg.shape))
        return R::failure(QuantityError::InvalidArgument);
      if (seg.curve == SegmentCurve::Exponential &&
          (!(level > 0.0) || !(v > 0.0)))
        return R::failure(QuantityError::DomainError);
      level = v;
    }
    // Count before the move: argument evaluation order is unspecified.
    const std::size_t count = segments.size();
    return R::success(SegmentEnvelope{
        start, std::move(segments),
        "segments[" + std::to_string(count) + "]"});
  }

  // Value at elapsed seconds since the envelope's start (elapsed < 0 gives
  // the start level; callers gate silence themselves).
  [[nodiscard]] double valueAfter(double elapsed) const {
    double v0 = start_.value();
    double segStart = 0.0;
    for (const auto& seg : segments_) {
      const double d = seg.duration.value();
      const double v1 = seg.target.value();
      const double segEnd = segStart + d;
      if (elapsed < segEnd) {
        if (elapsed <= segStart) return v0;
        const double u = (elapsed - segStart) / d;
        switch (seg.curve) {
          case SegmentCurve::Linear:
            return v0 + (v1 - v0) * u;
          case SegmentCurve::Exponential:
            return v0 * std::pow(v1 / v0, u);
          case SegmentCurve::Curved:
            if (seg.shape == 0.0) return v0 + (v1 - v0) * u;
            return v0 + (v1 - v0) * std::expm1(seg.shape * u) /
                            std::expm1(seg.shape);
        }
      }
      v0 = v1;
      segStart = segEnd;
    }
    return v0;
  }

  [[nodiscard]] DurationSeconds totalDuration() const {
    double total = 0.0;
    for (const auto& seg : segments_) total += seg.duration.value();
    return DurationSeconds{total};
  }
  [[nodiscard]] const std::vector<EnvelopeSegment>& segments() const noexcept {
    return segments_;
  }
  [[nodiscard]] const std::string& formula() const noexcept {
    return formula_;
  }

 private:
  SegmentEnvelope(AmplitudeLinear start, std::vector<EnvelopeSegment> segments,
                  std::string formula)
      : start_(start),
        segments_(std::move(segments)),
        formula_(std::move(formula)) {}
  AmplitudeLinear start_;
  std::vector<EnvelopeSegment> segments_;
  std::string formula_;
};

// Silence before on, then the shape from on onward.
[[nodiscard]] inline Result<Function1D<AmplitudeLinear>, QuantityError>
envelope(SegmentEnvelope shape, TimePointSeconds on) {
  using R = Result<Function1D<AmplitudeLinear>, QuantityError>;
  const double tOn = on.value();
  if (!std::isfinite(tOn)) return R::failure(QuantityError::InvalidArgument);
  std::string formula = "env(" + shape.formula() + ")";
  return R::success(wrap<AmplitudeLinear>(
      [shape = std::move(shape), tOn](TimePointSeconds t) {
        const double tt = t.value();
        if (tt < tOn) return AmplitudeLinear{0.0};
        return AmplitudeLinear{shape.valueAfter(tt - tOn)};
      },
      std::move(formula)));
}

// Strike shape until off, then release segments from the strike value at
// off. off < on is InvalidArgument; release validation as create() with the
// value at off as start (so an Exponential release from 0 is DomainError).
[[nodiscard]] inline Result<Function1D<AmplitudeLinear>, QuantityError>
gatedEnvelope(SegmentEnvelope strike, std::vector<EnvelopeSegment> release,
              TimePointSeconds on, TimePointSeconds off) {
  using R = Result<Function1D<AmplitudeLinear>, QuantityError>;
  const double tOn = on.value();
  const double tOff = off.value();
  if (!std::isfinite(tOn) || !std::isfinite(tOff) || tOff < tOn)
    return R::failure(QuantityError::InvalidArgument);
  const double atOff = strike.valueAfter(tOff - tOn);
  auto rel = SegmentEnvelope::create(AmplitudeLinear{atOff},
                                     std::move(release));
  if (!rel.ok()) return R::failure(rel.error());
  std::string formula =
      "gated(" + strike.formula() + ", " + rel.value().formula() + ")";
  return R::success(wrap<AmplitudeLinear>(
      [strike = std::move(strike), release = std::move(rel).value(), tOn,
       tOff](TimePointSeconds t) {
        const double tt = t.value();
        if (tt < tOn) return AmplitudeLinear{0.0};
        if (tt < tOff) return AmplitudeLinear{strike.valueAfter(tt - tOn)};
        return AmplitudeLinear{release.valueAfter(tt - tOff)};
      },
      std::move(formula)));
}

// Exponential decay to -60 dB at T60 after on (silence before on).
// T60 finite and > 0, on finite: otherwise InvalidArgument.
[[nodiscard]] inline Result<Function1D<AmplitudeLinear>, QuantityError>
t60Decay(TimePointSeconds on, DurationSeconds t60) {
  using R = Result<Function1D<AmplitudeLinear>, QuantityError>;
  const double tOn = on.value();
  const double T = t60.value();
  if (!std::isfinite(tOn) || !std::isfinite(T) || !(T > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(wrap<AmplitudeLinear>(
      [tOn, T](TimePointSeconds t) {
        const double tt = t.value();
        if (tt < tOn) return AmplitudeLinear{0.0};
        return AmplitudeLinear{std::pow(10.0, -3.0 * (tt - tOn) / T)};
      },
      "t60(" + std::to_string(T) + "s)"));
}

}  // namespace mmathen
