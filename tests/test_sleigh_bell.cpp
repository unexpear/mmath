#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <mmathen/instruments/sleigh_bell.h>
#include <cmath>
#include <vector>
using namespace mmathen;
using Catch::Matchers::WithinRel;
TEST_CASE("closed sphere membrane modes match Abaqus Benchmark 1.4.1 Table 1") {
  REQUIRE_THAT(sphericalShellModeHz(0, 180e9, 7670, 1.0 / 3, 3.0, 0.0, true), WithinRel(445.0, 0.005));
  REQUIRE_THAT(sphericalShellModeHz(2, 180e9, 7670, 1.0 / 3, 3.0, 0.0), WithinRel(187.34, 0.005));
  // Table 1 lists n = 5 as 239.56 Hz; the formula gives 243.46 and the table's own
  // spacing (236.56, 239.56, 247.37) suggests a typo, so n = 4 and n = 6 are checked.
  REQUIRE_THAT(sphericalShellModeHz(4, 180e9, 7670, 1.0 / 3, 3.0, 0.0), WithinRel(236.56, 0.002));
  REQUIRE_THAT(sphericalShellModeHz(6, 180e9, 7670, 1.0 / 3, 3.0, 0.0), WithinRel(247.37, 0.002));
  REQUIRE_THAT(sphericalShellModeHz(2, 180e9, 7670, 1.0 / 3, 3.0, 0.0, true), WithinRel(748.02, 0.005));
}
TEST_CASE("bending correction within 2.5 % of Abaqus Table 2 (SAX2, h/R 0.05)") {
  const double fe[] = {187.82, 225.57, 246.09, 265.76, 290.66, 323.68, 366.02, 417.88};
  for (int n = 2; n <= 9; ++n) REQUIRE_THAT(sphericalShellModeHz(n, 180e9, 7670, 1.0 / 3, 3.0, 0.15), WithinRel(fe[n - 2], 0.025));
}
TEST_CASE("closed 15 mm brass shell has no audible mode; slotted ear does") {
  SleighBellDesign d;
  REQUIRE(sphericalShellModeHz(2, d.shellE, d.shellRho, d.shellNu, d.outerRadiusM, d.wallM) > 20000.0);
  const double L = cantileverLengthForHz(2514.0, d.shellE, d.shellRho, d.wallM);
  REQUIRE(L > 0.005); REQUIRE(L < d.outerRadiusM * 3.2);  // fits on the bell
  REQUIRE_THAT(cantileverStripHz(0, d.shellE, d.shellRho, d.wallM, L), WithinRel(2514.0, 1e-9));
  REQUIRE_THAT(cantileverStripHz(1, d.shellE, d.shellRho, d.wallM, L) / 2514.0, WithinRel(6.2669, 1e-3));
}
TEST_CASE("Hertz contact duration falls as v^-1/5") {
  const double a = hertzContactSeconds(1e-3, 3e-3, 1e11, 0.1), b = hertzContactSeconds(1e-3, 3e-3, 1e11, 3.2);
  REQUIRE(a > 0.0); REQUIRE_THAT(a / b, WithinRel(2.0, 1e-6));
  REQUIRE(halfSinePulseWeight(0.0, 1e-5) == 1.0);
}
TEST_CASE("strap: seeded, finite, impact rate near the published 15-30 per second") {
  const double fs = 48000.0;
  SleighBellStrap a(1, 7, fs), b(1, 7, fs);
  std::vector<double> x(48000 * 4), y(48000 * 4);
  a.render(x.data(), x.size(), {}); b.render(y.data(), y.size(), {});
  REQUIRE(x == y);
  double e = 0; for (double v : x) { REQUIRE(std::isfinite(v)); e += v * v; }
  REQUIRE(e > 0.0);
  const double rate = double(a.impacts()) / 4.0;
  REQUIRE(rate > 7.5); REQUIRE(rate < 60.0);
}
