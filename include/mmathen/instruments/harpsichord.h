#pragma once

// French double harpsichord — Goermans / Taskin, Paris 1764 / 1783-4
// (docs/48). Musical Instrument Museums Edinburgh, accession 4329.
//
// Disposition and compass are the catalogue's: two manuals, 61 notes
// F1–F6, two 8' choirs and one 4' choir. Peau de buffle is a jack
// register on the lower 8', and the buff is a batten on the upper 8'.
// Neither is another choir, and neither is a hammer, a reed, or a bow.
// A jack plucks; the pluck of a tuned course is ExcitationKind::Pluck.
//
// Case length 2365 mm is Bray Baroque 2011, Table 1, for this Edinburgh
// instrument. The museum Measurements field prints 2365 and does not
// name the unit; the millimetre case length is Bray's. Speaking lengths
// are not that case length.
//
// The catalogue prints eleven long-8' speaking lengths and their
// plucking points. Bray's long-8' column matches the notes both sources
// print (F, C, c, c', c'' = F1, C2, C3, C4, C5). The 4' lengths at
// c, c', and c'' are Bray's Table 1. They are not half the 8'. The
// short 8' speaking lengths are in neither source and are not invented.
// Gauge numbers are stamped on the wrestplank and are not transcribed
// by the catalogue. Density and a tension interval are not printed.
// tuneHarpsichordString takes both as arguments. The catalogue does not
// print hertz, so a tuned course does not store a published frequency.
// Historical pitch is not the A440 adapter and is not stored.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>

#include "instrument.h"

namespace mmathen {

inline constexpr std::size_t kFrenchDoubleSpeakingLengths = 14;
inline constexpr int kFrenchDoubleCompassNotes = 61;
inline constexpr double kFrenchDoubleCaseLengthMm = 2365.0;

enum class HarpsichordChoir { LongEightFoot, ShortEightFoot, FourFoot };

// vibratingLengthMm and pluckingPointMm are the catalogue's millimetres.
// pluckingPoint is nullopt when that source does not print one. The
// point is not converted to a fraction: the catalogue does not say
// which end it is measured from.
struct HarpsichordSpeakingLength {
  const char* label;
  HarpsichordChoir choir;
  double vibratingLengthMm;
  std::optional<double> pluckingPointMm;
  const char* source;
};

struct FrenchDoubleCompass {
  const char* lowLabel;
  const char* highLabel;
  FrequencyHz low;
  FrequencyHz high;
  int noteCount;
  const char* source;
};

struct FrenchDouble {
  const char* name;
  DistanceMeters caseLength;
  const char* caseSource;
  int manuals;
  int eightFootChoirs;
  int fourFootChoirs;
  bool peauDeBuffle;
  bool buffStop;
  FrenchDoubleCompass compass;
  std::array<HarpsichordSpeakingLength, kFrenchDoubleSpeakingLengths> strings;
};

inline constexpr const char* kFrenchDoubleCatalogueSource =
    "Musical Instrument Museums Edinburgh, accession 4329, Jean Goermans "
    "/ Pascal Taskin, Paris 1764 / 1783-4, Russell Collection. "
    "Double-manual French harpsichord. Compass 61 notes, F1-F6. "
    "3 sets of strings: 2 x 8-ft, 1 x 4-ft. Peau de buffle on the lower "
    "manual 8-ft, buff batten on the upper manual 8-ft. String lengths "
    "(plucking points) as printed, millimetres. Gauge numbers are "
    "stamped and not transcribed. "
    "https://collections.ed.ac.uk/mimed/record/14796";

inline constexpr const char* kFrenchDoubleScalingSource =
    "Taskin Harpsichord Scalings and Stringings Revisited, Bray Baroque, "
    "2011, Table 1: 1784 Edinburgh case length 2365 mm; long 8' "
    "F 1835, C 1727, c 1207, c' 713, c'' 366 mm; 4' c 637, c' 351, "
    "c'' 169 mm. The 8' figures match the MIMEd catalogue at those "
    "notes. The short 8' is not in the table. "
    "https://www.braybaroque.ie/harps/Taskin_stringing.htm";

namespace harpsichord_detail {

[[nodiscard]] inline const std::array<HarpsichordSpeakingLength,
                                      kFrenchDoubleSpeakingLengths>&
table() {
  // Long 8' low to high, then 4' low to high. C3, C4, and C5 occur twice.
  static const std::array<HarpsichordSpeakingLength,
                          kFrenchDoubleSpeakingLengths>
      rows{{
          {"F1", HarpsichordChoir::LongEightFoot, 1835.0, 193.0,
           kFrenchDoubleCatalogueSource},
          {"C2", HarpsichordChoir::LongEightFoot, 1727.0, 174.0,
           kFrenchDoubleCatalogueSource},
          {"F2", HarpsichordChoir::LongEightFoot, 1532.0, 162.0,
           kFrenchDoubleCatalogueSource},
          {"C3", HarpsichordChoir::LongEightFoot, 1207.0, 148.0,
           kFrenchDoubleCatalogueSource},
          {"F3", HarpsichordChoir::LongEightFoot, 993.0, 138.0,
           kFrenchDoubleCatalogueSource},
          {"C4", HarpsichordChoir::LongEightFoot, 713.0, 124.0,
           kFrenchDoubleCatalogueSource},
          {"F4", HarpsichordChoir::LongEightFoot, 550.0, 113.0,
           kFrenchDoubleCatalogueSource},
          {"C5", HarpsichordChoir::LongEightFoot, 366.0, 105.0,
           kFrenchDoubleCatalogueSource},
          {"F5", HarpsichordChoir::LongEightFoot, 271.0, 98.0,
           kFrenchDoubleCatalogueSource},
          {"C6", HarpsichordChoir::LongEightFoot, 184.0, 87.0,
           kFrenchDoubleCatalogueSource},
          {"F6", HarpsichordChoir::LongEightFoot, 142.0, 80.0,
           kFrenchDoubleCatalogueSource},
          {"C3", HarpsichordChoir::FourFoot, 637.0, std::nullopt,
           kFrenchDoubleScalingSource},
          {"C4", HarpsichordChoir::FourFoot, 351.0, std::nullopt,
           kFrenchDoubleScalingSource},
          {"C5", HarpsichordChoir::FourFoot, 169.0, std::nullopt,
           kFrenchDoubleScalingSource},
      }};
  return rows;
}

}  // namespace harpsichord_detail

[[nodiscard]] inline const HarpsichordSpeakingLength* findSpeakingLength(
    const FrenchDouble& instrument, HarpsichordChoir choir,
    const char* label) {
  if (!detail::text(label)) return nullptr;
  for (const HarpsichordSpeakingLength& string : instrument.strings)
    if (string.choir == choir && string.label != nullptr &&
        std::strcmp(string.label, label) == 0)
      return &string;
  return nullptr;
}

// Ideal-string course at the A440 spelling of the catalogue's note name.
// Density and tension ranges are arguments. The course does not store a
// published hertz value. RangeError if the tension leaves the interval.
[[nodiscard]] inline Result<StringCourse, QuantityError>
tuneHarpsichordString(const HarpsichordSpeakingLength& string,
                      double densityMinGPerM, double densityMaxGPerM,
                      double tensionMinN, double tensionMaxN) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(string.label) || !detail::text(string.source))
    return R::failure(QuantityError::InvalidArgument);
  if (!(string.vibratingLengthMm > 0.0) ||
      !std::isfinite(string.vibratingLengthMm))
    return R::failure(QuantityError::InvalidArgument);
  return makeStringCourseAtPitch(
      DistanceMeters{string.vibratingLengthMm * 1.0e-3}, string.label,
      densityMinGPerM, densityMaxGPerM, tensionMinN, tensionMaxN,
      string.source);
}

[[nodiscard]] inline Result<FrenchDouble, QuantityError> frenchDouble() {
  using R = Result<FrenchDouble, QuantityError>;
  const auto& rows = harpsichord_detail::table();
  const double caseMetres = kFrenchDoubleCaseLengthMm * 1.0e-3;
  int longEight = 0;
  int shortEight = 0;
  int fourFoot = 0;
  double previousLong = 0.0;
  double previousFour = 0.0;
  for (const HarpsichordSpeakingLength& string : rows) {
    if (!detail::text(string.label) || !detail::text(string.source))
      return R::failure(QuantityError::InvalidArgument);
    if (!(string.vibratingLengthMm > 0.0) ||
        !(string.vibratingLengthMm < kFrenchDoubleCaseLengthMm))
      return R::failure(QuantityError::RangeError);
    if (string.pluckingPointMm.has_value()) {
      const double point = *string.pluckingPointMm;
      if (!(point > 0.0) || point == string.vibratingLengthMm)
        return R::failure(QuantityError::RangeError);
    }
    auto pitch = makePitch(string.label);
    if (!pitch.ok()) return R::failure(pitch.error());
    if (string.choir == HarpsichordChoir::LongEightFoot) {
      if (longEight > 0 &&
          !(pitch.value().frequency.value() > previousLong))
        return R::failure(QuantityError::RangeError);
      previousLong = pitch.value().frequency.value();
      ++longEight;
    } else if (string.choir == HarpsichordChoir::FourFoot) {
      if (fourFoot > 0 &&
          !(pitch.value().frequency.value() > previousFour))
        return R::failure(QuantityError::RangeError);
      previousFour = pitch.value().frequency.value();
      ++fourFoot;
    } else {
      ++shortEight;
    }
  }
  if (longEight != 11 || fourFoot != 3 || shortEight != 0)
    return R::failure(QuantityError::RangeError);
  // The printed 4' length is not the long 8' halved. Half a millimetre
  // is finer than either table's printing.
  for (const char* label : {"C3", "C4", "C5"}) {
    const HarpsichordSpeakingLength* eight = nullptr;
    const HarpsichordSpeakingLength* four = nullptr;
    for (const HarpsichordSpeakingLength& string : rows) {
      if (std::strcmp(string.label, label) != 0) continue;
      if (string.choir == HarpsichordChoir::LongEightFoot) eight = &string;
      if (string.choir == HarpsichordChoir::FourFoot) four = &string;
    }
    if (eight == nullptr || four == nullptr)
      return R::failure(QuantityError::RangeError);
    if (!(std::fabs(four->vibratingLengthMm - 0.5 * eight->vibratingLengthMm) >
          0.5))
      return R::failure(QuantityError::RangeError);
  }
  auto low = makePitch("F1");
  auto high = makePitch("F6");
  if (!low.ok() || !high.ok() || !low.value().midi.has_value() ||
      !high.value().midi.has_value())
    return R::failure(QuantityError::InvalidArgument);
  const int notes =
      *high.value().midi - *low.value().midi + 1;
  if (notes != kFrenchDoubleCompassNotes)
    return R::failure(QuantityError::RangeError);
  return R::success(FrenchDouble{
      "Goermans-Taskin French double",
      DistanceMeters{caseMetres},
      kFrenchDoubleScalingSource,
      2,
      2,
      1,
      true,
      true,
      FrenchDoubleCompass{"F1", "F6", low.value().frequency,
                          high.value().frequency, notes,
                          kFrenchDoubleCatalogueSource},
      rows,
  });
}

[[nodiscard]] inline std::string formula(const FrenchDouble& instrument) {
  std::string text = instrument.name == nullptr
                         ? std::string{}
                         : std::string{instrument.name};
  text.push_back(' ');
  if (instrument.compass.lowLabel != nullptr) text += instrument.compass.lowLabel;
  text.push_back(' ');
  if (instrument.compass.highLabel != nullptr)
    text += instrument.compass.highLabel;
  return text;
}

}  // namespace mmathen
