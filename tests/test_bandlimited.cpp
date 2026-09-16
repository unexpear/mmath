// D5 — PolyBLEP bandlimited oscillator tests (Catch2).
//
// Anchors are hand-computed from Tale's residual (verified verbatim):
// residual(0) = -1, residual(dt/2) = -0.25, residual(1-dt/2) = +0.25,
// zero away from discontinuities. Saw/square agreement with naive away
// from wraps is EXACT (shared code path + zero residual); the wrap
// sample differs substantially (the documented correction at work).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <utility>

#include <mmathen/math/comparison.h>
#include <mmathen/signal/bandlimited.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

constexpr double kDt = 440.0 / 48000.0;  // 0.0091667 cycles/sample
}  // namespace

TEST_CASE("residual anchors pin the polynomial", "[d5blep]") {
  // At phase 0 the saw's naive value (-1) plus residual (-1, subtracted)
  // lands exactly on 0: the smoothed step midpoint.
  CHECK(unwrap(polyblepSawValue(0.0, kDt)) == 0.0);
  CHECK(unwrap(polyblepSquareValue(0.0, kDt)) == 0.0);  // midpoint of step
  CHECK(unwrap(polyblepSquareValue(0.25, kDt)) ==
        unwrap(naiveSquareValue(0.25)));  // 1.0, far from edges
  CHECK(polyblepSawValue(-0.1, kDt).error() ==
        QuantityError::InvalidArgument);
  CHECK(polyblepSawValue(1.0, kDt).error() == QuantityError::InvalidArgument);
  CHECK(polyblepSawValue(0.25, 0.0).error() == QuantityError::InvalidArgument);
  CHECK(polyblepSawValue(0.25, 0.5).error() ==
        QuantityError::InvalidArgument);  // at Nyquist: rejected
  CHECK(polyblepSawValue(0.25, 0.75).error() == QuantityError::InvalidArgument);
}

TEST_CASE("bandlimited matches naive away from wraps", "[d5blep]") {
  for (double phase : {0.1, 0.25, 0.4, 0.6, 0.75, 0.9}) {
    CHECK(unwrap(polyblepSawValue(phase, kDt)) ==
          unwrap(naiveSawValue(phase)));
    CHECK(unwrap(polyblepSquareValue(phase, kDt)) ==
          unwrap(naiveSquareValue(phase)));
  }
  // At the wrap (n = 109 @440 Hz/48 kHz, phase = 47960/48000 = 0.9991667,
  // inside the transition region) the correction is large.
  const double phase = 109.0 * 440.0 / 48000.0;  // already in [0,1)
  CHECK(phase > 1.0 - kDt);  // inside the transition region
  CHECK(std::fabs(unwrap(polyblepSawValue(phase, kDt)) -
                  unwrap(naiveSawValue(phase))) > 0.5);
}

TEST_CASE("zero-crossing pitch is intact", "[d5blep]") {
  CHECK(unwrap(polyblepSawValue(0.5, kDt)) == 0.0);
  // Phase 0.5 exactly reachable: n = 4 at 6 kHz / 48 kHz.
  CHECK(unwrap(framePhase(FrameIndex{4}, FrequencyHz{6000.0},
                          SampleRateHz{48000.0}, 0.0)) == 0.5);
  CHECK(unwrap(framePhase(FrameIndex{0}, FrequencyHz{440.0},
                          SampleRateHz{48000.0}, 0.25)) == 0.25);
  CHECK(framePhase(FrameIndex{0}, FrequencyHz{24000.0},
                   SampleRateHz{48000.0}, 0.0)
            .error() == QuantityError::DomainError);  // at Nyquist
  CHECK(framePhase(FrameIndex{0}, FrequencyHz{0.0}, SampleRateHz{48000.0},
                   0.0)
            .error() == QuantityError::InvalidArgument);
}
