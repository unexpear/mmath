#pragma once

// Concert-grand string bank — Chabassier, Chaigne & Joly 2013, Table III
// (docs/48). Five notes, not an 88-key piano.
//
// The speaking lengths, core diameters, winding factors, tensions, and
// printed fundamentals are chabassier2013Strings(). Nothing is retyped
// here. The steel is steelPianoWireChabassier2013(). Young's 1952
// specific modulus is plain-wire inharmonicity and is not substituted
// for a length or a tension. Notes that Table III does not print are
// not invented. This is not the desktop piano panel.
//
// tunePianoString builds the ideal StringCourse from that row: mu is
// rho F pi d^2/4, tension is the printed T0, and the frequency is
// stringMode. It must sit within the D16 relative tolerance of the
// printed f0. The pitch adapter is not written over C#5 or G6.
// tunedPianoString is the hinged stiff string, pianoString, on the same
// row. A hammer strike is makeHardHammerBlow. Felt stiffness and
// exponent stay arguments. No hammer mass and no soundboard are stored.

#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../math/constants.h"
#include "../physics/materials.h"
#include "hammer.h"

namespace mmathen {

// D16 annex: every Table III fundamental is within 0.32% of the printed
// f0, and the check there is 0.5%.
inline constexpr double kChabassierFundamentalTolerance = 5.0e-3;

inline constexpr const char* kPianoStringBankSource =
    "Chabassier, Chaigne & Joly, JASA 134(1), 648-665 (2013), Table III, "
    "via chabassier2013Strings() and steelPianoWireChabassier2013(). "
    "Five concert-grand strings: D#1, C2, F3, C#5, G6. Speaking length, "
    "core diameter, winding factor, tension, and printed f0 are that "
    "table. The other piano keys are not in the table and are not "
    "invented. No soundboard and no hammer mass are stored.";

struct PianoStringBank {
  const char* name;
  std::vector<PianoStringSpec> strings;
  PlayingCompass compass;
  const char* source;
};

// Ideal course for one Table III row. Tension is the printed T0.
// Linear density is rho F A from the Chabassier steel and the row's
// core diameter and winding factor. RangeError if the ideal fundamental
// misses the printed f0 by more than kChabassierFundamentalTolerance.
[[nodiscard]] inline Result<StringCourse, QuantityError> tunePianoString(
    const PianoStringSpec& spec) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(spec.note))
    return R::failure(QuantityError::InvalidArgument);
  const WireMaterial steel = steelPianoWireChabassier2013();
  const double diameter = spec.diameter.value();
  const double factor = spec.densityFactor;
  const double rho = steel.densityKgPerM3;
  if (!detail::positiveFinite(diameter) || !detail::positiveFinite(factor) ||
      !detail::positiveFinite(rho) ||
      !detail::positiveFinite(spec.length.value()) ||
      !detail::positiveFinite(spec.tension.value()) ||
      !detail::positiveFinite(spec.publishedF0.value()))
    return R::failure(QuantityError::InvalidArgument);
  const double mu = rho * factor * constants::pi * diameter * diameter / 4.0;
  if (!detail::positiveFinite(mu))
    return R::failure(QuantityError::RangeError);
  const LinearDensityKgPerM density{mu};
  auto frequency = stringMode(spec.length, spec.tension, density, 1);
  if (!frequency.ok()) return R::failure(frequency.error());
  const double modeled = frequency.value().value();
  const double published = spec.publishedF0.value();
  if (!(std::fabs(modeled - published) <=
        kChabassierFundamentalTolerance * published))
    return R::failure(QuantityError::RangeError);
  const double gramsPerMeter = mu * 1.0e3;
  const double tension = spec.tension.value();
  return R::success(StringCourse{
      StringGeometry{spec.length},
      StringMaterial{density},
      StringTuning{spec.note, frequency.value()},
      spec.tension,
      TabulatedRange{gramsPerMeter, gramsPerMeter},
      TabulatedRange{tension, tension},
      published,
      true,
      kPianoStringBankSource});
}

// Hinged stiff string for the same row. The material is the Table III
// steel. Young's 1952 modulus is not passed in.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> tunedPianoString(
    const PianoStringSpec& spec) {
  return pianoString(spec, steelPianoWireChabassier2013());
}

[[nodiscard]] inline const PianoStringSpec* findPianoString(
    const PianoStringBank& bank, const char* note) {
  if (!detail::text(note)) return nullptr;
  for (const PianoStringSpec& string : bank.strings)
    if (string.note != nullptr && std::strcmp(string.note, note) == 0)
      return &string;
  return nullptr;
}

[[nodiscard]] inline Result<PianoStringBank, QuantityError>
concertGrandStringBank() {
  using R = Result<PianoStringBank, QuantityError>;
  auto specs = chabassier2013Strings();
  if (specs.size() != 5) return R::failure(QuantityError::InvalidArgument);
  const char* notes[5] = {"D#1", "C2", "F3", "C#5", "G6"};
  for (std::size_t i = 0; i < specs.size(); ++i) {
    if (!detail::text(specs[i].note) || std::strcmp(specs[i].note, notes[i]) != 0)
      return R::failure(QuantityError::RangeError);
    if (i > 0) {
      if (!(specs[i].length.value() < specs[i - 1].length.value()))
        return R::failure(QuantityError::RangeError);
      if (!(specs[i].semitonesFromC4 > specs[i - 1].semitonesFromC4))
        return R::failure(QuantityError::RangeError);
    }
    auto course = tunePianoString(specs[i]);
    if (!course.ok()) return R::failure(course.error());
    auto stiff = tunedPianoString(specs[i]);
    if (!stiff.ok()) return R::failure(stiff.error());
  }
  auto low = tunePianoString(specs.front());
  auto high = tunePianoString(specs.back());
  if (!low.ok() || !high.ok()) return R::failure(QuantityError::InvalidArgument);
  const double publishedLow = specs.front().publishedF0.value();
  const double publishedHigh = specs.back().publishedF0.value();
  return R::success(PianoStringBank{
      "concert grand",
      std::move(specs),
      PlayingCompass{notes[0], notes[4], low.value().tuning.frequency,
                     high.value().tuning.frequency, publishedLow, publishedHigh,
                     kPianoStringBankSource},
      kPianoStringBankSource});
}

[[nodiscard]] inline std::string formula(const PianoStringBank& bank) {
  std::string text = bank.name == nullptr ? std::string{} : std::string{bank.name};
  for (const PianoStringSpec& string : bank.strings) {
    text.push_back(' ');
    if (string.note != nullptr) text += string.note;
  }
  return text;
}

}  // namespace mmathen
