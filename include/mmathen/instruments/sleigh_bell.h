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
};
struct SleighBellPartial { double hz; double levelDb; double decayDbPerS; };
// MEASURED from the CC0 reference (see header). Levels: dB re the strongest.
// Index 0 and 6 are the ear's two bending modes; 7 and 8 (17.0, 17.69 kHz)
// are further measured top-octave peaks, decay taken as 16.8 kHz's (VERIFY).
inline constexpr std::array<SleighBellPartial, 9> kSleighBellMeasuredPartials{{
    {2514, -5.8, 40.7}, {5959, -4.7, 29.7}, {6659, 0.0, 31.9}, {7084, -2.4, 35.9},
    {10228, -0.3, 41.6}, {10686, -0.4, 47.8}, {16796, -6.5, 52.5}, {17000, -10.9, 52.5}, {17690, -7.0, 52.5},
}};
inline constexpr double kSleighBellShakeHz = 3.36;         // MEASURED: envelope-modulation peak of the reference
// Peak hand acceleration A (2 pi f)^2 must exceed g to lift the pellet: at
// 3.36 Hz that needs A > 22 mm. 25 mm is just above. UNSOURCED.
inline constexpr double kSleighBellShakeAmplitudeM = 0.025;
// Minimum-jerk strokes peak at 10/sqrt(3) A / T^2 (T = stroke time, 77 ms
// at duty 0.52): lift needs A > 10.2 mm. 15 mm is the smallest of the
// 10/15/20/25 mm sweep that lifts it and matched all 7 reference peaks. TUNED.
inline constexpr double kSleighBellStrokeAmplitudeM = 0.015;
// Free-ring decays measured on the reference are biased slow by re-hits; a
// model run with them reads slower on the same measure (x1 -> -15..-23 dB/s, x2 -> -47..-64 vs real -29..-40), and bells
// riveted to a leather strap lose energy into it. Decay rates x1.4: TUNED.
inline constexpr double kSleighBellStrapDampingFactor = 1.4;
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
    rStar_ = 1.0 / (1.0 / d_.pelletRadiusM - 1.0 / rIn);
    eStar_ = 1.0 / ((1 - d_.shellNu * d_.shellNu) / d_.shellE + (1 - d_.pelletNu * d_.pelletNu) / d_.pelletE);
    // ear length from the measured fundamental (TUNED), second ear mode from theory
    earLengthM_ = cantileverLengthForHz(kSleighBellMeasuredPartials[0].hz, d_.shellE, d_.shellRho, d_.wallM);
    const double earTip = kCantileverTipMassFraction * d_.shellRho * earLengthM_ * kSleighBellEarWidthM * d_.wallM;
    mStarEar_ = pelletMass_ * earTip / (pelletMass_ + earTip);
    for (int b = 0; b < bells; ++b) {
      Bell bell;
      const double earScale = 1.0 + kSleighBellEarScatter * gauss(), shellScale = 1.0 + kSleighBellShellScatter * gauss();
      for (std::size_t k = 0; k < kSleighBellMeasuredPartials.size(); ++k) {
        const auto& p = kSleighBellMeasuredPartials[k];
        double f = p.hz;
        if (k == 0) f = cantileverStripHz(0, d_.shellE, d_.shellRho, d_.wallM, earLengthM_);
        f /= (k == 0 || k == 6) ? earScale : shellScale;
        Mode m;
        m.w = 6.283185307179586 * f;
        const double t60 = 60.0 / (p.decayDbPerS * kSleighBellStrapDampingFactor);
        m.r = std::exp(-6.907755278982137 / (t60 * fs_));
        m.c = 2.0 * m.r * std::cos(m.w / fs_);
        m.gain = std::pow(10.0, p.levelDb / 20.0);
        bell.modes.push_back(m);
      }
      const double phi = 6.283185307179586 * uniform();
      bell.p = {0.3 * rIn * std::cos(phi), 0.3 * rIn * std::sin(phi), -0.5 * rIn};
      bell.lag = 0.01 * gauss();  // strap: small timing offsets between bells, UNSOURCED
      bells_.push_back(bell);
    }
  }
  [[nodiscard]] double earLengthM() const noexcept { return earLengthM_; }
  [[nodiscard]] std::uint64_t impacts() const noexcept { return impacts_; }

  // Render `n` samples of a steady shake, starting at the current time.
  void render(double* out, std::size_t n, const ShakeGesture& g) {
    const double dt = 1.0 / fs_, rIn = d_.outerRadiusM - d_.wallM, lim = rIn - d_.pelletRadiusM;
    for (std::size_t i = 0; i < n; ++i, t_ += dt) {
      double y = 0.0;
      for (Bell& b : bells_) {
        const double tt = t_ + b.lag;
        // shell acceleration (hand): vertical z plus lateral x
        const double az = shakeAcceleration(g, tt, false), ax = shakeAcceleration(g, tt, true);
        b.v[0] += -ax * dt; b.v[2] += (-9.81 - az) * dt;
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
              const double tau = hertzContactSeconds(nrm[2] > kSleighBellEarZoneCos ? mStarEar_ : mStar_, rStar_, eStar_, vn);
              for (std::size_t m = 0; m < b.modes.size(); ++m) {
                Mode& md = b.modes[m];
                md.y1 += md.gain * J * halfSinePulseWeight(md.w, tau) * std::sin(md.w / fs_);
              }
            }
          }
        }
        for (Mode& md : b.modes) {
          const double yn = md.c * md.y1 - md.r * md.r * md.y2;
          md.y2 = md.y1; md.y1 = yn;
          y += yn;
        }
      }
      out[i] = y;
    }
  }

 private:
  struct Mode { double w = 0, r = 0, c = 0, gain = 1, y1 = 0, y2 = 0; };
  struct Bell { std::vector<Mode> modes; std::array<double, 3> p{}, v{}; double lag = 0; };
  double uniform() noexcept {
    s_ ^= s_ << 13; s_ ^= s_ >> 7; s_ ^= s_ << 17;
    return (double(s_ >> 11) + 0.5) * (1.0 / 9007199254740992.0);
  }
  double gauss() noexcept { return std::sqrt(-2.0 * std::log(uniform())) * std::cos(6.283185307179586 * uniform()); }
  double fs_;
  SleighBellDesign d_;
  std::uint64_t s_;
  double pelletMass_ = 0, mStar_ = 0, mStarEar_ = 0, rStar_ = 0, eStar_ = 0, earLengthM_ = 0, t_ = 0;
  std::uint64_t impacts_ = 0;
  std::vector<Bell> bells_;
};

}  // namespace mmathen
