// Header self-containment: mmathen/stats/hmm.h compiles standalone.
#include <mmathen/stats/hmm.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stats/hmm.h is self-contained", "[headers]") {
  CHECK(mmathen::makeHmm({}, {}).error() == mmathen::QuantityError::InvalidArgument);
}
