#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/guitar_pluck.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <cmath>
#include <vector>
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
TEST_CASE("Guitar per-mode areas: Perry/Roberts BR2 values, T(1,1)1 opposite polarity") {
  const auto& a = kClassicalGuitarBodyModeAreasM2;
  REQUIRE(a.size() == classicalGuitarBodyModes().size());
  REQUIRE(a[0] < 0.0);
  REQUIRE(a[1] > 0.0);
  // A = Gamma00 * m w / Q on this body (Perry p.106, Debut-Antunes mode 1).
  const double tau = 6.283185307179586;
  REQUIRE_THAT(-a[0], Catch::Matchers::WithinRel(8.4e-3 * 0.40 * tau * 89.4 * 0.03, 0.01));
  REQUIRE_THAT(a[4], Catch::Matchers::WithinRel(3.0e-3 * 1.06 * tau * 403.5 * 0.011, 0.01));
}
TEST_CASE("Bass damping: bass note ends at next bass-string onset, treble lets ring") {
  // strings: 6 (bass), 2 (treble), 5 (bass), 6
  auto e = letRingEndsBassDamped({0.0, 0.5, 1.0, 2.0}, {6, 2, 5, 6}, 3.0);
  REQUIRE(e[0] == 1.0); REQUIRE(e[1] == 3.0); REQUIRE(e[2] == 2.0); REQUIRE(e[3] == 3.0);
}
TEST_CASE("BR2 composite body: Roberts Table 4.3 below 500 Hz, Debut above") {
  const auto m = classicalGuitarBR2CompositeBodyModes();
  REQUIRE(m[4].resonanceHz == 257.7); REQUIRE(m[5].modalMassKg == 2.705);
  REQUIRE(m[8].resonanceHz == 511.4);
  REQUIRE(kClassicalGuitarBR2CompositeAreasM2[0] < 0.0);
  for (std::size_t i = 1; i < m.size(); ++i) REQUIRE(m[i].resonanceHz > m[i - 1].resonanceHz);
}
