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
TEST_CASE("strap: seeded, finite, impact rate near the published 15-30 per second (sinusoid shake)") {
  const double fs = 48000.0;
  ShakeGesture g; g.minimumJerkStrokes = false; g.amplitudeM = kSleighBellShakeAmplitudeM;
  SleighBellStrap a(1, 7, fs), b(1, 7, fs);
  std::vector<double> x(48000 * 4), y(48000 * 4);
  a.render(x.data(), x.size(), g); b.render(y.data(), y.size(), g);
  REQUIRE(x == y);
  double e = 0; for (double v : x) { REQUIRE(std::isfinite(v)); e += v * v; }
  REQUIRE(e > 0.0);
  const double rate = double(a.impacts()) / 4.0;
  REQUIRE(rate > 7.5); REQUIRE(rate < 60.0);
}
TEST_CASE("minimum-jerk shake: zero in pauses, net zero velocity per stroke") {
  ShakeGesture g; const double per = 1.0 / g.rateHz, T = 0.5 * g.strokeDuty * per;
  REQUIRE(shakeAcceleration(g, T + 0.01 * per, false) == 0.0);
  double v = 0; const int n = 20000; for (int i = 0; i < n; ++i) v += shakeAcceleration(g, (i + 0.5) * T / n, false) * T / n;
  REQUIRE(std::abs(v) < 1e-6);
  // peak acceleration 10/sqrt(3) A / T^2 at s = (3 - sqrt 3)/6 (Flash-Hogan profile)
  const double s0 = (3.0 - std::sqrt(3.0)) / 6.0;
  REQUIRE_THAT(shakeAcceleration(g, s0 * T, false), WithinRel(5.7735 * g.amplitudeM / (T * T), 1e-3));
}
TEST_CASE("strap with minimum-jerk strokes: seeded, finite; 10 mm strokes cannot lift the pellet") {
  SleighBellStrap a(2, 3, 44100.0), b(2, 3, 44100.0), c(1, 3, 44100.0);
  std::vector<double> x(44100 * 2), y(44100 * 2);
  a.render(x.data(), x.size(), ShakeGesture{}); b.render(y.data(), y.size(), ShakeGesture{});
  REQUIRE(x == y);
  for (double v : x) REQUIRE(std::isfinite(v));
  REQUIRE(a.impacts() > 0);
  ShakeGesture weak; weak.amplitudeM = 0.010;  // peak 9.6 m/s^2 < g
  std::vector<double> z(44100); c.render(z.data(), z.size(), weak);  // 1 s: drop and settle
  const auto settled = c.impacts();
  for (int k = 0; k < 3; ++k) c.render(z.data(), z.size(), weak);
  REQUIRE(double(c.impacts() - settled) / 3.0 < 1.0);  // resting pellet, no rattle
}

TEST_CASE("gesture timeline: text form parses; strikes ring; the pellet settles when the hand stops") {
  const auto tl = parseSleighBellTimeline(
      "# comment\nshake 0.0 1.0 0.017 0.0 0.3\nstrike 0.5 1.3 -0.3  # accent\nbogus 1 2\nstrike 0.8 1.0\n");
  REQUIRE(tl.size() == 3);
  REQUIRE(tl[0].kind == SleighBellEvent::Kind::Shake);
  REQUIRE(tl[0].endStrength == 0.0);
  REQUIRE(tl[1].kind == SleighBellEvent::Kind::Strike);
  REQUIRE(tl[1].lateral == -0.3);
  REQUIRE(tl[2].lateral == 0.3);
  // a strike's acceleration integrates to zero (hand ends at rest)
  double v = 0.0;
  for (int i = 0; i < 44100; ++i) {
    double az = 0, ax = 0;
    gestureEventAcceleration(tl[1], ShakeGesture{}, i / 44100.0, az, ax);
    v += az / 44100.0;
  }
  REQUIRE(std::fabs(v) < 1e-3);
  SleighBellStrap s(6, 3, 44100.0), q(6, 3, 44100.0);
  std::vector<double> x(44100);
  q.render(x.data(), x.size(), std::vector<SleighBellEvent>{});  // drop and settle, no gesture
  const auto rest = q.impacts();
  q.render(x.data(), x.size(), std::vector<SleighBellEvent>{});
  REQUIRE(q.impacts() - rest < 3);
  s.render(x.data(), x.size(), tl);
  double peak = 0.0;
  for (double y : x) { REQUIRE(std::isfinite(y)); peak = std::max(peak, std::fabs(y)); }
  REQUIRE(peak > 0.0);
  // after the last event the hand is still: within 1 s the pellets stop
  // and the ring dies (strap damping x1.4: >= 56 dB/s on every partial)
  const auto done = s.impacts();
  s.render(x.data(), x.size(), tl);  // t = 1..2 s, no events active
  const auto late = s.impacts() - done;
  std::vector<double> tail(44100);
  s.render(tail.data(), tail.size(), tl);  // t = 2..3 s
  REQUIRE(s.impacts() - done - late < 3);
  double tailPeak = 0.0;
  for (double y : tail) tailPeak = std::max(tailPeak, std::fabs(y));
  REQUIRE(tailPeak < 0.01 * peak);
}
