// Header self-containment: mmathen/hearing/masking.h compiles standalone.
#include <mmathen/hearing/masking.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("hearing/masking.h is self-contained", "[headers]") {
  CHECK(mmathen::mpegSpreadingDb(0.0).ok());
}
