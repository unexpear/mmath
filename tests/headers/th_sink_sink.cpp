// Header self-containment: mmathen/sink/sink.h compiles standalone.
#include <mmathen/sink/sink.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("sink/sink.h is self-contained", "[headers]") {
  mmathen::NullSink sink;
  const double frames[2] = {0.25, -0.5};
  CHECK(sink.consume(frames, mmathen::FrameCount{2}));
  CHECK(sink.stats().framesRendered == 2);
}
