// M2 — pitch label tests (Catch2, test-only dependency).

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <cmath>
#include <string>

#include <mmathen/math/comparison.h>
#include <mmathen/music/pitch.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("label parsing pins MIDI degrees", "[pitch]") {
  CHECK(unwrap(makePitch("C4")).midi == 60);
  CHECK(unwrap(makePitch("A4")).midi == 69);
  CHECK(unwrap(makePitch("C-1")).midi == 0);
  CHECK(unwrap(makePitch("G9")).midi == 127);
  CHECK(unwrap(makePitch("c4")).spelling == "C4");  // case-insensitive letter
  CHECK(unwrap(makePitch("F##3")).midi == 55);      // F3=53, + 2 = G3
  CHECK(unwrap(makePitch("Gbb2")).midi == 41);      // G - 2 = F2
  CHECK(unwrap(makePitch("Gx5")).midi == 81);       // G + 2 = A5
}

TEST_CASE("parsed frequencies match 12-TET anchors", "[pitch]") {
  CHECK(unwrap(makePitch("A4")).frequency == FrequencyHz{440.0});
  CHECK(nearlyEqual(unwrap(makePitch("C4")).frequency.value(),
                    261.6255653005986, 1e-9, 1e-12));
}

TEST_CASE("enharmonics sound equal, spell different", "[pitch]") {
  const Pitch sharp = unwrap(makePitch("C#4"));
  const Pitch flat = unwrap(makePitch("Db4"));
  CHECK(sharp == flat);  // sounding equivalence
  CHECK(sharp.frequency == flat.frequency);
  CHECK(sharp.spelling != flat.spelling);
  CHECK(sharp.midi == flat.midi);
}

TEST_CASE("invalid labels are rejected", "[pitch]") {
  CHECK(makePitch("").error() == QuantityError::InvalidArgument);
  CHECK(makePitch("H4").error() == QuantityError::InvalidArgument);
  CHECK(makePitch("C#").error() == QuantityError::InvalidArgument);
  CHECK(makePitch("C4#").error() == QuantityError::InvalidArgument);
  CHECK(makePitch("C10").error() == QuantityError::InvalidArgument);
  CHECK(makePitch("C-2").error() == QuantityError::InvalidArgument);
  CHECK(makePitch("4C").error() == QuantityError::InvalidArgument);
}

TEST_CASE("grid transposition respells in sharps", "[pitch]") {
  const Pitch e = unwrap(transposeSemitones(unwrap(makePitch("C4")), 4));
  CHECK(e.midi == 64);
  CHECK(e.spelling == "E4");
  CHECK(nearlyEqual(e.frequency.value(), 329.6275569128699, 1e-9, 1e-12));
  CHECK(transposeSemitones(unwrap(makePitch("G9")), 1).error() ==
        QuantityError::InvalidArgument);  // past MIDI 127
  // Unspelled pitches cannot shift grids.
  const Pitch u = unwrap(unspelledPitch(FrequencyHz{660.0}));
  CHECK(transposeSemitones(u, 1).error() == QuantityError::DomainError);
  // Audit F1: extreme shifts are defined errors, not int overflow.
  CHECK(transposeSemitones(unwrap(makePitch("C4")), INT_MAX).error() ==
        QuantityError::InvalidArgument);
  CHECK(transposeSemitones(unwrap(makePitch("C4")), INT_MIN).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("ratio transposition and inversion leave the grid unspelled",
          "[pitch]") {
  const Pitch fifth = unwrap(transposeRatio(unwrap(makePitch("A4")),
                                            FrequencyRatio{3.0 / 2.0}));
  CHECK(fifth.frequency == FrequencyHz{660.0});
  CHECK(!fifth.midi.has_value());
  CHECK(fifth.spelling.empty());

  const Pitch inv = unwrap(invertPitch(unwrap(makePitch("A4")),
                                       FrequencyHz{880.0}));
  CHECK(nearlyEqual(inv.frequency.value(), 1760.0, 1e-9, 1e-12));
  const Pitch self = unwrap(invertPitch(unwrap(makePitch("A4")),
                                        FrequencyHz{440.0}));
  CHECK(self.frequency == FrequencyHz{440.0});
}
