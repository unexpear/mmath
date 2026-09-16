// Header self-containment: mmathen/time/function.h compiles standalone.
#include <mmathen/time/function.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("time/function.h is self-contained", "[headers]") {
  const mmathen::Function1D<mmathen::FrequencyHz> f(
      [](mmathen::TimePointSeconds) { return mmathen::FrequencyHz{440.0}; },
      "const");
  CHECK(f.at(mmathen::TimePointSeconds{0.0}) == mmathen::FrequencyHz{440.0});
}
