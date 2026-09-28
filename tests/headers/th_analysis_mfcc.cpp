// Header self-containment: mmathen/analysis/mfcc.h compiles standalone.
#include <mmathen/analysis/mfcc.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/mfcc.h is self-contained", "[headers]") {
  CHECK(mmathen::hzToMel(1000.0, mmathen::MelScale::Slaney) == 15.0);
}
