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

TEST_CASE("strap flex: bells knock when the gap closes; full ring has 38 measured partials") {
  REQUIRE(kSleighBellMeasuredPartialsFull.size() == 38);
  int ears = 0;
  for (const auto& p : kSleighBellMeasuredPartialsFull) ears += p.earMode ? 1 : 0;
  REQUIRE(ears == 2);
  SleighBellDesign d;
  d.strapHz = 30.0;
  d.bellGapM = 0.0005;
  SleighBellStrap s(6, 9, 44100.0, d), rigid(6, 9, 44100.0);
  const auto tl = parseSleighBellTimeline("shake 0 2 0.017 0.017 1.0\nstrike 0.7 2.5 1.0\nstrike 1.4 2.5 -1.0\n");
  std::vector<double> x(44100 * 2), y(44100 * 2);
  s.render(x.data(), x.size(), tl);
  rigid.render(y.data(), y.size(), tl);
  for (double v : x) REQUIRE(std::isfinite(v));
  REQUIRE(s.knocks() > 0);
  REQUIRE(rigid.knocks() == 0);
}

TEST_CASE("sphere mode radiation efficiency matches 1 / ((ka)^2 |h_n'(ka)|^2)") {
  // reference values from scipy.special.spherical_jn / spherical_yn (derivative=True)
  REQUIRE_THAT(sphereModeRadiationEfficiency(0, 0.5), WithinRel(0.2, 1e-9));  // (ka)^2 / (1 + (ka)^2)
  REQUIRE_THAT(sphereModeRadiationEfficiency(2, 0.55), WithinRel(3.312382e-4, 1e-5));
  REQUIRE_THAT(sphereModeRadiationEfficiency(4, 2.2), WithinRel(6.506543e-3, 1e-5));
  REQUIRE_THAT(sphereModeRadiationEfficiency(6, 4.4), WithinRel(5.739817e-2, 1e-5));
  REQUIRE(sphereModeRadiationEfficiency(3, 50.0) > 0.9);  // large ka: radiates fully
}
TEST_CASE("dense shell modes: count follows the plate modal density on the slotted hemisphere") {
  SleighBellDesign d;
  SleighBellDesign pro4 = d; pro4.shellModes = false; pro4.partialExtraDecayDbPerS = -1.0;
  SleighBellStrap a(4, 5, 44100.0, d), b(4, 5, 44100.0, pro4);
  REQUIRE(b.modeCount() == 4 * kSleighBellMeasuredPartialsFull.size());
  const double cL = std::sqrt(d.shellE / (d.shellRho * (1.0 - d.shellNu * d.shellNu)));
  const double perHz = 1.7320508075688772 * 6.283185307179586 * d.outerRadiusM * d.outerRadiusM / (d.wallM * cL);
  const double expected = 4.0 * perHz * (d.shellModeHiHz - 2514.0);
  const double extra = double(a.modeCount() - b.modeCount());
  REQUIRE(extra > 0.6 * expected); REQUIRE(extra < 1.4 * expected);
}
TEST_CASE("dense shell modes fill the gaps between partials; seeded; the pro4 bell is unchanged by them") {
  const auto tl = parseSleighBellTimeline("strike 0.2 1.2 -0.3\n");
  SleighBellDesign pro4; pro4.shellModes = false; pro4.partialExtraDecayDbPerS = -1.0;
  SleighBellStrap n1(6, 11, 44100.0), n2(6, 11, 44100.0), o(6, 11, 44100.0, pro4);
  std::vector<double> x(22050), y(22050), z(22050);
  n1.render(x.data(), x.size(), tl); n2.render(y.data(), y.size(), tl); o.render(z.data(), z.size(), tl);
  REQUIRE(x == y);
  REQUIRE(n1.impacts() == o.impacts());  // same pellets: only the shell response differs
  // 3-5 kHz (between the 2.5 and 5.9 kHz partials) re 6.4-7.0 kHz: Goertzel-free band energy by DFT bins
  auto band = [](const std::vector<double>& v, double lo, double hi) {
    const std::size_t a = 9261, n = 8192;  // 0.21 s .. 0.40 s
    double e = 0.0;
    for (double f = lo; f < hi; f += 44100.0 / n) {
      double re = 0, im = 0;
      for (std::size_t i = 0; i < n; ++i) { const double w = 6.283185307179586 * f * i / 44100.0; re += v[a + i] * std::cos(w); im += v[a + i] * std::sin(w); }
      e += re * re + im * im;
    }
    return e;
  };
  const double gapNew = band(x, 3000, 5000) / band(x, 2000, 16000), gapOld = band(z, 3000, 5000) / band(z, 2000, 16000);
  CAPTURE(gapNew, gapOld);
  REQUIRE(gapNew > 1.25 * gapOld);  // about +2 dB, as on the real-hit scoreboard (3-5k gap -43.5 -> -41.4 dB)
  for (double v : x) REQUIRE(std::isfinite(v));
}
