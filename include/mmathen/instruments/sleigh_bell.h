#pragma once
// Sleigh bells (crotals): a slotted spherical brass shell with a loose
// steel pellet, several bells on a strap, driven by a hand-shake gesture.
// Physics only: rigid-pellet flight in the moving shell, Hertz impacts,
// modal shell response. No samples.
//
// Shell modes.
//  - Closed thin sphere (membrane theory; Baker 1961, Silbiger 1962,
//    Kalnins 1964): with Omega = w R / c, c^2 = E / (rho (1 - nu^2)),
//    lambda = n (n + 1),
//      Omega^4 - (1 + 3 nu + lambda) Omega^2 + (lambda - 2)(1 - nu^2) = 0.
//    Checked against Abaqus Benchmarks 1.4.1 "Free vibrations of a
//    spherical shell", Table 1 (n = 0: 445.0 Hz, n = 2 lower: 187.34 Hz
//    for E 180 GPa, nu 1/3, rho 7670, R 3 m) in the tests.
//    Bending: Omega^2 += beta (lambda - 2)^2 lambda / (lambda + 1 - nu),
//    beta = h^2 / (12 R^2). This correction is an approximation (not
//    Kalnins' exact equation); against Abaqus Table 2 (SAX2, h/R 0.05)
//    it is within 2.5 % for n = 2..9 (tests). VERIFY.
//    Result: a closed 15 mm brass shell has no mode below ~20 kHz, so the
//    audible partials are not closed-sphere modes.
//  - The slot. "A slot in the top half of the crotal forms opposing ears
//    which vibrate, producing the familiar sleigh bell sound"; a longer
//    slot lowers the pitch (R. A. Bergum, "Tuning Sleigh Bells
//    (Crotals)", OCTOS Wurlitzer repairs page). Each ear is modelled as an
//    Euler-Bernoulli cantilever strip: f_k = (b_k L)^2 / (2 pi L^2)
//    sqrt(E h^2 / (12 rho)), b_k L = 1.8751, 4.6941 (clamped-free).
//    Curvature of the ear is ignored (VERIFY).
//  - Partials not covered by the cantilever: MEASURED from the CC0
//    reference "Sleigh bells.wav" (Wikimedia Commons, The Midnite Wolf,
//    AKG C414; mean spectrum, 8192-pt STFT): peaks at 2514, 5959, 6659,
//    7084, 10228, 10686, 16796 Hz. The 2514 Hz peak sets the ear length
//    (TUNED); 16796 Hz is the ear's second bending mode (cantilever ratio
//    6.27 predicts 15.8 kHz, 6 % low, VERIFY); the five middle partials
//    are kept at their measured frequencies, physical identity unassigned
//    (likely ear torsion / cap modes; UNSOURCED).
//  - Damping: free-ring decay MEASURED on the same recording (75th
//    percentile of 0.1 s falling slopes per partial band): 41, 30, 32, 36,
//    42, 48, 53 dB/s -> T60 = 60 / slope. VERIFY.
//
// Pellet. Point mass in the shell frame: dv/dt = g - a_shell(t); at the
// wall the normal velocity reverses with restitution e and the shell
// receives impulse J = m (1 + e) v_n. The force pulse is a half-sine of
// Hertz duration (K. L. Johnson, "Contact Mechanics", 1985, sec. 11.4):
//   tau = 2.87 (m*^2 / (R* E*^2 v))^(1/5),
// R* for a ball inside a concave sphere: 1/R* = 1/r - 1/R_in. The pulse
// spectrum |F(w)| = J |cos(w tau / 2)| / |1 - (w tau / pi)^2| weights each
// mode. Pellet bells are hit 15-30 times per second when shaken (Pomberger
// et al., Archaeologiai Ertesito / Hungarian National Museum pellet-bell
// study, 2022); the test checks the model lands in a wide band around it.
// Hand motion: sinusoid at kSleighBellShakeHz (MEASURED, envelope modulation
// of the reference) with amplitude UNSOURCED.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace mmathen {

// --- closed spherical shell -------------------------------------------------
[[nodiscard]] inline double sphericalShellMembraneOmega2(int n, double nu, bool upper = false) {
  const double l = double(n) * (n + 1);
  const double b = 1.0 + 3.0 * nu + l;
  const double d = std::sqrt(std::max(0.0, b * b - 4.0 * (l - 2.0) * (1.0 - nu * nu)));
  return upper ? (b + d) / 2.0 : (b - d) / 2.0;
}
[[nodiscard]] inline double sphericalShellModeHz(int n, double E, double rho, double nu, double R, double h,
                                                bool upper = false) {
  double w2 = sphericalShellMembraneOmega2(n, nu, upper);
  if (!upper && h > 0.0) {
    const double l = double(n) * (n + 1), beta = h * h / (12.0 * R * R);
    w2 += beta * (l - 2.0) * (l - 2.0) * l / (l + 1.0 - nu);
  }
  const double c = std::sqrt(E / (rho * (1.0 - nu * nu)));
  return std::sqrt(std::max(0.0, w2)) * c / (6.283185307179586 * R);
}

// --- slotted ear: Euler-Bernoulli clamped-free strip -----------------------
inline constexpr std::array<double, 2> kCantileverBetaL{1.8751040687, 4.6940911330};
[[nodiscard]] inline double cantileverStripHz(int k, double E, double rho, double h, double L) {
  const double b = kCantileverBetaL[std::size_t(k)];
  return b * b / (6.283185307179586 * L * L) * std::sqrt(E * h * h / (12.0 * rho));
}
[[nodiscard]] inline double cantileverLengthForHz(double f1, double E, double rho, double h) {
  const double b = kCantileverBetaL[0];
  return std::sqrt(b * b / (6.283185307179586 * f1) * std::sqrt(E * h * h / (12.0 * rho)));
}

// --- Hertz impact -----------------------------------------------------------
[[nodiscard]] inline double hertzContactSeconds(double reducedMass, double reducedRadius, double reducedModulus,
                                               double speed) {
  if (!(speed > 0.0) || !(reducedRadius > 0.0)) return 0.0;
  return 2.87 * std::pow(reducedMass * reducedMass / (reducedRadius * reducedModulus * reducedModulus * speed), 0.2);
}
[[nodiscard]] inline double halfSinePulseWeight(double omega, double tau) {
  const double x = omega * tau / 3.141592653589793;
  if (std::fabs(1.0 - x * x) < 1e-6) return 3.141592653589793 / 4.0;  // limit at x = 1
  return std::fabs(std::cos(omega * tau / 2.0) / (1.0 - x * x));
}

// --- bell, pellet, strap ----------------------------------------------------
struct SleighBellDesign {
  double outerRadiusM = 0.015;    // 30 mm bell (#1-#2 size, classicbells.com sizing table), VERIFY
  double wallM = 0.0006;          // UNSOURCED
  double pelletRadiusM = 0.003;   // UNSOURCED
  double shellE = 100e9, shellRho = 8500.0, shellNu = 0.34;    // brass, handbook typical VERIFY
  double pelletE = 200e9, pelletRho = 7850.0, pelletNu = 0.29; // steel, handbook typical VERIFY
  double restitution = 0.6;       // metal-metal low speed, VERIFY
  double tangentialKeep = 0.9;    // tangential velocity kept per impact, UNSOURCED
  // sd of bell-to-bell timing offsets on the strap. TUNED on the reference
  // accents (median 10-90 % rise, real 29 ms): 10 ms -> 98 ms, 3 ms -> 42 ms.
  double strapLagS = 0.003;
  // Contact (acceleration) noise: during the Hertz pulse the shell as a
  // rigid body is accelerated by F/M and radiates as a compact dipole,
  // p ~ d(F/M)/dt (A. Akay, "A review of impact noise", JASA 64, 977-987,
  // 1978). The pulse (tau ~ 10-40 us) is shorter than a sample, so the
  // click is the first difference of the impulse J/M: broadband, rising
  // 6 dB/octave. Level relative to the modes: clickGain, TUNED.
  // Attack band 12-15.5 kHz re 6.4-6.9 kHz, real -20.7 dB: gain 0 -> -26.3,
  // 3 -> -22.8, 10 -> -15.8 (3 ms lag). 3 chosen.
  double clickGain = 3.0;
  // Strap flex. Each bell hangs on the leather as a damped spring (base
  // excitation, vertical and lateral): shell acceleration is the hand's
  // filtered by that mode, so bells move apart and their pellets fall out
  // of step. 0 = rigid strap. Frequency, its bell-to-bell scatter and the
  // damping ratio are UNSOURCED. Pro4 sweep (15/20/30/45/60 Hz, zeta
  // 0.15/0.4, 3 seeds): flex cuts the extra shake onsets (29 -> 14-20, real
  // 16) but slows every accent (rise 52 -> 64-71 ms, real 29) and lowers the
  // pattern match (0.84 -> 0.76-0.82), so the default stays rigid.
  double strapHz = 0.0;
  double strapHzScatter = 0.15;
  double strapZeta = 0.15;
  // Neighbouring bells knock when their lateral gap closes: shell-shell
  // Hertz contact (two convex 15 mm spheres, R* = R/2, m* = M/2), equal-mass
  // restitution. Rest gap and restitution UNSOURCED. Needs strapHz > 0; at
  // 30 Hz with a 2 mm gap only 1-13 knocks happen in the 9.8 s reference
  // gesture (55 at 1 mm), too few to change the measures.
  double bellGapM = 0.002;
  double bellRestitution = 0.5;
  double dampingFactor = -1.0;  // <0: kSleighBellStrapDampingFactor
  bool fullRing = true;  // 38 measured partials (false: the 9 strongest, pro1-pro3)
};
struct SleighBellPartial { double hz; double levelDb; double decayDbPerS; int earMode = 0; };
// MEASURED from the CC0 reference (see header). Levels: dB re the strongest.
// Index 0 and 6 are the ear's two bending modes; 7 and 8 (17.0, 17.69 kHz)
// are further measured top-octave peaks, decay taken as 16.8 kHz's (VERIFY).
inline constexpr std::array<SleighBellPartial, 9> kSleighBellMeasuredPartials{{
    {2514, -5.8, 40.7, 1}, {5959, -4.7, 29.7}, {6659, 0.0, 31.9}, {7084, -2.4, 35.9},
    {10228, -0.3, 41.6}, {10686, -0.4, 47.8}, {16796, -6.5, 52.5, 2}, {17000, -10.9, 52.5}, {17690, -7.0, 52.5},
}};
// Full ring: every peak of the reference's mean spectrum (16384-pt Hann,
// 75 % overlap, whole clip) with prominence >= 6 dB, level > -32 dB re the
// strongest, above 1 kHz: 38 peaks. Decay per peak: same p75 falling-slope
// measure on a +-min(0.6 %, 40 Hz) band. MEASURED. The close clusters
// (2514/2657, 5669-6207, 9138-9434, 15.9-18.5 kHz) are likely bells of
// slightly different size on the strap; every model bell carries all of
// them (VERIFY: per-bell assignment). earMode 1/2: the ear's bending modes.
inline constexpr std::array<SleighBellPartial, 38> kSleighBellMeasuredPartialsFull{{
    {1494, -30.5, 34.7}, {2514, -6.0, 42.4, 1}, {2657, -9.9, 45.7}, {5669, -13.0, 27.3}, {5752, -19.3, 27.5},
    {5852, -7.7, 40.4}, {5957, -4.4, 38.8}, {6207, -12.9, 40.5}, {6659, -0.0, 37.5}, {6802, -4.7, 39.1},
    {7087, -2.3, 39.1}, {8433, -30.3, 37.9}, {9138, -13.2, 38.9}, {9246, -8.9, 34.0}, {9310, -14.2, 32.5},
    {9434, -13.7, 34.2}, {10228, 0.0, 37.1}, {10331, -5.9, 45.6}, {10414, -7.8, 47.6}, {10686, -0.4, 55.0},
    {10772, -2.1, 56.0}, {10958, -8.0, 60.8}, {11399, -11.1, 69.8}, {12508, -27.3, 56.0}, {15943, -14.9, 73.4},
    {16158, -13.9, 57.0}, {16258, -13.1, 51.3}, {16422, -16.0, 63.7}, {16607, -23.4, 44.3}, {16796, -7.0, 64.9, 2},
    {17003, -11.0, 62.8}, {17243, -15.9, 57.4}, {17480, -16.3, 73.1}, {17690, -7.6, 66.2}, {17873, -12.5, 71.1},
    {18066, -12.0, 65.2}, {18481, -23.8, 53.6}, {18545, -16.7, 67.0},
}};
inline constexpr double kSleighBellShakeHz = 3.36;         // MEASURED: envelope-modulation peak of the reference
// Peak hand acceleration A (2 pi f)^2 must exceed g to lift the pellet: at
// 3.36 Hz that needs A > 22 mm. 25 mm is just above. UNSOURCED.
inline constexpr double kSleighBellShakeAmplitudeM = 0.025;
// Minimum-jerk strokes peak at 10/sqrt(3) A / T^2 (T = stroke time, 77 ms
// at duty 0.52): lift needs A > 10.2 mm. 15 mm is the smallest of the
// 10/15/20/25 mm sweep that lifts it and matched all 7 reference peaks. TUNED.
inline constexpr double kSleighBellStrokeAmplitudeM = 0.015;
// Free-ring decays measured on the reference are biased slow by re-hits, and
// bells riveted to a leather strap held in the hand lose energy into it.
// Pro4 (pattern-driven A/B, 38-partial ring, 3 seeds): factor 1.4 -> accent
// 10 dB fall 79 ms, 2 -> 68, 2.5 -> 56, 3 -> 48 (real 52); the p75 free-decay
// measure reads -43..-48 dB/s at 2.5 vs real -29..-40 (x2: -30..-38). 2.5 is
// the compromise: TUNED.
inline constexpr double kSleighBellStrapDampingFactor = 2.5;
// Bell-to-bell scatter of mode frequency, MEASURED from -3 dB peak widths
// of the reference (32768-pt mean spectrum): ear mode 2514 Hz 2.9 % wide
// -> sd 1.25 %; cap/shell partial 6659 Hz 0.87 % -> sd 0.37 % (FWHM /
// 2.355, assuming the width is scatter, not damping: Q from the measured
// decay would give < 0.01 %). Ears are set by the slot, which varies most
// (Bergum: slot length sets pitch).
inline constexpr double kSleighBellEarScatter = 0.0125;
inline constexpr double kSleighBellShellScatter = 0.0037;
// Hand motion. Shaking is a sequence of point-to-point strokes; each stroke
// follows the minimum-jerk profile x = A (10 s^3 - 15 s^4 + 6 s^5), s = t/T
// (T. Flash and N. Hogan, "The coordination of arm movements", J.
// Neurosci. 5, 1688-1703, 1985). Up stroke, pause, down stroke, pause.
// Stroke share of the cycle MEASURED from the reference: frames within
// 10 dB of each cycle's peak = 0.52 of the cycle.
inline constexpr double kSleighBellStrokeDuty = 0.52;

struct ShakeGesture {
  double rateHz = kSleighBellShakeHz;
  double amplitudeM = kSleighBellStrokeAmplitudeM;  // use kSleighBellShakeAmplitudeM for the sinusoid
  double lateralFraction = 0.3;  // sideways share of the hand motion, UNSOURCED
  bool minimumJerkStrokes = true; // false: plain sinusoid (pro1)
  double strokeDuty = kSleighBellStrokeDuty;
};
// Hand acceleration (m/s^2) at time t: vertical, and the lateral component.
[[nodiscard]] inline double shakeAcceleration(const ShakeGesture& g, double t, bool lateral) {
  const double w = 6.283185307179586 * g.rateHz;
  if (!g.minimumJerkStrokes)
    return lateral ? -g.lateralFraction * g.amplitudeM * w * w * std::cos(w * t)
                   : -g.amplitudeM * w * w * std::sin(w * t);
  const double period = 1.0 / g.rateHz, T = 0.5 * g.strokeDuty * period;
  double ph = std::fmod(t + (lateral ? 0.25 * period : 0.0), period);
  if (ph < 0) ph += period;
  double sign = 1.0;
  if (ph >= 0.5 * period) { ph -= 0.5 * period; sign = -1.0; }
  if (ph >= T) return 0.0;  // pause
  const double s = ph / T, A = g.amplitudeM * (lateral ? g.lateralFraction : 1.0);
  return sign * A / (T * T) * (60.0 * s - 180.0 * s * s + 120.0 * s * s * s);
}
// --- gesture timeline ---------------------------------------------------------
// A performance is a list of events on the strap's clock:
//  Shake  - minimum-jerk strokes (ShakeGesture shape) from timeSec for
//           durationSec, stroke amplitude ramping linearly from strength
//           to endStrength (metres). Below ~10 mm the pellets stay down.
//  Strike - a hit: the hand swings down and is stopped hard (on the palm,
//           knee or a table). timeSec is the stop. Shell velocity goes
//           0 -> -V over kSleighBellStrikeSwingS (half-sine acceleration),
//           then -V -> 0 over kSleighBellStrikeStopS. strength = V (m/s).
//           The swing lifts the pellets (peak pi V / (2 T) > g for V above
//           ~0.4 m/s) so they meet the cap first (the soft pre-hit heard
//           ~40 ms before each accent in the reference) and slam the bottom
//           at the stop. lateral = sideways share, signed.
// The hand is still outside events, so the pellets settle.
struct SleighBellEvent {
  enum class Kind { Shake, Strike };
  Kind kind = Kind::Strike;
  double timeSec = 0.0;
  double durationSec = 0.0;  // Shake only
  double strength = 0.0;     // Strike: V (m/s); Shake: stroke amplitude (m)
  double endStrength = -1.0; // Shake: amplitude at the end (<0: same as strength)
  double lateral = 0.3;      // sideways share of the motion, UNSOURCED
};
inline constexpr double kSleighBellStrikeSwingS = 0.06;  // down-swing time, UNSOURCED
inline constexpr double kSleighBellStrikeStopS = 0.004;  // hand stopped on palm/knee, UNSOURCED
// Strike velocity at the 0 dB accent of the reference: TUNED (level match);
// hand speeds of ~1 m/s are typical of fast arm strokes (VERIFY).
inline constexpr double kSleighBellStrikeRefSpeed = 1.2;
[[nodiscard]] inline double strikeSpeedForDb(double db) { return kSleighBellStrikeRefSpeed * std::pow(10.0, db / 20.0); }

inline void gestureEventAcceleration(const SleighBellEvent& e, const ShakeGesture& shape, double t, double& az,
                                     double& ax) {
  az = ax = 0.0;
  constexpr double kPi = 3.141592653589793;
  if (e.kind == SleighBellEvent::Kind::Strike) {
    const double t0 = e.timeSec - kSleighBellStrikeStopS - kSleighBellStrikeSwingS;
    double a = 0.0;
    if (t >= t0 && t < e.timeSec - kSleighBellStrikeStopS)
      a = -kPi * e.strength / (2.0 * kSleighBellStrikeSwingS) * std::sin(kPi * (t - t0) / kSleighBellStrikeSwingS);
    else if (t >= e.timeSec - kSleighBellStrikeStopS && t < e.timeSec)
      a = kPi * e.strength / (2.0 * kSleighBellStrikeStopS) *
          std::sin(kPi * (t - e.timeSec + kSleighBellStrikeStopS) / kSleighBellStrikeStopS);
    az = a;
    ax = e.lateral * a;
    return;
  }
  if (t < e.timeSec || t >= e.timeSec + e.durationSec || e.durationSec <= 0.0) return;
  const double u = (t - e.timeSec) / e.durationSec;
  const double endA = e.endStrength < 0.0 ? e.strength : e.endStrength;
  ShakeGesture g = shape;
  g.amplitudeM = e.strength + (endA - e.strength) * u;
  g.lateralFraction = e.lateral;
  // strokes are phased from the event start so a shake begins at rest
  az = shakeAcceleration(g, t - e.timeSec, false);
  ax = shakeAcceleration(g, t - e.timeSec, true);
}

// Text form, one event per line ('#' starts a comment):
//   strike <timeSec> <speed m/s> [lateral]
//   shake  <timeSec> <durationSec> <amplitude m> [endAmplitude m] [lateral]
[[nodiscard]] inline std::vector<SleighBellEvent> parseSleighBellTimeline(const std::string& text) {
  std::vector<SleighBellEvent> out;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const auto hash = line.find('#');
    if (hash != std::string::npos) line.resize(hash);
    std::istringstream f(line);
    std::string kind;
    if (!(f >> kind)) continue;
    SleighBellEvent e;
    if (kind == "strike") {
      e.kind = SleighBellEvent::Kind::Strike;
      if (!(f >> e.timeSec >> e.strength)) continue;
      f >> e.lateral;
    } else if (kind == "shake") {
      e.kind = SleighBellEvent::Kind::Shake;
      if (!(f >> e.timeSec >> e.durationSec >> e.strength)) continue;
      if (f >> e.endStrength) f >> e.lateral;
    } else {
      continue;
    }
    out.push_back(e);
  }
  return out;
}

// Contact effective mass. A hit on an ear (thin cantilever) moves only the
// ear: tip effective mass of a cantilever = 33/140 of its mass (Rayleigh,
// static-deflection shape; e.g. Blevins, "Formulas for natural frequency
// and mode shape", 1979). Hits elsewhere use the whole shell mass. Ear
// width and the ear zone (top cap, n_z > 0.6) are UNSOURCED.
inline constexpr double kSleighBellEarWidthM = 0.008;
inline constexpr double kSleighBellEarZoneCos = 0.6;
inline constexpr double kCantileverTipMassFraction = 33.0 / 140.0;

class SleighBellStrap {
 public:
  SleighBellStrap(int bells, std::uint64_t seed, double sampleRate, SleighBellDesign d = {})
      : fs_(sampleRate), d_(d), s_(seed ? seed : 1) {
    const double rIn = d_.outerRadiusM - d_.wallM;
    pelletMass_ = d_.pelletRho * 4.18879020479 * std::pow(d_.pelletRadiusM, 3);
    const double shellMass = d_.shellRho * 12.5663706144 * d_.outerRadiusM * d_.outerRadiusM * d_.wallM;
    mStar_ = pelletMass_ * shellMass / (pelletMass_ + shellMass);
    shellMass_ = shellMass;
    // click in the modes' units: a mode's kick is gain * J * sin(w / fs);
    // scale J / M back by M so clickGain = 1 means a click of the same
    // peak as the strongest mode's first sample at 6659 Hz
    clickScale_ = shellMass * std::sin(6.283185307179586 * 6659.0 / fs_);
    rStar_ = 1.0 / (1.0 / d_.pelletRadiusM - 1.0 / rIn);
    eStar_ = 1.0 / ((1 - d_.shellNu * d_.shellNu) / d_.shellE + (1 - d_.pelletNu * d_.pelletNu) / d_.pelletE);
    // ear length from the measured fundamental (TUNED), second ear mode from theory
    earLengthM_ = cantileverLengthForHz(kSleighBellMeasuredPartials[0].hz, d_.shellE, d_.shellRho, d_.wallM);
    const double earTip = kCantileverTipMassFraction * d_.shellRho * earLengthM_ * kSleighBellEarWidthM * d_.wallM;
    mStarEar_ = pelletMass_ * earTip / (pelletMass_ + earTip);
    // shell-shell contact: two equal convex spheres
    eShellShell_ = d_.shellE / (2.0 * (1.0 - d_.shellNu * d_.shellNu));
    for (int b = 0; b < bells; ++b) {
      Bell bell;
      const double earScale = 1.0 + kSleighBellEarScatter * gauss(), shellScale = 1.0 + kSleighBellShellScatter * gauss();
      const SleighBellPartial* table = d_.fullRing ? kSleighBellMeasuredPartialsFull.data() : kSleighBellMeasuredPartials.data();
      const std::size_t count = d_.fullRing ? kSleighBellMeasuredPartialsFull.size() : kSleighBellMeasuredPartials.size();
      for (std::size_t k = 0; k < count; ++k) {
        const auto& p = table[k];
        double f = p.hz;
        if (p.earMode == 1) f = cantileverStripHz(0, d_.shellE, d_.shellRho, d_.wallM, earLengthM_);
        f /= p.earMode ? earScale : shellScale;
        Mode m;
        m.w = 6.283185307179586 * f;
        const double t60 = 60.0 / (p.decayDbPerS * (d_.dampingFactor > 0.0 ? d_.dampingFactor : kSleighBellStrapDampingFactor));
        m.r = std::exp(-6.907755278982137 / (t60 * fs_));
        m.c = 2.0 * m.r * std::cos(m.w / fs_);
        m.gain = std::pow(10.0, p.levelDb / 20.0);
        bell.modes.push_back(m);
      }
      const double phi = 6.283185307179586 * uniform();
      bell.p = {0.3 * rIn * std::cos(phi), 0.3 * rIn * std::sin(phi), -0.5 * rIn};
      bell.lag = d_.strapLagS * gauss();  // strap: small timing offsets between bells
      bell.strapW = 6.283185307179586 * d_.strapHz * std::max(0.2, 1.0 + d_.strapHzScatter * gauss());
      bells_.push_back(bell);
    }
  }
  [[nodiscard]] double earLengthM() const noexcept { return earLengthM_; }
  [[nodiscard]] std::uint64_t impacts() const noexcept { return impacts_; }
  [[nodiscard]] std::uint64_t knocks() const noexcept { return knocks_; }

  // Render `n` samples of a steady shake, starting at the current time.
  void render(double* out, std::size_t n, const ShakeGesture& g) {
    renderWith(out, n, [&g](double tt, double& az, double& ax) {
      az = shakeAcceleration(g, tt, false);
      ax = shakeAcceleration(g, tt, true);
    });
  }
  // Render `n` samples driven by a gesture timeline (shake/strike events,
  // times on this strap's clock, which starts at 0 and advances with every
  // render call). Between and after events the hand is still and the
  // pellets settle under gravity and restitution alone.
  void render(double* out, std::size_t n, const std::vector<SleighBellEvent>& timeline,
              const ShakeGesture& shape = {}) {
    renderWith(out, n, [&](double tt, double& az, double& ax) {
      az = ax = 0.0;
      for (const SleighBellEvent& e : timeline) {
        double z = 0.0, x = 0.0;
        gestureEventAcceleration(e, shape, tt, z, x);
        az += z; ax += x;
      }
    });
  }

 private:
  struct Mode { double w = 0, r = 0, c = 0, gain = 1, y1 = 0, y2 = 0; };
  struct Bell {
    std::vector<Mode> modes;
    std::array<double, 3> p{}, v{};
    double lag = 0, strapW = 0, az = 0, ax = 0;
    std::array<double, 2> u{}, du{};  // strap displacement (z, x) re the hand
  };
  void kick(Bell& b, double J, double tau, double& click) {
    click += J / shellMass_;
    for (Mode& md : b.modes) md.y1 += md.gain * J * halfSinePulseWeight(md.w, tau) * std::sin(md.w / fs_);
  }
  template <class Accel>
  void renderWith(double* out, std::size_t n, Accel&& accel) {
    const double dt = 1.0 / fs_, rIn = d_.outerRadiusM - d_.wallM, lim = rIn - d_.pelletRadiusM;
    for (std::size_t i = 0; i < n; ++i, t_ += dt) {
      double y = 0.0, click = 0.0;
      // 1. shell accelerations: the hand, through each bell's strap spring
      for (Bell& b : bells_) {
        double az = 0.0, ax = 0.0;
        accel(t_ + b.lag, az, ax);
        if (b.strapW > 0.0) {
          const double w2 = b.strapW * b.strapW, c = 2.0 * d_.strapZeta * b.strapW;
          const double uz = -az - w2 * b.u[0] - c * b.du[0], ux = -ax - w2 * b.u[1] - c * b.du[1];
          b.du[0] += uz * dt; b.du[1] += ux * dt;
          b.u[0] += b.du[0] * dt; b.u[1] += b.du[1] * dt;
          az += uz; ax += ux;  // absolute shell acceleration
        }
        b.az = az; b.ax = ax;
      }
      // 2. neighbour knocks along the strap (lateral axis)
      for (std::size_t k = 0; k + 1 < bells_.size() && d_.strapHz > 0.0; ++k) {
        Bell &l = bells_[k], &r = bells_[k + 1];
        const double gap = d_.bellGapM + r.u[1] - l.u[1], vc = l.du[1] - r.du[1];
        if (gap < 0.0 && vc > 0.0) {
          const double dv = 0.5 * (1.0 + d_.bellRestitution) * vc;
          l.du[1] -= dv; r.du[1] += dv;
          l.u[1] += 0.5 * gap; r.u[1] -= 0.5 * gap;
          if (vc > 1e-3) {
            ++knocks_;
            const double J = shellMass_ * dv, tau = hertzContactSeconds(shellMass_ / 2.0, d_.outerRadiusM / 2.0, eShellShell_, vc);
            kick(l, J, tau, click); kick(r, J, tau, click);
          }
        }
      }
      // 3. pellets
      for (Bell& b : bells_) {
        b.v[0] += -b.ax * dt; b.v[2] += (-9.81 - b.az) * dt;
        for (int k = 0; k < 3; ++k) b.p[k] += b.v[k] * dt;
        const double r = std::sqrt(b.p[0] * b.p[0] + b.p[1] * b.p[1] + b.p[2] * b.p[2]);
        if (r > lim) {
          double nrm[3] = {b.p[0] / r, b.p[1] / r, b.p[2] / r};
          const double vn = b.v[0] * nrm[0] + b.v[1] * nrm[1] + b.v[2] * nrm[2];
          for (int k = 0; k < 3; ++k) b.p[k] = nrm[k] * lim;
          if (vn > 0.0) {
            double vt[3];
            for (int k = 0; k < 3; ++k) vt[k] = b.v[k] - vn * nrm[k];
            for (int k = 0; k < 3; ++k) b.v[k] = d_.tangentialKeep * vt[k] - d_.restitution * vn * nrm[k];
            const double J = pelletMass_ * (1.0 + d_.restitution) * vn;
            if (vn > 1e-3) {
              ++impacts_;
              kick(b, J, hertzContactSeconds(nrm[2] > kSleighBellEarZoneCos ? mStarEar_ : mStar_, rStar_, eStar_, vn), click);
            }
          }
        }
        for (Mode& md : b.modes) {
          const double yn = md.c * md.y1 - md.r * md.r * md.y2;
          md.y2 = md.y1; md.y1 = yn;
          y += yn;
        }
      }
      out[i] = y + d_.clickGain * clickScale_ * (click - clickPrev_);
      clickPrev_ = click;
    }
  }
  double uniform() noexcept {
    s_ ^= s_ << 13; s_ ^= s_ >> 7; s_ ^= s_ << 17;
    return (double(s_ >> 11) + 0.5) * (1.0 / 9007199254740992.0);
  }
  double gauss() noexcept { return std::sqrt(-2.0 * std::log(uniform())) * std::cos(6.283185307179586 * uniform()); }
  double fs_;
  SleighBellDesign d_;
  std::uint64_t s_;
  double pelletMass_ = 0, mStar_ = 0, mStarEar_ = 0, rStar_ = 0, eStar_ = 0, earLengthM_ = 0, t_ = 0;
  double shellMass_ = 0, clickScale_ = 0, clickPrev_ = 0;
  std::uint64_t impacts_ = 0, knocks_ = 0;
  double eShellShell_ = 0;
  std::vector<Bell> bells_;
};

}  // namespace mmathen
