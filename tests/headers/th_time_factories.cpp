// Header self-containment: mmathen/time/factories.h compiles standalone.
#include <mmathen/time/factories.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("time/factories.h is self-contained", "[headers]") {
  const auto f = mmathen::constant(mmathen::FrequencyHz{440.0});
  CHECK(f.at(mmathen::TimePointSeconds{1.0}) == mmathen::FrequencyHz{440.0});
}
