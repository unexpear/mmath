#pragma once

// Viola — four ideal strings on a 410 mm body (docs/48).
//
// Not a renamed violin. Guettler's table gives a vibrating length of
// 37.5 cm (against the violin's 32.8 cm) and a heavier, lower set:
// C3 G3 D4 A4, with its own g/m and tension ranges. The same nominal
// pitch (G3, D4, A4) is therefore a different wave speed, because
// c = 2 L f and L is longer.
//
// Body length is the MENC 16-inch standard viola, 410 mm. That chart's
// nominal string length is also 375 mm, which matches Guettler's vibrating
// length; the body length is still not substituted for it.
//
// Compass C3–C6 is HyperPhysics's approximate orchestral range. The low
// end is the open C, a fifth below the violin's open G.

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kViolaBodySource =
    "MENC nominal size chart, 16-inch standard viola: body length 410 mm, "
    "string length nominal 375 mm. The vibrating length used here is "
    "Guettler's 37.5 cm (the length his density and tension ranges were "
    "stated at), which matches that nominal string length. Body length is "
    "not the vibrating length.";

[[nodiscard]] inline Result<StringInstrument, QuantityError> viola() {
  return makeStringInstrument(StringInstrumentSpec{
      "viola",
      DistanceMeters{0.410},
      kViolaBodySource,
      DistanceMeters{0.375},
      kGuettlerBowedStringSource,
      std::array<BowedStringRow, kStringInstrumentCourses>{
          BowedStringRow{"C3", 130.8, 4.95, 6.31, 47.6, 60.7},
          BowedStringRow{"G3", 196.0, 2.20, 2.81, 47.6, 60.7},
          BowedStringRow{"D4", 293.7, 0.98, 1.25, 47.6, 60.7},
          BowedStringRow{"A4", 440.0, 0.56, 0.92, 60.6, 100.2},
      },
      "C3",
      "C6",
      130.8,
      1046.5,
      kHyperphysicsPlayingRange});
}

}  // namespace mmathen
