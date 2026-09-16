// Header self-containment: mmathen/physics/waves.h compiles standalone.
#include <mmathen/physics/waves.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/waves.h is self-contained", "[headers]") {
  CHECK(mmathen::wavenumber(mmathen::DistanceMeters{1.0}).ok());
}
