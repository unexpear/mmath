// Header self-containment: mmathen/signal/segment_envelope.h compiles
// standalone.
#include <mmathen/signal/segment_envelope.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("signal/segment_envelope.h is self-contained", "[headers]") {
  const auto e = mmathen::SegmentEnvelope::create(
      mmathen::AmplitudeLinear{0.0},
      {mmathen::EnvelopeSegment{mmathen::DurationSeconds{1.0},
                                mmathen::AmplitudeLinear{1.0}}});
  CHECK(e.ok());
}
