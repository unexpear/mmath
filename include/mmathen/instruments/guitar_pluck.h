#pragma once

// Classical guitar pluck and string-loss data (docs/48, guitar pro check).
//
// String loss: J. Woodhouse, "Plucked guitar transients: comparison of
// measurements and synthesis", Acta Acustica united with Acustica 90 (2004)
// 945-965, Table 1 (D'Addario Pro Arte Composites, hard tension, 0.65 m),
// as corrected by the author's errata
// https://euphonics.org/wp-content/uploads/2022/03/Guitar-II-errata.pdf.
// Loss factor of a string mode at angular frequency w, wavenumber k = w/c:
//   eta(w) = [T (etaF + etaA / w) + B etaB k^2] / (T + B k^2)
// (friction/air term etaA/w, intrinsic etaF, bending-stiffness loss etaB).
// VERIFY: the form above is the paper's equation (8) as we read it.
// Amplitude decays as exp(-pi f eta t), so T60 = ln(1000) / (pi f eta).
//
// Pluck position: "normal plucking position is between a third and a tenth
// of the string length (3 to 20 cm)"; the "round" timbre near the sound
// hole is plucked 13 cm from the bridge (C. Traube, An interdisciplinary
// study of the timbre of the classical guitar, PhD thesis, McGill 2004;
// C. Traube et al., DAFx 2005 P_104, "round" = 13 cm). Traube & Depalle,
// DAFx 2003 (dafx54), measured classical-guitar plucks at 12-14 cm.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <mmathen/instruments/instrument.h>

namespace mmathen {

struct WoodhouseGuitarString {
  double bendingStiffnessNm2;  // B
  double etaF;                 // intrinsic loss factor
  double etaB;                 // bending loss factor
  double etaAPerSecond;        // air/friction term, s^-1
};
// Index 0 = string 1 (top E4) ... index 5 = string 6 (low E2).
inline constexpr std::array<WoodhouseGuitarString, 6> kWoodhouse2004NylonStrings{{
    {130e-6, 40e-5, 2.4e-2, 1.5}, {160e-6, 40e-5, 2.0e-2, 1.2}, {310e-6, 14e-5, 2.0e-2, 1.7},
    {51e-6, 5e-5, 2.0e-2, 1.2},   {40e-6, 7e-5, 2.5e-2, 0.9},   {57e-6, 2e-5, 2.0e-2, 1.2},
}};

inline constexpr double kClassicalGuitarRoundPluckFromBridgeM = 0.13;  // Traube 2005

// Per-string tension: D'Addario EJ45 Pro-Arte Normal Tension chart
// (daddario.com product page, J4501-J4506, lb at standard pitch on a
// 25.5 in scale): E4 16.2, B3 12.0, G3 11.9, D3 15.6, A2 15.9, E2 14.2 lb.
// At fixed density T scales with L^2, so the 650 mm scale is (650 /
// 647.7)^2 = 1.007 times the chart. Linear density follows from the
// course pitch: mu = T / (2 L f)^2. (Before this, the band/guitar
// harnesses gave every string 2 g/m, i.e. 23 N on E2 and 367 N on E4.)
inline constexpr std::array<double, 6> kDAddarioEJ45TensionLb{{16.2, 12.0, 11.9, 15.6, 15.9, 14.2}};
[[nodiscard]] inline double classicalGuitarStringTensionN(int stringNumber, double scaleM = 0.650) {
  const double lbf = 4.4482216152605;  // N per lbf
  const double chartScale = 25.5 * 0.0254;
  return kDAddarioEJ45TensionLb[static_cast<std::size_t>(stringNumber - 1)] * lbf *
         (scaleM / chartScale) * (scaleM / chartScale);
}

// Woodhouse loss factor for string number 1..6 at frequency f (Hz) on a
// string with tension T (N) and linear density mu (kg/m).
[[nodiscard]] inline double woodhouseLossFactor(int stringNumber, double f, double T, double mu) {
  const auto& s = kWoodhouse2004NylonStrings[static_cast<std::size_t>(stringNumber - 1)];
  const double w = 2.0 * 3.14159265358979323846 * f, c = std::sqrt(T / mu), k = w / c;
  const double bk2 = s.bendingStiffnessNm2 * k * k;
  return (T * (s.etaF + s.etaAPerSecond / w) + bk2 * s.etaB) / (T + bk2);
}
[[nodiscard]] inline double woodhouseT60Seconds(int stringNumber, double f, double T, double mu) {
  return std::log(1000.0) / (3.14159265358979323846 * f * woodhouseLossFactor(stringNumber, f, T, mu));
}

// LossModel for a (possibly stopped) course on string 1..6, fitted through
// Woodhouse's T60 at f0 and 6 f0 (the two-point form lossFromT60 takes).
[[nodiscard]] inline Result<LossModel, QuantityError> classicalGuitarStringLoss(
    const StringCourse& c, int stringNumber) {
  using R = Result<LossModel, QuantityError>;
  if (stringNumber < 1 || stringNumber > 6) return R::failure(QuantityError::InvalidArgument);
  const double f = c.tuning.frequency.value(), T = c.tension.value(), mu = c.material.linearDensity.value();
  auto ideal = courseString(c);
  if (!ideal.ok()) return R::failure(ideal.error());
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{woodhouseT60Seconds(stringNumber, f, T, mu)},
                     FrequencyHz{6.0 * f}, DurationSeconds{woodhouseT60Seconds(stringNumber, 6.0 * f, T, mu)});
}

// Pluck point as a fraction of the vibrating length, from a distance
// measured from the bridge. Clamped to (0, 0.5].
[[nodiscard]] inline double pluckFractionFromBridge(double distanceFromBridgeM, double vibratingLengthM) {
  if (!(vibratingLengthM > 0.0) || !(distanceFromBridgeM > 0.0)) return 0.5;
  return std::min(0.5, distanceFromBridgeM / vibratingLengthM);
}

// Let-ring: a plucked string sounds until the same string is plucked again
// (or the piece ends). onsetSeconds/stringNumber are per note in time order.
// Returns each note's end time.
[[nodiscard]] inline std::vector<double> letRingEnds(const std::vector<double>& onsetSeconds,
                                                     const std::vector<int>& stringNumber, double endSeconds) {
  std::vector<double> out(onsetSeconds.size(), endSeconds);
  for (std::size_t i = 0; i < onsetSeconds.size(); ++i)
    for (std::size_t j = i + 1; j < onsetSeconds.size(); ++j)
      if (stringNumber[j] == stringNumber[i] && onsetSeconds[j] > onsetSeconds[i]) { out[i] = onsetSeconds[j]; break; }
  return out;
}

// Bass damping. Classical-guitar practice: when the bass moves to a
// different bass string, the previous bass note is damped as (or right
// after) the new one is played: thumb rest stroke onto the higher
// adjacent string, back of the thumb, or a left-hand finger, so bass notes
// do not overlap. D. Niedt, "String Damping" parts 1-4 (douglasniedt.com,
// part 3: "with this technique there is no overlap of the two notes";
// Right-Hand Thumb Technique: the thumb "frequently damps bass strings
// that, if allowed to ring, would muddy the harmony or bass line").
// Rule: a note on a bass string (4, 5, 6) ends at the next onset on any
// bass string. Treble strings keep let-ring. Which strings count as bass
// (4-6) is the usual thumb assignment (p plays 4-6), VERIFY per piece.
inline constexpr int kClassicalGuitarFirstBassString = 4;
[[nodiscard]] inline std::vector<double> letRingEndsBassDamped(const std::vector<double>& onsetSeconds,
                                                               const std::vector<int>& stringNumber, double endSeconds,
                                                               int firstBassString = kClassicalGuitarFirstBassString) {
  std::vector<double> out = letRingEnds(onsetSeconds, stringNumber, endSeconds);
  for (std::size_t i = 0; i < onsetSeconds.size(); ++i) {
    if (stringNumber[i] < firstBassString) continue;
    for (std::size_t j = i + 1; j < onsetSeconds.size(); ++j)
      if (stringNumber[j] >= firstBassString && onsetSeconds[j] > onsetSeconds[i]) { out[i] = std::min(out[i], onsetSeconds[j]); break; }
  }
  return out;
}

}  // namespace mmathen
