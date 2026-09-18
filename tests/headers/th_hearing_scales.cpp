// Header self-containment: mmathen/hearing/scales.h compiles standalone.
#include <mmathen/hearing/scales.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("hearing/scales.h is self-contained", "[headers]") {
  CHECK(mmathen::barkRate(mmathen::FrequencyHz{1000.0}).ok());
}
