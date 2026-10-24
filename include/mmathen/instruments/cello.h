#pragma once

// Cello — four ideal strings on a 755 mm body (docs/48).
//
// Not a renamed viola. Guettler's table gives a vibrating length of
// 69.0 cm (viola 37.5 cm, violin 32.8 cm) and the lower set C2 G2 D3 A3,
// with its own g/m and tension ranges. Wave speed on each open string is
// c = 2 L f at that string's pitch and this length.
//
// Body length is the MENC 4/4 nominal, 755 mm. That chart's nominal
// string length is 695 mm. The vibrating length used here is Guettler's
// 69.0 cm, the length his density and tension ranges were stated at.
// Body length is not substituted for it.
//
// Compass C2–E5 is HyperPhysics's approximate orchestral range. The low
// end is the open C, an octave below the viola's open C.

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kCelloBodySource =
    "MENC nominal size chart, 4/4 cello: body length 755 mm, string length "
    "nominal 695 mm. The vibrating length used here is Guettler's 69.0 cm "
    "(the length his density and tension ranges were stated at). Body "
    "length is not the vibrating length.";

[[nodiscard]] inline Result<StringInstrument, QuantityError> cello() {
  return makeStringInstrument(StringInstrumentSpec{
      "cello",
      DistanceMeters{0.755},
      kCelloBodySource,
      DistanceMeters{0.690},
      kGuettlerBowedStringSource,
      std::array<BowedStringRow, kStringInstrumentCourses>{
          BowedStringRow{"C2", 65.4, 14.33, 16.98, 116.7, 138.3},
          BowedStringRow{"G2", 98.0, 6.38, 7.56, 116.7, 138.3},
          BowedStringRow{"D3", 146.9, 2.94, 3.57, 121.0, 146.9},
          BowedStringRow{"A3", 220.0, 1.50, 1.92, 138.3, 177.2},
      },
      "C2",
      "E5",
      65.4,
      659.3,
      kHyperphysicsPlayingRange});
}

}  // namespace mmathen
