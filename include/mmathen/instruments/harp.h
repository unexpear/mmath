#pragma once

// Concert pedal harp — Camac Atlantide Prestige, 47 strings (docs/48).
//
// Not a folk harp. The string set is Le Carrou's Annexe A.1, measured on
// a Camac Atlantide Prestige. Strings are numbered 00 (highest) to 45
// (lowest) in that table and stored here low to high. The pitches are the
// flat-pedal position: the thesis prints note names and hertz, and the
// labels below are the A440 scientific spellings of those hertz (Cb1 is
// 30.9 Hz, Gb7 is 2960 Hz). Chadefaux's "Do b -1" / "Sol b 6" are a
// different octave count for the same hertz and are not the stored labels.
//
// The study harp did not have the pedal discs fitted, so the lengths are
// the full open lengths. Natural and sharp speaking lengths are not in
// the table and are not invented from a fork spacing.
//
// Vibrating length is the Annexe A.1 "longueur" column. Instrument height
// is Chadefaux's "1 m 86" for a Camac Atlantide Prestige. The two are
// different objects. Annexe A.2 prints further millimetre figures without
// saying which edge they measure; those figures are not used.
//
// Diameter, angle, and the "position" column are stored as printed. They
// are not a second vibrating length. Linear density is not derived from
// diameter: strings 34-45 are wound, and table 7.1's volumetric densities
// are hypotheses taken from another harp. No per-string tension interval
// is printed. tuneHarpString takes both ranges as arguments and keeps the
// ideal-string tension only when it lies inside the tension interval.
// Chadefaux's 45 N to 756 N is the spread of tensions on one Atlantide,
// not that per-string interval, and it is not applied as one.

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include "instrument.h"

namespace mmathen {

inline constexpr std::size_t kConcertHarpStrings = 47;
inline constexpr double kConcertHarpHeightM = 1.86;

enum class HarpStringMaterial { Nylon, Gut, WoundSteel };

// One row of Le Carrou's Annexe A.1. Lengths are centimetres and the
// diameter is millimetres, as printed. printedHertzUnit is one unit of
// the way that frequency was printed (1 Hz for a whole number, 0.1 Hz
// for a tenth).
struct HarpStringGeometry {
  const char* sourceNumber;
  const char* label;
  double publishedFrequencyHz;
  double printedHertzUnit;
  double vibratingLengthCm;
  double diameterMm;
  double angleDegrees;
  double positionCm;
  HarpStringMaterial material;
};

struct PedalHarp {
  const char* name;
  DistanceMeters height;
  const char* heightSource;
  std::array<HarpStringGeometry, kConcertHarpStrings> strings;  // low to high
  PlayingCompass compass;
  // Spread of the tensions Chadefaux reports for an Atlantide Prestige.
  // Not a per-string allowance.
  TabulatedRange reportedTensionSpreadN;
  const char* tensionSpreadSource;
  const char* stringSource;
};

inline constexpr const char* kConcertHarpStringSource =
    "Jean-Loic Le Carrou, Vibro-acoustique de la harpe de concert, "
    "these, Universite du Maine, 2006, Annexe A.1 (Camac Atlantide "
    "Prestige, 47 cordes, longueurs et frequences en position bemol). "
    "https://theses.hal.science/tel-00133443 . The pedal action was not "
    "fitted on the study instrument, so the lengths are the full open "
    "lengths. Nylon is 00-6, sheep gut 7-33, wound steel 34-45. The "
    "table does not print linear density or a per-string tension "
    "interval. Table 7.1 densities are hypotheses from another harp and "
    "are not used.";

inline constexpr const char* kConcertHarpHeightSource =
    "Delphine Chadefaux, Musician / instrument interaction: the case of "
    "the concert harp, these, Universite Pierre et Marie Curie, 2012, "
    "Camac Atlantide modele Prestige: hauteur 1 m 86. "
    "https://theses.hal.science/tel-00828314 . Instrument height, not a "
    "vibrating length. Le Carrou Annexe A.2's unlabelled millimetre "
    "figures are not used.";

inline constexpr const char* kConcertHarpTensionSpreadSource =
    "Chadefaux 2012, same Camac Atlantide Prestige paragraph: the "
    "string tensions are spread from 45 N to 756 N. That is the range "
    "the set occupies, not an interval each string is allowed to leave "
    "or stay inside. tuneHarpString does not read it.";

namespace harp_detail {

[[nodiscard]] inline const std::array<HarpStringGeometry, kConcertHarpStrings>&
table() {
  static const std::array<HarpStringGeometry, kConcertHarpStrings> rows{{

      HarpStringGeometry{"45", "Cb1", 30.9, 0.1, 154, 2.96, 31, 4, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"44", "Db1", 34.6, 0.1, 151.4, 2.72, 31, 7.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"43", "Eb1", 38.9, 0.1, 148, 2.72, 31, 10.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"42", "Fb1", 41.2, 0.1, 144.7, 2.44, 31, 13.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"41", "Gb1", 46.2, 0.1, 142.8, 2.3, 31, 16.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"40", "Ab1", 51.9, 0.1, 138.9, 2.2, 31, 19.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"39", "Bb1", 58.3, 0.1, 136, 2.1, 31, 22.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"38", "Cb2", 61.7, 0.1, 132.3, 2.04, 31, 25.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"37", "Db2", 69.3, 0.1, 128.8, 1.94, 31, 28.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"36", "Eb2", 77.8, 0.1, 125.4, 1.8, 31, 31.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"35", "Fb2", 82.4, 0.1, 121.8, 1.74, 31, 34.2, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"34", "Gb2", 92.5, 0.1, 117.2, 1.6, 31, 37, HarpStringMaterial::WoundSteel},
      HarpStringGeometry{"33", "Ab2", 103.8, 0.1, 113.2, 2.36, 32, 40, HarpStringMaterial::Gut},
      HarpStringGeometry{"32", "Bb2", 116.5, 0.1, 108.1, 2.2, 32, 43, HarpStringMaterial::Gut},
      HarpStringGeometry{"31", "Cb3", 123.5, 0.1, 103, 2.1, 32, 45.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"30", "Db3", 138.6, 0.1, 97.5, 1.96, 32, 48.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"29", "Eb3", 155.6, 0.1, 90.7, 1.9, 32, 51.6, HarpStringMaterial::Gut},
      HarpStringGeometry{"28", "Fb3", 164.8, 0.1, 83.8, 1.78, 32, 54.4, HarpStringMaterial::Gut},
      HarpStringGeometry{"27", "Gb3", 185.0, 0.1, 76.8, 1.68, 32, 57.2, HarpStringMaterial::Gut},
      HarpStringGeometry{"26", "Ab3", 207.7, 0.1, 70, 1.58, 32, 60, HarpStringMaterial::Gut},
      HarpStringGeometry{"25", "Bb3", 233.1, 0.1, 64.3, 1.48, 34, 62.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"24", "Cb4", 246.9, 0.1, 58.8, 1.4, 35, 65.5, HarpStringMaterial::Gut},
      HarpStringGeometry{"23", "Db4", 277.2, 0.1, 54.2, 1.32, 35, 68.2, HarpStringMaterial::Gut},
      HarpStringGeometry{"22", "Eb4", 311.1, 0.1, 50, 1.28, 35, 70.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"21", "Fb4", 329.6, 0.1, 46, 1.24, 35, 73.5, HarpStringMaterial::Gut},
      HarpStringGeometry{"20", "Gb4", 370.0, 0.1, 42.4, 1.22, 35, 76.2, HarpStringMaterial::Gut},
      HarpStringGeometry{"19", "Ab4", 415.3, 0.1, 39, 1.16, 36, 78.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"18", "Bb4", 466.2, 0.1, 36, 1.1, 36, 81.4, HarpStringMaterial::Gut},
      HarpStringGeometry{"17", "Cb5", 493.9, 0.1, 33.3, 1.06, 36, 83.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"16", "Db5", 554.4, 0.1, 31, 1.02, 36, 86.5, HarpStringMaterial::Gut},
      HarpStringGeometry{"15", "Eb5", 622.3, 0.1, 28.8, 0.98, 36, 89.4, HarpStringMaterial::Gut},
      HarpStringGeometry{"14", "Fb5", 659.3, 0.1, 26.5, 0.94, 36, 91.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"13", "Gb5", 740.0, 0.1, 24.5, 0.9, 36, 94.4, HarpStringMaterial::Gut},
      HarpStringGeometry{"12", "Ab5", 830.6, 0.1, 22.6, 0.82, 36, 96.9, HarpStringMaterial::Gut},
      HarpStringGeometry{"11", "Bb5", 932.3, 0.1, 20.9, 0.78, 36, 99.2, HarpStringMaterial::Gut},
      HarpStringGeometry{"10", "Cb6", 987.8, 0.1, 19.3, 0.72, 37, 101.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"9", "Db6", 1109, 1.0, 17.6, 0.7, 37, 104.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"8", "Eb6", 1245, 1.0, 16.3, 0.68, 37, 106.8, HarpStringMaterial::Gut},
      HarpStringGeometry{"7", "Fb6", 1319, 1.0, 15.2, 0.66, 38, 109.2, HarpStringMaterial::Gut},
      HarpStringGeometry{"6", "Gb6", 1480, 1.0, 13.8, 0.64, 40, 111.5, HarpStringMaterial::Nylon},
      HarpStringGeometry{"5", "Ab6", 1661, 1.0, 12.8, 0.62, 40, 114.2, HarpStringMaterial::Nylon},
      HarpStringGeometry{"4", "Bb6", 1865, 1.0, 11.7, 0.6, 40, 116.4, HarpStringMaterial::Nylon},
      HarpStringGeometry{"3", "Cb7", 1975, 1.0, 11.2, 0.58, 42, 118.3, HarpStringMaterial::Nylon},
      HarpStringGeometry{"2", "Db7", 2217, 1.0, 10.4, 0.56, 42, 120.3, HarpStringMaterial::Nylon},
      HarpStringGeometry{"1", "Eb7", 2489, 1.0, 9.1, 0.54, 44, 122.3, HarpStringMaterial::Nylon},
      HarpStringGeometry{"0", "Fb7", 2637, 1.0, 8.4, 0.52, 44, 124.3, HarpStringMaterial::Nylon},
      HarpStringGeometry{"00", "Gb7", 2960, 1.0, 7.7, 0.5, 44, 126.3, HarpStringMaterial::Nylon},
  }};
  return rows;
}

}  // namespace harp_detail

[[nodiscard]] inline const HarpStringGeometry* findHarpString(
    const PedalHarp& harp, const char* label) {
  if (!detail::text(label)) return nullptr;
  for (const HarpStringGeometry& string : harp.strings)
    if (string.label != nullptr && std::strcmp(string.label, label) == 0)
      return &string;
  return nullptr;
}

[[nodiscard]] inline const HarpStringGeometry* findHarpStringByNumber(
    const PedalHarp& harp, const char* sourceNumber) {
  if (!detail::text(sourceNumber)) return nullptr;
  for (const HarpStringGeometry& string : harp.strings)
    if (string.sourceNumber != nullptr &&
        std::strcmp(string.sourceNumber, sourceNumber) == 0)
      return &string;
  return nullptr;
}

// Ideal-string course for one published string. Density and tension
// ranges are arguments because Annexe A.1 does not print them.
// RangeError if the adapter frequency is outside the printed hertz unit,
// or if T = mu (2 L f)^2 leaves the tension interval.
[[nodiscard]] inline Result<StringCourse, QuantityError> tuneHarpString(
    const HarpStringGeometry& string, double densityMinGPerM,
    double densityMaxGPerM, double tensionMinN, double tensionMaxN) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(string.label) || !detail::text(string.sourceNumber))
    return R::failure(QuantityError::InvalidArgument);
  if (!(string.vibratingLengthCm > 0.0) ||
      !std::isfinite(string.vibratingLengthCm))
    return R::failure(QuantityError::InvalidArgument);
  const BowedStringRow row{string.label, string.publishedFrequencyHz,
                           densityMinGPerM, densityMaxGPerM, tensionMinN,
                           tensionMaxN};
  return makeStringCourse(DistanceMeters{string.vibratingLengthCm * 1.0e-2},
                          row, kConcertHarpStringSource,
                          string.printedHertzUnit);
}

[[nodiscard]] inline Result<PedalHarp, QuantityError> concertPedalHarp() {
  using R = Result<PedalHarp, QuantityError>;
  const auto& rows = harp_detail::table();
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const HarpStringGeometry& string = rows[i];
    if (!detail::text(string.label) || !detail::text(string.sourceNumber))
      return R::failure(QuantityError::InvalidArgument);
    if (!(string.vibratingLengthCm > 0.0) || !(string.diameterMm > 0.0) ||
        !(string.printedHertzUnit > 0.0) ||
        !std::isfinite(string.positionCm) || !std::isfinite(string.angleDegrees))
      return R::failure(QuantityError::InvalidArgument);
    if (string.positionCm == string.vibratingLengthCm)
      return R::failure(QuantityError::RangeError);
    auto pitch = makePitch(string.label);
    if (!pitch.ok()) return R::failure(pitch.error());
    if (!tabulatedHertzAgrees(pitch.value().frequency.value(),
                              string.publishedFrequencyHz,
                              string.printedHertzUnit))
      return R::failure(QuantityError::RangeError);
    if (i > 0) {
      auto previous = makePitch(rows[i - 1].label);
      if (!previous.ok()) return R::failure(previous.error());
      if (!(pitch.value().frequency.value() >
            previous.value().frequency.value()))
        return R::failure(QuantityError::RangeError);
    }
    if (i > 0 && !(string.vibratingLengthCm < rows[i - 1].vibratingLengthCm))
      return R::failure(QuantityError::RangeError);
    const double metres = string.vibratingLengthCm * 1.0e-2;
    if (!(metres < kConcertHarpHeightM))
      return R::failure(QuantityError::RangeError);
  }
  auto low = makePitch(rows.front().label);
  auto high = makePitch(rows.back().label);
  if (!low.ok() || !high.ok()) return R::failure(QuantityError::InvalidArgument);
  return R::success(PedalHarp{
      "Camac Atlantide Prestige",
      DistanceMeters{kConcertHarpHeightM},
      kConcertHarpHeightSource,
      rows,
      PlayingCompass{rows.front().label, rows.back().label,
                     low.value().frequency, high.value().frequency,
                     rows.front().publishedFrequencyHz,
                     rows.back().publishedFrequencyHz,
                     kConcertHarpStringSource},
      TabulatedRange{45.0, 756.0},
      kConcertHarpTensionSpreadSource,
      kConcertHarpStringSource});
}

[[nodiscard]] inline std::string formula(const PedalHarp& harp) {
  std::string text = harp.name == nullptr ? std::string{} : std::string{harp.name};
  text.push_back(' ');
  if (!harp.strings.empty() && harp.strings.front().label != nullptr)
    text += harp.strings.front().label;
  text.push_back(' ');
  if (!harp.strings.empty() && harp.strings.back().label != nullptr)
    text += harp.strings.back().label;
  return text;
}

}  // namespace mmathen

