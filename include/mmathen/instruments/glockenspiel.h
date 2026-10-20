#pragma once

// Orchestral glockenspiel: rectangular steel bars on felt supports at the
// fundamental's nodes, struck by hard plastic mallets (struck_bar.h).
// Sounding pitch is the bar's first flexural mode; the bar length is
// solved for it with a fixed section. Constants are labelled at each line.

#include <cmath>

#include "struck_bar.h"

namespace mmathen {

// Bar steel: E 200 GPa, rho 7850 kg/m^3, nu 0.29 (carbon steel; ASM
// Handbook vol. 1, VERIFY grade). Section 1 in x 1/4 in (25.4 x 6.35 mm),
// a common orchestral bar stock (VERIFY maker). With it the top bars'
// second-mode ratio is 2.67-2.69 against 2.68-2.69 measured (VSCO G6, C7).
inline constexpr double kGlockSteelYoungsPa = 200.0e9;   // VERIFY
inline constexpr double kGlockSteelDensity = 7850.0;     // VERIFY
inline constexpr double kGlockSteelPoisson = 0.29;       // VERIFY
inline constexpr double kGlockBarWidthM = 25.4e-3;       // VERIFY
inline constexpr double kGlockBarThicknessM = 6.35e-3;   // VERIFY
// Losses, TUNED to the VSCO 2 CE "glock_medium" partial decays: the
// fundamentals' Q (3500-5800) sets Q_int; the upper partials' extra decay
// (sigma 7-21 1/s at 4.5-8.5 kHz) is the felt supports.
inline constexpr double kGlockInternalQ = 4200.0;        // TUNED
inline constexpr double kGlockSupportDashpot = 0.6;      // N s/m per support, TUNED
// Mallet: a "medium" head, radius 12.5 mm, 10 g moving mass (VERIFY).
// Head modulus TUNED to the VSCO glock_medium spectra: polycarbonate
// (2.4 GPa, contact 0.17 ms) put mode 3 21 dB over the fundamental at
// G5; 0.24 GPa (hard rubber range, VERIFY) gives a 0.43 ms contact and
// the fundamental within 4 dB of the strongest partial on every bar.
inline constexpr double kGlockMalletRadiusM = 12.5e-3;   // VERIFY
inline constexpr double kGlockMalletMassKg = 0.010;      // VERIFY
inline constexpr double kGlockMalletYoungsPa = 0.24e9;   // TUNED
inline constexpr double kGlockMalletPoisson = 0.4;       // VERIFY
inline constexpr double kGlockMalletLambda = 0.2;        // s/m, TUNED
// Players strike near the bar centre, slightly off it (UNSOURCED values).
inline constexpr double kGlockStrikeFraction = 0.45;     // UNSOURCED (0.40 put mode 2 10 dB over the G5 fundamental)
inline constexpr double kGlockStrikeOffset = 0.15;       // of the width, UNSOURCED

[[nodiscard]] inline BarSpec glockenspielBar(double hz) {
  BarSpec b;
  b.material = {kGlockSteelYoungsPa, kGlockSteelDensity, kGlockSteelPoisson};
  b.section.widthM = kGlockBarWidthM;
  b.section.thicknessM = kGlockBarThicknessM;
  b.lengthM = barLengthFor(b, hz);
  return b;
}
[[nodiscard]] inline BarLosses glockenspielLosses() {
  BarLosses l;
  l.internalQ = kGlockInternalQ;
  l.supportDashpotNsPerM = kGlockSupportDashpot;
  return l;
}
[[nodiscard]] inline BarMallet glockenspielMallet() {
  BarMallet m;
  m.massKg = kGlockMalletMassKg;
  m.stiffness = hertzSphereOnFlatStiffness(kGlockMalletRadiusM, kGlockMalletYoungsPa,
                                           kGlockMalletPoisson, kGlockSteelYoungsPa, kGlockSteelPoisson);
  m.exponent = 1.5;
  m.dampingPerMeterPerSecond = kGlockMalletLambda;
  return m;
}
// One glockenspiel note at sounding pitch hz, mallet speed in m/s
// (about 0.5 pp to 4 ff, VERIFY).
[[nodiscard]] inline Result<StruckBarVoice, QuantityError> glockenspielNote(
    double hz, double speedMps, SampleRateHz rate, const BarListener& listener = BarListener{}) {
  BarListener l = listener;
  if (l.polarRad == 0.0 && l.azimuthRad == 0.0) {  // default seat: 30 deg off the normal, 30 deg azimuth (UNSOURCED)
    l.polarRad = constants::pi / 6.0;
    l.azimuthRad = constants::pi / 6.0;
  }
  return StruckBarVoice::create(glockenspielBar(hz), glockenspielLosses(), glockenspielMallet(), speedMps,
                                kGlockStrikeFraction, kGlockStrikeOffset, rate, l);
}

}  // namespace mmathen
