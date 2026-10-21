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
TEST_CASE("Statistical body: Woodhouse 2004 modes 1.4-5.19 kHz present and deterministic") {
  const auto fitted = classicalGuitarBR2CompositeBodyModes();
  const auto a = classicalGuitarBR2StatisticalBodyModes(), b = classicalGuitarBR2StatisticalBodyModes();
  const auto areas = classicalGuitarBR2StatisticalAreasM2();
  // Mean spacing 38 Hz over 3790 Hz: ~100 modes; allow wide statistical slack.
  REQUIRE(a.size() > fitted.size() + 60);
  REQUIRE(a.size() < fitted.size() + 160);
  REQUIRE(areas.size() == a.size());
  for (std::size_t i = 0; i < fitted.size(); ++i) {
    REQUIRE(a[i].resonanceHz == fitted[i].resonanceHz);
    REQUIRE(a[i].qualityFactor == fitted[i].qualityFactor);
    REQUIRE(a[i].modalMassKg == fitted[i].modalMassKg);
    REQUIRE(areas[i] == kClassicalGuitarBR2CompositeAreasM2[i]);
  }
  REQUIRE(b.size() == a.size());
  double sumSpacing = 0.0;
  for (std::size_t i = fitted.size(); i < a.size(); ++i) {
    REQUIRE(a[i].resonanceHz > kWoodhouse2004StatisticalLowHz);
    REQUIRE(a[i].resonanceHz < kWoodhouse2004StatisticalHighHz);
    REQUIRE(a[i].qualityFactor >= kWoodhouse2004StatisticalQMin);
    REQUIRE(a[i].qualityFactor <= kWoodhouse2004StatisticalQMax);
    REQUIRE(a[i].modalMassKg >= 0.102);
    REQUIRE(a[i].modalMassKg <= 2.705);
    REQUIRE(areas[i] == 0.0);
    REQUIRE(a[i].resonanceHz > a[i - 1].resonanceHz);
    // Same seed, same guitar (libm may differ in the last ulp across compilers).
    REQUIRE(std::abs(b[i].resonanceHz - a[i].resonanceHz) < 1e-9);
    REQUIRE(b[i].qualityFactor == a[i].qualityFactor);
    REQUIRE(b[i].modalMassKg == a[i].modalMassKg);
    if (i > fitted.size()) sumSpacing += a[i].resonanceHz - a[i - 1].resonanceHz;
  }
  const double meanSpacing = sumSpacing / static_cast<double>(a.size() - fitted.size() - 1);
  REQUIRE(meanSpacing > 30.0);
  REQUIRE(meanSpacing < 46.0);
  // Another seed draws another guitar from the same statistics.
  const auto c = woodhouseStatisticalBodyModes(1400.0, 5190.0, 0.102, 2.705, kClassicalGuitarStatisticalSeed + 1);
  REQUIRE(!c.empty());
  REQUIRE(c[0].resonanceHz != a[fitted.size()].resonanceHz);
  REQUIRE(woodhouseStatisticalBodyModes(1400.0, 1000.0, 0.1, 1.0).empty());
}
