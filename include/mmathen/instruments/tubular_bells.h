#pragma once

// Tubular bells (orchestral chimes): brass tubes hung from a cord at the
// top, struck on the top end cap with a rawhide mallet (struck_bar.h,
// tube section). The heard strike tone is a virtual pitch an octave
// below flexural mode 4, whose modes 4, 5, 6 lie near 2 : 3 : 4 of it
// (Rossing, Science of Percussion Instruments, 2000, ch. 4; Fletcher and
// Rossing, Physics of Musical Instruments, 2nd ed., sec. 19.3): the tube
// length is solved so mode 4 sits at twice the note. Constants are
// labelled at each line.

#include <cmath>

#include "struck_bar.h"

namespace mmathen {

// Brass C26000: E 110 GPa, rho 8530 kg/m^3, nu 0.34 (ASM Handbook vol. 2,
// VERIFY alloy). Tube 1.5 in outer diameter, 1/16 in wall (common chime
// stock, VERIFY maker).
inline constexpr double kChimeBrassYoungsPa = 110.0e9;   // VERIFY
inline constexpr double kChimeBrassDensity = 8530.0;     // VERIFY
inline constexpr double kChimeBrassPoisson = 0.34;       // VERIFY
inline constexpr double kChimeOuterRadiusM = 0.75 * 25.4e-3;               // VERIFY
inline constexpr double kChimeInnerRadiusM = 0.75 * 25.4e-3 - 25.4e-3 / 16.0;  // VERIFY
// Losses TUNED to the VSCO 2 CE TB_hit partial decays (Q 6300-9400 at
// modes 4-6 of C4, falling to about 2100 by 2.6 kHz; radiation damping
// is computed). The cord at the top hangs at an antinode of every mode
// and is taken as lossless (VERIFY).
inline constexpr double kChimeInternalQ = 6000.0;        // TUNED (9000 rang modes 4-5 of C4 37 / 24 s vs 26 / 27 s; 6000 gives 16-28 / 10-16 s across C4-F5)
// Mallet: rawhide-faced, 200 g head (VERIFY); face modulus TUNED
// (below), radius 20 mm (VERIFY), on the brass cap.
inline constexpr double kChimeMalletMassKg = 0.20;       // VERIFY
inline constexpr double kChimeMalletRadiusM = 20.0e-3;   // VERIFY
inline constexpr double kChimeMalletYoungsPa = 1.2e9;    // TUNED: 0.3 GPa (rawhide, VERIFY) left the centroid 1137-1588 Hz vs 1553-2018; 1.2 GPa gives 1326-2274
inline constexpr double kChimeMalletPoisson = 0.4;       // VERIFY
inline constexpr double kChimeMalletLambda = 0.3;        // s/m, TUNED

[[nodiscard]] inline BarSpec chimeTube(double noteHz) {
  BarSpec b;
  b.material = {kChimeBrassYoungsPa, kChimeBrassDensity, kChimeBrassPoisson};
  b.section.tube = true;
  b.section.outerRadiusM = kChimeOuterRadiusM;
  b.section.innerRadiusM = kChimeInnerRadiusM;
  // Mode 4 at 2 x note: bisection on length.
  double lo = 0.2, hi = 4.0;
  for (int i = 0; i < 200; ++i) {
    b.lengthM = 0.5 * (lo + hi);
    if (barFlexuralHz(b, 4) > 2.0 * noteHz) lo = b.lengthM; else hi = b.lengthM;
  }
  return b;
}
[[nodiscard]] inline BarMallet chimeMallet() {
  BarMallet m;
  m.massKg = kChimeMalletMassKg;
  m.stiffness = hertzSphereOnFlatStiffness(kChimeMalletRadiusM, kChimeMalletYoungsPa, kChimeMalletPoisson,
                                           kChimeBrassYoungsPa, kChimeBrassPoisson);
  m.exponent = 1.5;
  m.dampingPerMeterPerSecond = kChimeMalletLambda;
  return m;
}
// One chime stroke on the top cap (x = 0); mallet speed in m/s (about 1 pp
// to 5 ff, VERIFY).
[[nodiscard]] inline Result<StruckBarVoice, QuantityError> tubularBellNote(
    double noteHz, double speedMps, SampleRateHz rate, const BarMallet* mallet = nullptr) {
  BarLosses l;
  l.internalQ = kChimeInternalQ;
  l.supportDashpotNsPerM = 0.0;
  BarListener li;
  li.polarRad = constants::pi / 3.0;    // listener level with the tube's upper part (UNSOURCED)
  li.azimuthRad = 0.0;
  return StruckBarVoice::create(chimeTube(noteHz), l, mallet ? *mallet : chimeMallet(), speedMps, 0.0, 0.0,
                                rate, li);
}

}  // namespace mmathen
