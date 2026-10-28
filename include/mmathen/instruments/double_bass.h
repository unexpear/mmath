#pragma once

// Double bass — four ideal strings on a 1110 mm body (docs/48).
//
// Orchestral tuning from Guettler's table, not the solo tuning (F#1 B1 E2
// A2) in the same table. Vibrating length 106.0 cm for every open string.
// The four courses, low to high, are E1 A1 D2 G2, ascending by fourths
// rather than by the fifths of the violin, viola, and cello. That table
// also lists a
// low B0 extension on the same length; it is not a fifth course here.
//
// Each string has its own published g/m range and tension range. Nominal
// density is the midpoint. Tension is the ideal-string tension at the
// A440 pitch of the label. Construction fails if that tension leaves the
// published interval.
//
// Body length is the MENC 3/4 standard bass, 1110 mm. That chart's nominal
// string length is 1060 mm, which matches Guettler's 106.0 cm. The 4/4
// column of the same chart is 1160 mm of body and 1100 mm of string, and
// is not used. Body length is not substituted for the vibrating length.
//
// Sounding compass E1–B3 is HyperPhysics's playing range. The low end is
// the open E. Orchestral parts are written an octave higher, so the
// practical written compass for this sounding range is E2–B4. The
// frequencies stored on the instrument are the sounding pitches.

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kDoubleBassBodySource =
    "MENC nominal size chart, 3/4 standard double bass: body length 1110 mm, "
    "string length nominal 1060 mm. The vibrating length used here is "
    "Guettler's 106.0 cm (the length his orchestra-tuning density and "
    "tension ranges were stated at), which matches that nominal string "
    "length. Body length is not the vibrating length. The 4/4 column "
    "(body 1160 mm, string 1100 mm) is a different instrument.";

inline constexpr const char* kDoubleBassCompassSource =
    "Sounding compass E1-B3: HyperPhysics, \"Playing Ranges of Instruments\", "
    "http://hyperphysics.phy-astr.gsu.edu/hbase/Music/orchins.html , "
    "E1 41.2 Hz to B3 246.9 Hz (the page cites Rossing, Science of Sound, "
    "p. 219, and White & White, p. 280). Approximate orchestral compass, "
    "not a mechanical stop. Orchestral parts are written an octave higher, "
    "so the practical written compass for this sounding range is E2-B4. "
    "The stored frequencies are the sounding pitches; the low end is the "
    "open E1.";

[[nodiscard]] inline Result<StringInstrument, QuantityError> doubleBass() {
  return makeStringInstrument(StringInstrumentSpec{
      "double bass",
      DistanceMeters{1.110},
      kDoubleBassBodySource,
      DistanceMeters{1.060},
      kGuettlerBowedStringSource,
      std::array<BowedStringRow, kStringInstrumentCourses>{
          BowedStringRow{"E1", 41.2, 37.32, 44.12, 284.7, 336.6},
          BowedStringRow{"A1", 55.0, 20.45, 23.88, 278.0, 324.7},
          BowedStringRow{"D2", 73.4, 10.96, 13.12, 265.3, 317.6},
          BowedStringRow{"G2", 98.0, 5.98, 7.25, 258.0, 312.8},
      },
      "E1",
      "B3",
      41.2,
      246.9,
      kDoubleBassCompassSource});
}

}  // namespace mmathen
