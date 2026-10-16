#pragma once

// Classical guitar — six ideal strings, standard E2 A2 D3 G3 B3 E4
// (docs/48).
//
// This is not the desktop chord recognizer. A course is an ideal string:
// a vibrating length, a caller-supplied linear density, and the tension
// that tunes the pitch-adapter frequency. A pluck is ExcitationKind::Pluck.
// A fingered stop is fingerStop. No fret table is stored.
//
// Vibrating length is 650 mm. Savarez's standard-prime block prints
// 65 cm for notes E A D G B E (positions 6 to 1). Yamaha's C40II prints
// the same scale length on the row whose body length is 490 mm. The body
// is not the scale. Total length 995 mm, nut width 52 mm, and string
// spacing 11.8 mm are not stored.
//
// The Savarez block prints 82.4, 110, 146.79, 196, 246.88, and 329.6 Hz.
// 146.79 and 246.88 are not the A440 adapter's D3 and B3 within one
// printed hundredth, so tuneClassicalGuitarString does not store any of
// those hertz on the course. The block says the technology depends on
// the Savarez set and prints no diameter and no tension. Yamaha names
// the strings only as Medium. Density and tension ranges are arguments.
// D'Addario's classical tension chart is computed at 25.5 in and is not
// applied to this 650 mm scale.

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include "body.h"
#include "instrument.h"

namespace mmathen {

inline constexpr std::size_t kClassicalGuitarStrings = 6;
inline constexpr double kClassicalGuitarBodyLengthMm = 490.0;
inline constexpr double kClassicalGuitarScaleLengthMm = 650.0;

// One row of Savarez's standard-prime block, stored low to high.
// position is Savarez's string number (6 is the low E). printedFrequencyHz
// is the figure in that block. printedHertzUnit is one place of that
// printing. The course frequency is the pitch adapter, not this figure.
struct ClassicalGuitarString {
  const char* position;
  const char* label;
  double printedFrequencyHz;
  double printedHertzUnit;
};

struct ClassicalGuitar {
  const char* name;
  BodyGeometry body;
  DistanceMeters vibratingLength;
  const char* vibratingLengthSource;
  std::array<ClassicalGuitarString, kClassicalGuitarStrings> strings;
  PlayingCompass compass;
  const char* stringSource;
};

inline constexpr const char* kClassicalGuitarStringSource =
    "Savarez, Ensemble classical guitar strings, standard prime guitar, "
    "vibrating length 65 cm, notes E A D G B E at positions 6 to 1. "
    "http://www.savarez.com/sites/default/files/public/produit/plaquette/"
    "doc_guitar_densemble_eng.pdf . The block prints 82.4, 110, 146.79, "
    "196, 246.88, and 329.6 Hz. 146.79 and 246.88 are not the A440 "
    "adapter within one printed hundredth, so a tuned course does not "
    "store those hertz. Technology depends on the Savarez set; the block "
    "prints no diameter and no tension. tuneClassicalGuitarString takes "
    "both ranges as arguments.";

inline constexpr const char* kClassicalGuitarBodySource =
    "Yamaha C40II, C Series specifications, body length 490 mm. "
    "https://usa.yamaha.com/products/musical_instruments/guitars_basses/"
    "cl_guitars/c_cx/specs.html . Scale length on the same row is 650 mm "
    "and is not this body length. Total length 995 mm, nut width 52 mm, "
    "and string spacing 11.8 mm are not stored. The row names the strings "
    "only as Medium and does not print linear density or tension.";

inline constexpr const char* kClassicalGuitarScaleSource =
    "Savarez standard prime guitar, vibrating length 65 cm, and the "
    "Yamaha C40II scale length, 650 mm. Both are the open-string length. "
    "Neither is the 490 mm body. D'Addario classical tensions are stated "
    "at 25.5 in and are not used at this length.";

namespace guitar_detail {

[[nodiscard]] inline const std::array<ClassicalGuitarString,
                                      kClassicalGuitarStrings>&
table() {
  static const std::array<ClassicalGuitarString, kClassicalGuitarStrings>
      rows{{
          ClassicalGuitarString{"6", "E2", 82.4, 0.1},
          ClassicalGuitarString{"5", "A2", 110.0, 1.0},
          ClassicalGuitarString{"4", "D3", 146.79, 0.01},
          ClassicalGuitarString{"3", "G3", 196.0, 1.0},
          ClassicalGuitarString{"2", "B3", 246.88, 0.01},
          ClassicalGuitarString{"1", "E4", 329.6, 0.1},
      }};
  return rows;
}

}  // namespace guitar_detail

[[nodiscard]] inline const ClassicalGuitarString* findClassicalGuitarString(
    const ClassicalGuitar& guitar, const char* label) {
  if (!detail::text(label)) return nullptr;
  for (const ClassicalGuitarString& string : guitar.strings)
    if (string.label != nullptr && std::strcmp(string.label, label) == 0)
      return &string;
  return nullptr;
}

[[nodiscard]] inline const ClassicalGuitarString*
findClassicalGuitarStringByPosition(const ClassicalGuitar& guitar,
                                    const char* position) {
  if (!detail::text(position)) return nullptr;
  for (const ClassicalGuitarString& string : guitar.strings)
    if (string.position != nullptr &&
        std::strcmp(string.position, position) == 0)
      return &string;
  return nullptr;
}

// Ideal-string course for one open string. The length is the guitar's
// scale, not the body. Density and tension ranges are arguments.
// The course label is the row's pitch. frequencyPrinted is false.
// RangeError if T = mu (2 L f)^2 leaves the tension interval.
[[nodiscard]] inline Result<StringCourse, QuantityError>
tuneClassicalGuitarString(const ClassicalGuitar& guitar,
                          const ClassicalGuitarString& string,
                          double densityMinGPerM, double densityMaxGPerM,
                          double tensionMinN, double tensionMaxN) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(string.label) || !detail::text(string.position))
    return R::failure(QuantityError::InvalidArgument);
  if (!detail::positiveFinite(guitar.vibratingLength.value()))
    return R::failure(QuantityError::InvalidArgument);
  if (!(guitar.vibratingLength.value() != guitar.body.length.value()))
    return R::failure(QuantityError::RangeError);
  return makeStringCourseAtPitch(guitar.vibratingLength, string.label,
                                 densityMinGPerM, densityMaxGPerM,
                                 tensionMinN, tensionMaxN,
                                 kClassicalGuitarStringSource);
}

[[nodiscard]] inline Result<ClassicalGuitar, QuantityError> classicalGuitar() {
  using R = Result<ClassicalGuitar, QuantityError>;
  const auto& rows = guitar_detail::table();
  const double scale = kClassicalGuitarScaleLengthMm * 1.0e-3;
  const double body = kClassicalGuitarBodyLengthMm * 1.0e-3;
  if (!detail::positiveFinite(scale) || !detail::positiveFinite(body) ||
      !(scale != body))
    return R::failure(QuantityError::RangeError);
  // Open-string steps, low to high: three fourths, a major third, a fourth.
  // 12-TET semitones. Not a fret spacing.
  constexpr int kSteps[kClassicalGuitarStrings - 1] = {5, 5, 5, 4, 5};
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const ClassicalGuitarString& string = rows[i];
    if (!detail::text(string.label) || !detail::text(string.position))
      return R::failure(QuantityError::InvalidArgument);
    if (!(string.printedHertzUnit > 0.0) ||
        !std::isfinite(string.printedFrequencyHz) ||
        !(string.printedFrequencyHz > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    auto pitch = makePitch(string.label);
    if (!pitch.ok()) return R::failure(pitch.error());
    if (i > 0) {
      auto previous = makePitch(rows[i - 1].label);
      if (!previous.ok()) return R::failure(previous.error());
      if (!pitch.value().midi.has_value() || !previous.value().midi.has_value())
        return R::failure(QuantityError::InvalidArgument);
      if (*pitch.value().midi - *previous.value().midi != kSteps[i - 1])
        return R::failure(QuantityError::RangeError);
    }
  }
  auto low = makePitch(rows.front().label);
  auto high = makePitch(rows.back().label);
  if (!low.ok() || !high.ok()) return R::failure(QuantityError::InvalidArgument);
  // Only the printed endpoints fall inside one printed unit of the adapter.
  // D3 and B3 do not, and those figures are not rewritten to make them.
  if (!tabulatedHertzAgrees(low.value().frequency.value(),
                            rows.front().printedFrequencyHz,
                            rows.front().printedHertzUnit) ||
      !tabulatedHertzAgrees(high.value().frequency.value(),
                            rows.back().printedFrequencyHz,
                            rows.back().printedHertzUnit))
    return R::failure(QuantityError::RangeError);
  return R::success(ClassicalGuitar{
      "classical guitar",
      BodyGeometry{DistanceMeters{body}, kClassicalGuitarBodySource},
      DistanceMeters{scale},
      kClassicalGuitarScaleSource,
      rows,
      PlayingCompass{rows.front().label, rows.back().label,
                     low.value().frequency, high.value().frequency,
                     rows.front().printedFrequencyHz,
                     rows.back().printedFrequencyHz, kClassicalGuitarStringSource},
      kClassicalGuitarStringSource});
}

// Classical guitar body: ten modes measured at the bridge.
// Source: V. Debut and J. Antunes, "Physical synthesis of six-string guitar
// plucks using the Udwadia-Kalaba modal formulation", J. Acoust. Soc. Am.
// (2020), Table I, open preprint
// https://hal.science/hal-03604430v1/file/UK_JASA_Preprint.pdf.
// The table gives f_n (Hz), zeta_n (%), and the modal mass m_n (kg) at the
// bridge. Q = 1/(2 zeta) is the standard relation for light damping, so
// zeta = 1.5 % gives Q = 33.3. These are the modes of one measured
// instrument, not of the Yamaha C40II whose scale is stored above.
// Per-mode radiation: see kClassicalGuitarBodyModeAreasM2 below.
struct DebutAntunesGuitarMode {
  double frequencyHz;
  double dampingPercent;
  double modalMassKg;
};
inline constexpr std::array<DebutAntunesGuitarMode, 10> kDebutAntunes2020GuitarBody{{
    {89.4, 1.5, 0.40},  {182.6, 1.82, 0.08}, {229.9, 0.64, 0.20}, {310.2, 0.89, 0.47},
    {403.5, 0.55, 1.06}, {511.4, 0.75, 0.58}, {544.2, 0.53, 1.82}, {613.9, 0.61, 0.69},
    {669.8, 0.69, 0.38}, {848.4, 0.59, 0.81},
}};

// The table above as BodyModeSpec rows (Q = 1/(2 zeta)).
[[nodiscard]] inline std::array<BodyModeSpec, 10> classicalGuitarBodyModes() {
  std::array<BodyModeSpec, 10> modes{};
  for (std::size_t i = 0; i < modes.size(); ++i) {
    const auto& row = kDebutAntunes2020GuitarBody[i];
    modes[i] = BodyModeSpec{row.frequencyHz, 1.0 / (2.0 * row.dampingPercent * 1.0e-2),
                            row.modalMassKg};
  }
  return modes;
}

// Per-mode monopole effective area A_n (m^2, signed) for the ten modes
// above, for SoundboardVoice's per-mode overload: U = sum A_n v_n.
// Sources (both Cardiff PhD theses, 2015, CC BY-NC-ND, same guitar BR2):
//  I. A. Perry, "Sound radiation measurements on guitars and other
//   stringed musical instruments": monopole source strength Gamma_00
//   (volume velocity per bridge force, m^3 s^-1 N^-1) of BR2 at the bridge,
//   T(1,1)1 = 8.4e-3 (text p.106-107), T(3,1) = 0.345e-3 at 446 Hz
//   (Table 5.1, p.137); T(1,1)2 ~7e-3, T(1,2)2 ~3e-3, T(2,1) no monopole
//   peak (~0.3e-3) read off Fig. 5.7 (p.104), log axis, VERIFY +-20 %.
//  W. H. Roberts, "Physical modelling of the guitar and banjo": BR2 modal
//   f0, Q, M_eff, Table 4.3 (p.72); T(1,1)1 and T(1,1)2 of opposite
//   polarity (p.120); statistical monopole limits +-0.04 m^2 for
//   0.5-1.5 kHz, Table 7.1 (p.125).
// At resonance v = F Q/(M w), so A = Gamma_00 * M w / Q (BR2 values):
//   T(1,1)1 91.4 Hz, Q 45, M 0.292 kg -> A = 8.4e-3 * 3.73  = 0.031 m^2
//   T(1,1)2 178.6 Hz, Q 46, M 0.102   -> A = 7e-3   * 2.49  = 0.017
//   T(2,1)  206.5 Hz, Q 56, M 0.423   -> A = 0.3e-3 * 9.80  = 0.003
//   T(1,2)2 411.3 Hz, Q 45, M 0.319   -> A = 3e-3   * 18.3  = 0.055
// Modes above 500 Hz: rms of Roberts' uniform +-0.04 m^2 = 0.04/sqrt(3)
// = 0.023 m^2, sign positive (Roberts draws the sign at random; VERIFY).
// Mapping Debut-Antunes modes to BR2 mode names by frequency order is
// VERIFY (different guitars). Mode 4 (310.2 Hz) has no BR2 counterpart
// with a usable Gamma_00 (BR2 289 Hz has M_eff 2.7 kg, the bridge sits near
// a node), so it keeps the previous 0.02 m^2: UNSOURCED.
inline constexpr std::array<double, 10> kClassicalGuitarBodyModeAreasM2{{
    -0.031, 0.017, 0.003, 0.020, 0.055, 0.023, 0.023, 0.023, 0.023, 0.023,
}};

[[nodiscard]] inline std::string formula(const ClassicalGuitar& guitar) {
  std::string text = guitar.name == nullptr ? std::string{}
                                            : std::string{guitar.name};
  for (const ClassicalGuitarString& string : guitar.strings) {
    text.push_back(' ');
    if (string.label != nullptr) text += string.label;
  }
  return text;
}

}  // namespace mmathen
