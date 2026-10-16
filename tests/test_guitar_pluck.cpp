#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <mmathen/instruments/guitar_pluck.h>
using namespace mmathen;
TEST_CASE("Woodhouse nylon loss gives longer low-mode T60 than high modes") {
  // String 1, Woodhouse Table 1: T 70.3 N, mu 0.38 g/m.
  const double lo = woodhouseT60Seconds(1, 329.6, 70.3, 0.38e-3), hi = woodhouseT60Seconds(1, 6 * 329.6, 70.3, 0.38e-3);
  REQUIRE(lo > hi);
  REQUIRE(lo > 3.0);  // eta ~1.1e-3 at f0 -> ~5.9 s
  REQUIRE(lo < 10.0);
}
TEST_CASE("Pluck fraction from bridge") {
  REQUIRE_THAT(pluckFractionFromBridge(0.13, 0.65), Catch::Matchers::WithinAbs(0.2, 1e-12));
  REQUIRE(pluckFractionFromBridge(0.5, 0.65) == 0.5);
}
TEST_CASE("Let-ring ends at next pluck of the same string") {
  auto e = letRingEnds({0.0, 0.1, 0.5}, {1, 2, 1}, 2.0);
  REQUIRE(e[0] == 0.5); REQUIRE(e[1] == 2.0); REQUIRE(e[2] == 2.0);
}
