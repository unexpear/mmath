// Header self-containment: mmathen/hearing/loudness.h compiles standalone.
#include <mmathen/hearing/loudness.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("hearing/loudness.h is self-contained", "[headers]") {
  CHECK(mmathen::sonesFromPhon(mmathen::Phons{40.0}).ok());
}
