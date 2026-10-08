#pragma once

// Electric guitar — six ideal strings, E2 A2 D3 G3 B3 E4 (docs/48).
//
// Not the classical guitar and not the desktop chord recognizer. The
// scale is 25.5 in, the length D'Addario's tension charts use for
// guitar ("Guitar = 25 1/2 in"). Fender prints the same 25.5 in and a
// parenthetical 648 mm; that millimetre is a rounding and is not L.
// L is 25.5 times the international inch, 0.0254 m. No body length is
// printed with that scale, so none is stored. Nut width, fingerboard
// radius, and fret count are not a fret table.
//
// The courses are D'Addario EXL110, 10-46 regular light. The product
// page prints a tension in pounds for each item and note, and does not
// print unit weight, so linear density is an argument.
// tuneElectricGuitarString accepts it only when T = mu (2 L f)^2 agrees
// with the printed pounds within one printed thousandth of a pound.
// The pound-force is the defined avoirdupois pound times standard
// gravity, not D'Addario's rounded "multiply by 4.45". Fender's
// .009-.042 set is a different gauge and is not these six tensions.
// No pickup inductance, turns, or magnet strength is printed, and none
// is stored here. A pluck of a tuned course is PluckedStringVoice: the
// listen fraction is the caller's position comb, and CoilVelocity is
// the string velocity at that node.

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

#include "instrument.h"

namespace mmathen {

inline constexpr std::size_t kElectricGuitarStrings = 6;
inline constexpr double kElectricGuitarScaleInches = 25.5;
// International inch, the same defined 0.0254 m used for the drum catalog.
inline constexpr double kElectricGuitarInchMeters = 0.0254;
// 1 lbf = 0.45359237 kg * 9.80665 m/s^2.
inline constexpr double kAvoirdupoisPoundKg = 0.45359237;
inline constexpr double kStandardGravityMetersPerSecondSquared = 9.80665;
inline constexpr double kPoundForceNewtons =
    kAvoirdupoisPoundKg * kStandardGravityMetersPerSecondSquared;
// EXL110 tensions are printed to a thousandth of a pound.
inline constexpr double kElectricGuitarTensionUnitPounds = 0.001;

// One EXL110 row, stored low to high. The product chart lists the high
// E first. tensionPounds is the printed chart value. The item number is
// the chart's item, not a diameter used to invent linear density.
struct ElectricGuitarString {
  const char* item;
  const char* label;
  double tensionPounds;
};

struct ElectricGuitar {
  const char* name;
  DistanceMeters vibratingLength;
  const char* scaleSource;
  std::array<ElectricGuitarString, kElectricGuitarStrings> strings;
  PlayingCompass compass;
  const char* stringSource;
};

inline constexpr const char* kElectricGuitarStringSource =
    "D'Addario EXL110, 10-46 Regular Light, XL Nickel Wound, string "
    "tension chart: PL010 E4 16.200 lb, PL013 B3 15.400 lb, PL017 G3 "
    "16.600 lb, NW026 D3 18.400 lb, NW036 A2 19.000 lb, NW046 E2 "
    "16.900 lb. "
    "https://www.daddario.com/products/"
    "exl110-xl-nickel-wound-electric-guitar-strings-regular-light-10-46/ "
    ". The page does not print unit weight. tuneElectricGuitarString "
    "takes linear density as an argument and keeps it only when the "
    "ideal-string tension agrees with the printed pounds within "
    "0.001 lb.";

inline constexpr const char* kElectricGuitarScaleSource =
    "D'Addario Catalog Supplement, String Tension Specifications: the "
    "guitar tension charts are calculated at 25 1/2 in. Fender prints "
    "25.5 in (648 mm) for a Stratocaster scale; 648 mm is that "
    "parenthetical rounding and is not the vibrating length. The length "
    "used here is 25.5 times the international inch 0.0254 m. No body "
    "length is printed with the scale, and none is stored. Fender's "
    ".009-.042 string gauge is not the EXL110 set. No pickup constant "
    "is printed, and no pickup model is stored.";

namespace electric_guitar_detail {

[[nodiscard]] inline const std::array<ElectricGuitarString,
                                      kElectricGuitarStrings>&
table() {
  static const std::array<ElectricGuitarString, kElectricGuitarStrings>
      rows{{
          ElectricGuitarString{"NW046", "E2", 16.900},
          ElectricGuitarString{"NW036", "A2", 19.000},
          ElectricGuitarString{"NW026", "D3", 18.400},
          ElectricGuitarString{"PL017", "G3", 16.600},
          ElectricGuitarString{"PL013", "B3", 15.400},
          ElectricGuitarString{"PL010", "E4", 16.200},
      }};
  return rows;
}

}  // namespace electric_guitar_detail

[[nodiscard]] inline const ElectricGuitarString* findElectricGuitarString(
    const ElectricGuitar& guitar, const char* label) {
  if (!detail::text(label)) return nullptr;
  for (const ElectricGuitarString& string : guitar.strings)
    if (string.label != nullptr && std::strcmp(string.label, label) == 0)
      return &string;
  return nullptr;
}

[[nodiscard]] inline const ElectricGuitarString*
findElectricGuitarStringByItem(const ElectricGuitar& guitar,
                               const char* item) {
  if (!detail::text(item)) return nullptr;
  for (const ElectricGuitarString& string : guitar.strings)
    if (string.item != nullptr && std::strcmp(string.item, item) == 0)
      return &string;
  return nullptr;
}

// Ideal-string course at the chart scale. linearDensityGPerM is an
// argument. The pitch is the adapter frequency of the row's note.
// frequencyPrinted is false: the chart does not print hertz. RangeError
// if T = mu (2 L f)^2 misses the printed pounds by more than 0.001 lb.
[[nodiscard]] inline Result<StringCourse, QuantityError>
tuneElectricGuitarString(const ElectricGuitar& guitar,
                         const ElectricGuitarString& string,
                         double linearDensityGPerM) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(string.label) || !detail::text(string.item))
    return R::failure(QuantityError::InvalidArgument);
  if (!detail::positiveFinite(guitar.vibratingLength.value()) ||
      !std::isfinite(string.tensionPounds) || !(string.tensionPounds > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double publishedN = string.tensionPounds * kPoundForceNewtons;
  const double unitN = kElectricGuitarTensionUnitPounds * kPoundForceNewtons;
  if (!(publishedN > unitN)) return R::failure(QuantityError::RangeError);
  return makeStringCourseAtPitch(guitar.vibratingLength, string.label,
                                 linearDensityGPerM, linearDensityGPerM,
                                 publishedN - unitN, publishedN + unitN,
                                 kElectricGuitarStringSource);
}

[[nodiscard]] inline Result<ElectricGuitar, QuantityError> electricGuitar() {
  using R = Result<ElectricGuitar, QuantityError>;
  const auto& rows = electric_guitar_detail::table();
  const double scale = kElectricGuitarScaleInches * kElectricGuitarInchMeters;
  // 648 mm is Fender's rounded parenthetical. 650 mm is the classical
  // scale. Neither is this chart length.
  if (!detail::positiveFinite(scale) || !(scale != 0.648) ||
      !(scale != 0.650))
    return R::failure(QuantityError::RangeError);
  constexpr int kSteps[kElectricGuitarStrings - 1] = {5, 5, 5, 4, 5};
  for (std::size_t i = 0; i < rows.size(); ++i) {
    const ElectricGuitarString& string = rows[i];
    if (!detail::text(string.label) || !detail::text(string.item))
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(string.tensionPounds) || !(string.tensionPounds > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    auto pitch = makePitch(string.label);
    if (!pitch.ok()) return R::failure(pitch.error());
    if (i > 0) {
      auto previous = makePitch(rows[i - 1].label);
      if (!previous.ok()) return R::failure(previous.error());
      if (!pitch.value().midi.has_value() ||
          !previous.value().midi.has_value())
        return R::failure(QuantityError::InvalidArgument);
      if (*pitch.value().midi - *previous.value().midi != kSteps[i - 1])
        return R::failure(QuantityError::RangeError);
    }
  }
  auto low = makePitch(rows.front().label);
  auto high = makePitch(rows.back().label);
  if (!low.ok() || !high.ok()) return R::failure(QuantityError::InvalidArgument);
  return R::success(ElectricGuitar{
      "electric guitar",
      DistanceMeters{scale},
      kElectricGuitarScaleSource,
      rows,
      PlayingCompass{rows.front().label, rows.back().label,
                     low.value().frequency, high.value().frequency, 0.0, 0.0,
                     kElectricGuitarStringSource},
      kElectricGuitarStringSource});
}

[[nodiscard]] inline std::string formula(const ElectricGuitar& guitar) {
  std::string text = guitar.name == nullptr ? std::string{}
                                            : std::string{guitar.name};
  for (const ElectricGuitarString& string : guitar.strings) {
    text.push_back(' ');
    if (string.label != nullptr) text += string.label;
  }
  return text;
}

}  // namespace mmathen
