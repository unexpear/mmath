#pragma once

// Violin — four ideal strings on a 356 mm body (docs/48).
//
// Vibrating length and the density/tension ranges are one table: Guettler,
// "Some typical properties of bowed strings" (32.8 cm for every open
// string). That length is the length the table was stated at. It is not
// the MENC nominal string length (330 mm) and not Jansson's nut-to-bridge
// (327 mm); those are other conventions and are not mixed into T or μ.
//
// Body length is a different geometric object from the vibrating string.
// Jansson ch. 7 and the MENC 4/4 nominal both give 356 mm. It does not
// enter the string law.
//
// Open strings G3 D4 A4 E5 are the pitch adapter's A440 12-TET pitches.
// Guettler's printed hertz values are that table rounded to 0.1 Hz.
// Compass G3–E7 is HyperPhysics's approximate orchestral range, not a
// measured fingerboard stop. The low end is the open G.

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kViolinBodySource =
    "Erik Jansson, Acoustics for Violin and Guitar Makers, KTH, chapter 7: "
    "body length 356 mm (bridge halfway, 195 mm of 356 mm). The MENC 4/4 "
    "nominal body length is the same 356 mm. Vibrating length is Guettler's "
    "32.8 cm, not this body length.";

[[nodiscard]] inline Result<StringInstrument, QuantityError> violin() {
  return makeStringInstrument(StringInstrumentSpec{
      "violin",
      DistanceMeters{0.356},
      kViolinBodySource,
      DistanceMeters{0.328},
      kGuettlerBowedStringSource,
      std::array<BowedStringRow, kStringInstrumentCourses>{
          BowedStringRow{"G3", 196.0, 2.12, 3.09, 35.0, 51.1},
          BowedStringRow{"D4", 293.7, 0.92, 1.63, 34.3, 60.6},
          BowedStringRow{"A4", 440.0, 0.58, 0.75, 48.3, 62.7},
          BowedStringRow{"E5", 659.3, 0.38, 0.48, 71.4, 90.7},
      },
      "G3",
      "E7",
      196.0,
      2637.0,
      kHyperphysicsPlayingRange});
}

}  // namespace mmathen
