// Header self-containment: mmathen/stats/gmm.h compiles standalone.
#include <mmathen/stats/gmm.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stats/gmm.h is self-contained", "[headers]") {
  CHECK(mmathen::trainGmm({}, {}).error() == mmathen::QuantityError::InvalidArgument);
}
