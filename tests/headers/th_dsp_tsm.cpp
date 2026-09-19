// Header self-containment: mmathen/dsp/tsm.h compiles standalone.
#include <mmathen/dsp/tsm.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("dsp/tsm.h is self-contained", "[headers]") {
  CHECK(mmathen::pghiStretch({0.0}, 9.0).error() == mmathen::QuantityError::InvalidArgument);
}
