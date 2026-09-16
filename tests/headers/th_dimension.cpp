// Header self-containment: mmathen/units/dimension.h compiles standalone.
#include <mmathen/units/dimension.h>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

TEST_CASE("dimension.h is self-contained", "[headers]") {
  CHECK_FALSE((std::is_same_v<mmathen::DimTime, mmathen::DimInverseTime>));
}
