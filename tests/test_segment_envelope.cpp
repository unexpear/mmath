// D7 — segment envelope tests (Catch2).
//
// Linear anchors are binary-exact (CHECK ==). Exponential segments are
// checked for dB-linearity against log10 literals; curved segments against
// the closed form at c = +-2*ln(3), where the midpoint is exactly 1/4 or
// 3/4. gatedEnvelope is cross-checked against the independent adsr() and
// the exponential segment against the independent t60Decay().

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/signal/envelope.h>
#include <mmathen/signal/segment_envelope.h>
#include <mmathen/time/function.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

EnvelopeSegment seg(double d, double target,
                    SegmentCurve curve = SegmentCurve::Linear,
                    double shape = 0.0) {
  return EnvelopeSegment{DurationSeconds{d}, AmplitudeLinear{target}, curve,
                         shape};
}

double at(const Function1D<AmplitudeLinear>& f, double t) {
  return f.at(TimePointSeconds{t}).value();
}
}  // namespace

TEST_CASE("linear segments hit exact boundaries and hold", "[d7env]") {
  const auto shape = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0}, {seg(0.25, 1.0), seg(0.5, 0.5), seg(0.0, 0.75)}));
  CHECK(shape.valueAfter(-1.0) == 0.0);
  CHECK(shape.valueAfter(0.0) == 0.0);
  CHECK(shape.valueAfter(0.125) == 0.5);
  CHECK(shape.valueAfter(0.25) == 1.0);
  CHECK(shape.valueAfter(0.5) == 0.75);
  // Zero-duration segment jumps: at 0.75 the 0.5 target is passed at once.
  CHECK(shape.valueAfter(0.75) == 0.75);
  CHECK(shape.valueAfter(5.0) == 0.75);
  CHECK(shape.totalDuration() == DurationSeconds{0.75});
  CHECK(shape.formula() == "segments[3]");

  const auto env = unwrap(envelope(shape, TimePointSeconds{1.0}));
  CHECK(at(env, 0.999) == 0.0);
  CHECK(at(env, 1.125) == 0.5);
  CHECK(env.formula() == "env(segments[3])");
}

TEST_CASE("exponential segments are straight lines in dB", "[d7env]") {
  const auto shape = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{1.0}, {seg(3.0, 0.001, SegmentCurve::Exponential)}));
  // -60 dB over 3 s: -20 dB per second.
  for (const double t : {0.5, 1.0, 1.5, 2.0, 2.5}) {
    const double db = 20.0 * std::log10(shape.valueAfter(t));
    CHECK(nearlyEqual(db, -20.0 * t, 1e-12, 0.0));
  }
  CHECK(nearlyEqual(shape.valueAfter(1.0), 0.1, 0.0, 1e-14));
  CHECK(shape.valueAfter(3.0) == 0.001);  // segment end is exact
}

TEST_CASE("curved segments follow the expm1 closed form", "[d7env]") {
  const double c = 2.0 * std::log(3.0);  // e^(c/2) = 3: midpoint 1/(3+1)
  const auto slow = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0}, {seg(1.0, 1.0, SegmentCurve::Curved, c)}));
  const auto fast = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0}, {seg(1.0, 1.0, SegmentCurve::Curved, -c)}));
  const auto flat = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0}, {seg(1.0, 1.0, SegmentCurve::Curved, 0.0)}));
  const auto tiny = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0}, {seg(1.0, 1.0, SegmentCurve::Curved, 1e-12)}));
  CHECK(nearlyEqual(slow.valueAfter(0.5), 0.25, 1e-15, 0.0));
  CHECK(nearlyEqual(fast.valueAfter(0.5), 0.75, 1e-15, 0.0));
  CHECK(flat.valueAfter(0.5) == 0.5);
  CHECK(nearlyEqual(tiny.valueAfter(0.5), 0.5, 1e-12, 0.0));
  CHECK(slow.valueAfter(1.0) == 1.0);
  // Monotone rise for both signs of c.
  double prevSlow = 0.0;
  double prevFast = 0.0;
  for (int i = 1; i <= 100; ++i) {
    const double u = i / 100.0;
    CHECK(slow.valueAfter(u) >= prevSlow);
    CHECK(fast.valueAfter(u) >= prevFast);
    prevSlow = slow.valueAfter(u);
    prevFast = fast.valueAfter(u);
  }
}

TEST_CASE("gated linear envelope reproduces adsr", "[d7env]") {
  const auto segs = unwrap(makeADSR(DurationSeconds{0.25},
                                    DurationSeconds{0.5}, AmplitudeLinear{0.5},
                                    DurationSeconds{0.5}));
  const auto strike = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0}, {seg(0.25, 1.0), seg(0.5, 0.5)}));
  // Full note, and a short note released during the attack.
  for (const double off : {2.5, 0.6}) {
    const auto ref =
        unwrap(adsr(segs, TimePointSeconds{0.5}, TimePointSeconds{off}));
    const auto gated = unwrap(gatedEnvelope(strike, {seg(0.5, 0.0)},
                                            TimePointSeconds{0.5},
                                            TimePointSeconds{off}));
    for (double t = 0.0; t <= 3.5; t += 0.0173)
      CHECK(nearlyEqual(at(gated, t), at(ref, t), 1e-12, 0.0));
    CHECK(gated.formula() == "gated(segments[2], segments[1])");
  }
}

TEST_CASE("exponential segment and t60Decay agree", "[d7env]") {
  const auto decay =
      unwrap(t60Decay(TimePointSeconds{0.2}, DurationSeconds{1.5}));
  CHECK(at(decay, 0.1) == 0.0);
  CHECK(at(decay, 0.2) == 1.0);
  CHECK(nearlyEqual(at(decay, 0.7), 0.1, 0.0, 1e-14));
  CHECK(nearlyEqual(at(decay, 1.7), 0.001, 0.0, 1e-14));
  const auto shape = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{1.0}, {seg(1.5, 0.001, SegmentCurve::Exponential)}));
  const auto env = unwrap(envelope(shape, TimePointSeconds{0.2}));
  for (double t = 0.2; t < 1.7; t += 0.037)
    CHECK(nearlyEqual(at(env, t), at(decay, t), 0.0, 1e-12));
}

TEST_CASE("segment envelopes reject invalid input", "[d7env]") {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const AmplitudeLinear one{1.0};
  CHECK(SegmentEnvelope::create(AmplitudeLinear{-0.1}, {}).error() ==
        QuantityError::InvalidArgument);
  CHECK(SegmentEnvelope::create(one, {seg(-1.0, 0.5)}).error() ==
        QuantityError::InvalidArgument);
  CHECK(SegmentEnvelope::create(one, {seg(1.0, -0.5)}).error() ==
        QuantityError::InvalidArgument);
  CHECK(SegmentEnvelope::create(one, {seg(1.0, 0.5, SegmentCurve::Curved,
                                          nan)})
            .error() == QuantityError::InvalidArgument);
  // A true exponential never reaches or leaves zero.
  CHECK(SegmentEnvelope::create(one, {seg(1.0, 0.0, SegmentCurve::Exponential)})
            .error() == QuantityError::DomainError);
  CHECK(SegmentEnvelope::create(AmplitudeLinear{0.0},
                                {seg(1.0, 1.0, SegmentCurve::Exponential)})
            .error() == QuantityError::DomainError);

  const auto strike = unwrap(
      SegmentEnvelope::create(AmplitudeLinear{0.0}, {seg(1.0, 1.0)}));
  CHECK(gatedEnvelope(strike, {seg(1.0, 0.0)}, TimePointSeconds{1.0},
                      TimePointSeconds{0.5})
            .error() == QuantityError::InvalidArgument);
  // Released at on: strike value is 0, so an exponential release fails.
  CHECK(gatedEnvelope(strike, {seg(1.0, 0.1, SegmentCurve::Exponential)},
                      TimePointSeconds{1.0}, TimePointSeconds{1.0})
            .error() == QuantityError::DomainError);
  CHECK(t60Decay(TimePointSeconds{0.0}, DurationSeconds{0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(envelope(strike, TimePointSeconds{nan}).error() ==
        QuantityError::InvalidArgument);
}
