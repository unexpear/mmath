// Header self-containment: mmathen/units/quantity.h compiles standalone.
#include <mmathen/units/quantity.h>

#include <catch2/catch_test_macros.hpp>

namespace {
struct LocalDim {};
struct LocalTag {};
using LocalQ = mmathen::Quantity<double, LocalDim, LocalTag>;
}  // namespace

TEST_CASE("quantity.h is self-contained", "[headers]") {
  CHECK(LocalQ{1.0} == LocalQ{1.0});
}
