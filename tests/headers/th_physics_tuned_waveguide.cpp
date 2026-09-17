// Header self-containment: mmathen/physics/tuned_waveguide.h compiles
// standalone.
#include <mmathen/physics/tuned_waveguide.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/tuned_waveguide.h is self-contained", "[headers]") {
  mmathen::LoopFilters f;
  f.intervals = 10;
  CHECK(mmathen::loopPole(f, mmathen::SampleRateHz{48000.0}, 1).ok());
}
