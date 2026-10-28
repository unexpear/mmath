#pragma once

// Celesta: steel bars over tuned wooden resonator boxes, struck from above
// by small felt hammers through a keyboard action (struck_bar.h with a
// resonator). Sounding range C4-C8 (a five-octave instrument sounding an
// octave above written). Constants are labelled at each line.

#include <cmath>

#include "struck_bar.h"

namespace mmathen {

// Bars: steel (as glockenspiel.h), section 30 x 5 mm (VERIFY maker).
inline constexpr double kCelestaBarWidthM = 30.0e-3;      // VERIFY
inline constexpr double kCelestaBarThicknessM = 5.0e-3;   // VERIFY
inline constexpr double kCelestaSteelYoungsPa = 200.0e9;  // VERIFY (ASM, carbon steel)
inline constexpr double kCelestaSteelDensity = 7850.0;    // VERIFY
inline constexpr double kCelestaSteelPoisson = 0.29;      // VERIFY
inline constexpr double kCelestaInternalQ = 1100.0;       // TUNED: f1 T60 median 4.3 s in the Freesound Macsat-Rd celesta (3.9-8.2 s); Q 3000 rang 8-15 s. Includes the resonator draw, whose back-reaction is not modelled
inline constexpr double kCelestaSupportDashpot = 0.6;     // N s/m, TUNED (as glockenspiel.h)
// Hammers: felt, power law F = K d^2.5 (piano felt exponent, Chaigne and
// Askenfelt, JASA 95(2), 1994); 3 g moving mass (VERIFY). K TUNED so the
// contact (about 1.5 ms) leaves the bar's mode 3 more than 25 dB under
// the fundamental, as in the reference.
inline constexpr double kCelestaHammerMassKg = 0.003;     // VERIFY
inline constexpr double kCelestaHammerExponent = 2.5;     // Chaigne-Askenfelt felt
inline constexpr double kCelestaHammerStiffness = 2.0e9;  // N/m^2.5, TUNED
inline constexpr double kCelestaHammerLambda = 0.0;
inline constexpr double kCelestaStrikeFraction = 0.5;     // centre (UNSOURCED)
// Resonators: tuned to the bar fundamental (instrument design); box Q 30
// (VERIFY: wooden resonators of tuned percussion are of order 20-50);
// mouth area 0.8 x the bar's plan area under the middle half (VERIFY);
// coupling TUNED to put the resonator's f1 level at the reference's
// fundamental-to-partial balance.
inline constexpr double kCelestaResonatorQ = 30.0;        // VERIFY
inline constexpr double kCelestaMouthFraction = 0.8;      // VERIFY
inline constexpr double kCelestaResonatorCoupling = 0.5;  // TUNED

[[nodiscard]] inline BarSpec celestaBar(double hz) {
  BarSpec b;
  b.material = {kCelestaSteelYoungsPa, kCelestaSteelDensity, kCelestaSteelPoisson};
  b.section.widthM = kCelestaBarWidthM;
  b.section.thicknessM = kCelestaBarThicknessM;
  b.lengthM = barLengthFor(b, hz);
  return b;
}
[[nodiscard]] inline BarMallet celestaHammer() {
  BarMallet m;
  m.massKg = kCelestaHammerMassKg;
  m.stiffness = kCelestaHammerStiffness;
  m.exponent = kCelestaHammerExponent;
  m.dampingPerMeterPerSecond = kCelestaHammerLambda;
  return m;
}
// One celesta note at sounding pitch hz; hammer speed in m/s (about 0.5 pp
// to 3 ff, VERIFY). coupling < 0 uses kCelestaResonatorCoupling.
[[nodiscard]] inline Result<StruckBarVoice, QuantityError> celestaNote(
    double hz, double speedMps, SampleRateHz rate, double coupling = -1.0,
    const BarMallet* hammer = nullptr) {
  using R = Result<StruckBarVoice, QuantityError>;
  const BarSpec bar = celestaBar(hz);
  BarLosses l;
  l.internalQ = kCelestaInternalQ;
  l.supportDashpotNsPerM = kCelestaSupportDashpot;
  BarListener li;
  li.polarRad = constants::pi / 6.0;   // UNSOURCED seat, as glockenspiel.h
  li.azimuthRad = constants::pi / 6.0;
  auto v = StruckBarVoice::create(bar, l, hammer ? *hammer : celestaHammer(), speedMps,
                                  kCelestaStrikeFraction, 0.0, rate, li);
  if (!v.ok()) return v;
  auto voice = std::move(v).value();
  BarResonator r;
  r.hz = hz;
  r.q = kCelestaResonatorQ;
  r.mouthAreaM2 = kCelestaMouthFraction * kCelestaBarWidthM * 0.5 * bar.lengthM;
  r.coupling = coupling >= 0.0 ? coupling : kCelestaResonatorCoupling;
  if (!voice.attachResonator(r)) return R::failure(QuantityError::InvalidArgument);
  return R::success(std::move(voice));
}

}  // namespace mmathen
