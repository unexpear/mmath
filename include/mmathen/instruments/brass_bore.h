#pragma once
// Measured-geometry trumpet bore as a reflection-function load (opt-in).
//
// Geometry (Bb trumpet), Eveno / Kergomard et al. bore-profile study,
// HAL hal-01309223, Sec. 3.2, after Causse's measured profile: backbore
// cone L1 = 72 mm from R1 = 1.85 mm (top angle 3.64 deg), main bore
// L2 = 1.28 m of radius R2 = 5 mm prolongated by a Bessel horn of
// Bessel horn. Bach 180S37 published specs (Conn-Selmer, bachbrass.com):
// bore .459 in (R2 = 5.83 mm), #25 leadpipe venturi .345 in (radius
// 4.38 mm), bell diameter 4-13/16 in (mouth radius 61.1 mm). Bessel flare
// nu = 0.60, inside the 0.5-0.65 range Benade measured for trumpet bells
// (Benade, "The Physics of Brasses", Sci. Am. 229(1), 1973). Cup: Bach 7C
// rim diameter 16.30 mm, hemisphere V0 = 2/3 pi Rb^3 with Rb = 8.15 mm
// (hemisphere reading UNSOURCED). FITTED, not measured (UNSOURCED as
// geometry): leadpipe venturi-to-bore taper 0.25 m, bell length 0.60 m,
// main length L2 = 1.295 m, chosen so peaks 2-8 sit within 17 cents of
// n x 116.54 Hz and the peak/trough ratio drops below 10 dB at 1.28 kHz.
// Wall loss: Keefe, JASA 75, 58-62 (1984), leading order in 1/r_v.
// Radiation: unflanged pipe (Levine and Schwinger 1948) via the Silva,
// Guillemain, Kergomard, Mallaroni and Norris rational fit, JASA 125
// (2009). Chain matrices per stepped cylinder, inverse DFT to an input
// pressure reflection function and a bell-flow transfer function.
#include <cmath>
#include <complex>
#include <vector>

namespace mmathen::brass_bore {

struct Segment { double length; double radius; };

struct Kernels {
  std::vector<double> reflection;   // p- from p+ at the backbore input
  std::vector<double> mouthFlow;    // U at the bell mouth from p+ (m^3/s per Pa)
  double throatRadius = 0.00185;
  double cupVolume = 0.0;
};

// Valves. Each pressed valve inserts a cylindrical loop of the main-bore
// radius at the valve block. Loop length L_open (2^(n/12) - 1) for n = 2,
// 1, 3 semitones (valves 1, 2, 3) is the equal-tempered first-order rule
// (Campbell and Greated, The Musician's Guide to Acoustics); measured Bach
// 37 slide lengths are UNSOURCED, so these are the rule, not a catalog.
// Valve block position 0.25 m from the input is UNSOURCED.
struct Fingering {
  bool v1 = false, v2 = false, v3 = false;
  bool trombone = false;     // Bb tenor trombone profile instead of the trumpet
  double slideMeters = 0.0;  // trombone slide extension x; adds 2x of slide-bore tube
};

// Bb tenor trombone (opt-in). Bell: Scott, "Measurement and Synthesis of
// the Trombone", SFU M.Sc. (2012), Table 2.2, Bessel horn
// a(x) = b (x + x0)^-0.7, b = 0.0063, x0 = 0.0174, length 0.502 m, mouth
// 0.108 m (as quoted in docs/48). Slide bore: Bach 42 .547 in (radius
// 6.95 mm, Conn-Selmer published spec; VERIFY). Mouthpiece: Bach 6-1/2AL
// rim 25.5 mm, throat .281 in (radius 3.57 mm) (VERIFY), cup read as a
// hemisphere of the rim radius (UNSOURCED, as for the trumpet). Backbore
// cone 3.57 to 5.5 mm over 80 mm, bell-neck taper 6.95 mm to the bell's
// 9.96 mm throat over 0.60 m: UNSOURCED. The closed-slide cylinder length
// is FITTED so impedance peak 2 sits at Bb2 116.54 Hz. Slide positions add
// 2x of the slide bore: a real tube length, x = L0 (2^(n/12) - 1) / 2.
constexpr double kTromboneThroatRadius = 0.00357;
constexpr double kTromboneBackboreMeters = 0.080;
constexpr double kTromboneRimRadius = 0.01275;
constexpr double kTromboneCylinderMeters = 1.39;  // FITTED (see above)
constexpr double kTromboneNeckMeters = 0.60;
constexpr double kTromboneBellMeters = 0.502;
constexpr double kTromboneClosedMeters =
    kTromboneBackboreMeters + kTromboneCylinderMeters + kTromboneNeckMeters + kTromboneBellMeters;
inline double tromboneSlideMeters(double semitones) {
  return 0.5 * kTromboneClosedMeters * (std::pow(2.0, semitones / 12.0) - 1.0);
}
inline std::vector<Segment> bbTromboneProfile(double& mouthRadius, double slide) {
  std::vector<Segment> s;
  const double r1 = kTromboneThroatRadius, r2 = 0.0055, rb = 0.00695, rn = 0.00996;
  for (int i = 0; i < 40; ++i)
    s.push_back({kTromboneBackboreMeters / 40, r1 + (r2 - r1) * (i + 0.5) / 40});
  s.push_back({kTromboneCylinderMeters + 2.0 * slide, rb});
  for (int i = 0; i < 40; ++i)
    s.push_back({kTromboneNeckMeters / 40, rb + (rn - rb) * (i + 0.5) / 40});
  const double b = 0.0063, x0 = 0.0174, L = kTromboneBellMeters;
  const int m = 300;
  for (int i = 0; i < m; ++i) {
    const double x = L * (1.0 - (i + 0.5) / m);
    s.push_back({L / m, b * std::pow(x + x0, -0.7)});
  }
  mouthRadius = b * std::pow(x0, -0.7);
  return s;
}
constexpr double kOpenLengthMeters = 0.072 + 1.295;
constexpr double kValveBlockMeters = 0.25;
inline double valveLoopMeters(int semitones) {
  return kOpenLengthMeters * (std::pow(2.0, semitones / 12.0) - 1.0);
}
inline std::vector<Segment> bbTrumpetProfile(double& mouthRadius, Fingering fg = {}) {
  if (fg.trombone) return bbTromboneProfile(mouthRadius, fg.slideMeters);
  std::vector<Segment> s;
  const double r1 = 0.00185, l1 = 0.072;
  const double rout = r1 + l1 * std::tan(3.64 * 0.5 * 3.14159265358979323846 / 180.0);
  for (int i = 0; i < 40; ++i) s.push_back({l1 / 40, r1 + (rout - r1) * (i + 0.5) / 40});
  const double r2 = 0.00583, venturi = 0.00438, lpl = 0.25;
  for (int i = 0; i < 20; ++i) s.push_back({lpl / 20, venturi + (r2 - venturi) * (i + 0.5) / 20});
  const double bell = 0.60, nu = 0.60;
  mouthRadius = 0.0611;
  double loop = 0.0;
  if (fg.v1) loop += valveLoopMeters(2);
  if (fg.v2) loop += valveLoopMeters(1);
  if (fg.v3) loop += valveLoopMeters(3);
  s.push_back({1.295 - bell - lpl + loop, r2});
  const double x0 = bell / (std::pow(mouthRadius / r2, 1.0 / nu) - 1.0);
  const double b = mouthRadius * std::pow(x0, nu);
  const int m = 300;
  for (int i = 0; i < m; ++i) {
    const double xm = bell * (1.0 - (i + 0.5) / m);
    s.push_back({bell / m, b * std::pow(xm + x0, -nu)});
  }
  return s;
}

// Bell radiation load. LevineSchwinger (default) is the unflanged pipe
// above. HelieRodet is the pulsating portion of a sphere: T. Helie and
// X. Rodet, Acta Acustica 89, 565-577 (2003), in the second-order
// high-pass form and Table I polynomial coefficients printed by Eveno,
// Dalmont, Causse and Gilbert, Acta Acustica 98, 158-165 (2012), eq. (4):
//   Zr/Zc = (Pa s + s^2) / (1 + 2 Px s + s^2),  s = i nu Pn,  nu = r0 f / c,
// with Pa, Px, Pn quintic polynomials in the cone half-angle theta0. The
// numerator/denominator grouping is read from the typeset equation
// (VERIFY against Helie-Rodet 2003). Bell geometry: Kausel, Mayer and
// Beauchamp, JASA 149, 2698 (2021), Bb trumpet cutoff 1630 Hz and rim
// tan(theta) = 3.23. The bell radius is b = 1.84 c / (2 pi f_cutoff), their
// cutoff relation, and r0 = b / sin(theta0) is the radius of the sphere
// whose cap meets the rim at that angle.
enum class BellRadiation { LevineSchwinger, HelieRodet };
constexpr double kKauselCutoffHz = 1630.0;
constexpr double kKauselRimTan = 3.23;
inline std::complex<double> helieRodetLoad(double f, double c) {
  using C = std::complex<double>;
  const double th = std::atan(kKauselRimTan);
  const double b = 1.84 * c / (2.0 * 3.14159265358979323846 * kKauselCutoffHz);
  const double r0 = b / std::sin(th);
  auto poly = [th](const double (&k)[6]) {
    double v = 0.0, t = 1.0;
    for (double coef : k) { v += coef * t; t *= th; }
    return v;
  };
  static constexpr double kA[6] = {0.8788, 1.0830, -1.24200, 1.1620, -0.6360, 0.1113};
  static constexpr double kX[6] = {0.7200, 0.0799, 0.22100, -0.1440, 0.0207, 0.0000};
  static constexpr double kN[6] = {-0.0220, 4.7040, -0.07946, -0.4240, 0.2607, -0.1980};
  const double pa = poly(kA), px = poly(kX), pn = poly(kN);
  const C s(0.0, r0 * f / c * pn);
  return (pa * s + s * s) / (1.0 + 2.0 * px * s + s * s);
}

// Input impedance (with optional cup), reflectance re zc, mouth flow per p+.
inline void evaluate(double f, double rho, double c, double zcRef, double cup, Fingering fg,
                     std::complex<double>* zin, std::complex<double>* refl,
                     std::complex<double>* uMouth,
                     BellRadiation bell = BellRadiation::LevineSchwinger) {
  using C = std::complex<double>;
  const double eta = 1.85e-5, pr = 0.71, gam = 1.4;
  double mouth = 0.0;
  const auto prof = bbTrumpetProfile(mouth, fg);
  const double w = 2.0 * 3.14159265358979323846 * f, k = w / c;
  const double ka = k * mouth;
  const double zm = rho * c / (3.14159265358979323846 * mouth * mouth);
  const double rm = (1 + 0.2 * ka - 0.084 * ka * ka) / (1 + 0.2 * ka + (0.5 - 0.084) * ka * ka);
  const double s2 = std::sin(2 * ka);
  const double dl = mouth * (0.6133 * (1 + 0.044 * ka * ka) / (1 + 0.19 * ka * ka) - 0.02 * s2 * s2);
  const C rc = -rm * std::exp(C(0, -2 * k * dl));
  C Z = (f > 0) ? zm * (1.0 + rc) / (1.0 - rc) : C(0, 0);
  if (bell == BellRadiation::HelieRodet && f > 0) Z = zm * helieRodetLoad(f, c);
  C A(1, 0), B(0, 0), Cc(0, 0), D(1, 0);  // total chain, input = M * mouth
  for (auto it = prof.rbegin(); it != prof.rend(); ++it) {
    const double r = it->radius;
    const double rv = r * std::sqrt(rho * std::max(w, 1e-9) / eta);
    const double a = (1 + (gam - 1) / std::sqrt(pr)) / (std::sqrt(2.0) * rv);
    const C G = C(k * a, k * (1 + a));
    const C zc = rho * c / (3.14159265358979323846 * r * r) *
                 (1.0 + C(1, -1) * (1 - (gam - 1) / std::sqrt(pr)) / (2 * std::sqrt(2.0) * rv));
    const C ch = std::cosh(G * it->length), sh = std::sinh(G * it->length);
    Z = (ch * Z + zc * sh) / (sh * Z / zc + ch);
    const C nA = ch * A + zc * sh * Cc, nB = ch * B + zc * sh * D;
    const C nC = sh / zc * A + ch * Cc, nD = sh / zc * B + ch * D;
    A = nA; B = nB; Cc = nC; D = nD;
  }
  if (refl) *refl = (Z - zcRef) / (Z + zcRef);
  if (uMouth) {
    const C R = (Z - zcRef) / (Z + zcRef);
    const C pin = 1.0 + R, uin = (1.0 - R) / zcRef;
    *uMouth = -Cc * pin + A * uin;  // det M = 1
  }
  if (zin) *zin = cup > 0 ? 1.0 / (1.0 / Z + C(0, w * cup / (rho * c * c))) : Z;
}

// Steepening length int D0 / D(x) dx, from the narrowest point of the
// mouthpipe onward (the mouthpiece backbore, 72 mm here, is excluded:
// at playing frequencies it is a lumped volume, not a traveling-wave
// section, so its p+ is the cup's). D0 is that narrowest diameter. This
// is the numerator of the brassiness potential B = (1/L_ecl) int D0/D dx
// of Myers, Pyle, Gilbert, Campbell, Chick and Logie, JASA 131, 678-688
// (2012); trumpets in 4-1/2 ft Bb measure B about 0.58-0.76 (Myers,
// Galpin Society / IBEW summary of the same method). The earlier
// throat-referred integral (r_throat / r over the whole profile, 0.407 m)
// counted the backbore's area growth as amplitude loss.
inline double steepeningLengthMeters(Fingering fg = {}) {
  double mouth = 0.0, acc = 0.0, x = 0.0;
  const auto prof = bbTrumpetProfile(mouth, fg);
  double r0 = 1.0;
  for (const auto& sg : prof) { if (x >= (fg.trombone ? kTromboneBackboreMeters : 0.072) - 1e-9) r0 = std::min(r0, sg.radius); x += sg.length; }
  x = 0.0;
  for (const auto& sg : prof) { if (x >= (fg.trombone ? kTromboneBackboreMeters : 0.072) - 1e-9) acc += sg.length * r0 / sg.radius; x += sg.length; }
  return acc;
}
// Myers brassiness B with L_ecl = c / (2 f_nominal) for the Bb (116.54 Hz)
// equivalent complete cone.
inline double myersBrassiness(double c) {
  return steepeningLengthMeters() / (c / (2.0 * 116.54));
}
// Trombone: L_ecl = c / (2 x 58.27 Hz), the Bb1 complete cone.
inline double myersTromboneBrassiness(double c) {
  Fingering fg; fg.trombone = true;
  return steepeningLengthMeters(fg) / (c / (2.0 * 58.27));
}

// Starting lip tuning for a note (Adachi and Sato 1996 Table III start
// near f_note); the per-note value comes from
// ConeValveVoice::tuneAdachiSatoLipHz.
inline double adachiSatoLipHz(double noteHz) { return noteHz; }
// Brightness target: the low-level trumpet envelope of Fletcher and
// Tarnopolsky, JASA 105, 874-881 (1999), Fig. 5 (C4 on a Bb trumpet):
// about +6 dB/octave below ~1 kHz, nearly level from 1 kHz to the
// 2-3 kHz upper transition. Returned in dB of harmonic k over h1 for a
// note at f1; above 1 kHz it is held level (the loud-playing extension
// only adds to it). A grade target beside Iowa MIS, not a filter.
inline double fletcherTarnopolskyDb(int k, double f1) {
  const double f = k * f1, knee = 1000.0;
  const double top = std::min(f, knee), base = std::min(f1, knee);
  return 6.0 * std::log2(top / base);
}

// Blowing pressure for the measured-bore phrase trumpet: 5 kPa, inside
// the 1.07-9.07 kPa Bb3-Bb5 range of Freour et al. 2010 (IRCAM,
// simultaneous pressure/flow/sound in trumpet playing). The value within
// that range is a choice.
constexpr double kMeasuredTrumpetPressurePa = 5000.0;

inline Kernels makeKernels(double fs, double rho, double c, Fingering fg = {}, std::size_t taps = 2048,
                           std::size_t n = 8192,
                           BellRadiation bell = BellRadiation::LevineSchwinger) {
  using C = std::complex<double>;
  Kernels kr;
  kr.cupVolume = 2.0 / 3.0 * 3.14159265358979323846 * std::pow(fg.trombone ? kTromboneRimRadius : 0.00815, 3);
  if (fg.trombone) kr.throatRadius = kTromboneThroatRadius;
  const double zcRef = rho * c / (3.14159265358979323846 * kr.throatRadius * kr.throatRadius);
  std::vector<C> R(n / 2 + 1), U(n / 2 + 1);
  for (std::size_t j = 0; j <= n / 2; ++j) {
    const double f = std::max(1.0, fs * j / n);
    evaluate(f, rho, c, zcRef, 0.0, fg, nullptr, &R[j], &U[j], bell);
    // Raised-cosine taper above 0.8 Nyquist keeps the kernels causal-smooth.
    const double x = double(j) / (n / 2);
    const double t = x < 0.8 ? 1.0 : 0.5 * (1 + std::cos(3.14159265358979323846 * (x - 0.8) / 0.2));
    R[j] *= t; U[j] *= t;
  }
  kr.reflection.assign(taps, 0.0);
  kr.mouthFlow.assign(taps, 0.0);
  for (std::size_t t = 0; t < taps; ++t) {
    double sr = 0, su = 0;
    for (std::size_t j = 0; j <= n / 2; ++j) {
      const double wgt = (j == 0 || j == n / 2) ? 1.0 : 2.0;
      const C e = std::exp(C(0, 2 * 3.14159265358979323846 * double(j) * t / n));
      sr += wgt * (R[j] * e).real();
      su += wgt * (U[j] * e).real();
    }
    kr.reflection[t] = sr / n;
    kr.mouthFlow[t] = su / n;
  }
  return kr;
}

// Impedance peak of the measured trombone with the slide out by
// slideMeters: the |Zin| maximum (mouthpiece cup included, as makeKernels)
// within +-6 % of guessHz, 0.05 % grid, parabolic in log|Z|.
inline double tromboneImpedancePeakHz(double slideMeters, double guessHz, double rho, double c) {
  Fingering fg; fg.trombone = true; fg.slideMeters = slideMeters;
  const double cup = 2.0 / 3.0 * 3.14159265358979323846 * std::pow(kTromboneRimRadius, 3);
  const double zcRef = rho * c / (3.14159265358979323846 * kTromboneThroatRadius * kTromboneThroatRadius);
  const int m = 240;
  std::vector<double> mag(m + 1);
  for (int i = 0; i <= m; ++i) {
    std::complex<double> z;
    evaluate(guessHz * (0.94 + 0.12 * i / m), rho, c, zcRef, cup, fg, &z, nullptr, nullptr);
    mag[i] = std::log(std::abs(z) + 1e-30);
  }
  int k = 1;
  for (int i = 1; i < m; ++i) if (mag[i] > mag[k]) k = i;
  const double den = mag[k - 1] - 2 * mag[k] + mag[k + 1];
  const double d = std::fabs(den) > 1e-18 ? 0.5 * (mag[k - 1] - mag[k + 1]) / den : 0.0;
  return guessHz * (0.94 + 0.12 * (k + d) / m);
}
// Slide extension that puts impedance peak `partial` (2 = Bb2 closed) at
// targetHz. A trombonist sets the slide by ear; positions are not stops,
// so the slide is continuous here instead of the 0-6 equal-tempered
// semitones of tromboneSlideMeters. The peak falls monotonically as the
// slide comes out; bisection over 0 to 7 semitones of slide. -1 if the
// target is outside that reach.
inline double tromboneSlideForPeakHz(double targetHz, int partial, double rho, double c) {
  const double p0 = tromboneImpedancePeakHz(0.0, partial * 58.27, rho, c);
  auto peakAt = [&](double x) {
    return tromboneImpedancePeakHz(x, p0 * kTromboneClosedMeters / (kTromboneClosedMeters + 2.0 * x), rho, c);
  };
  double lo = 0.0, hi = tromboneSlideMeters(7.0);
  if (targetHz > peakAt(lo) || targetHz < peakAt(hi)) return -1.0;
  for (int it = 0; it < 30; ++it) {
    const double mid = 0.5 * (lo + hi);
    if (peakAt(mid) > targetHz) lo = mid; else hi = mid;
  }
  return 0.5 * (lo + hi);
}

}  // namespace mmathen::brass_bore
