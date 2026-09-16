// M2 — structure and transform tests (Catch2, test-only dependency).

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <cmath>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/music/pitch.h>
#include <mmathen/music/structures.h>
#include <mmathen/music/transforms.h>
#include <mmathen/music/tuning.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

NoteEvent note(const std::string& label, double start, double dur) {
  return NoteEvent{unwrap(makePitch(label)), TimePointSeconds{start},
                   DurationSeconds{dur}, AmplitudeLinear{0.25}, ""};
}

Melody scale012() {
  Melody m;
  m.notes = {note("C4", 0.0, 0.5), note("D4", 0.5, 0.5), note("E4", 1.0, 1.0)};
  m.formula = "scale012";
  return m;
}
}  // namespace

TEST_CASE("chord is root plus ratios, just vs 12-TET", "[structures]") {
  const double c4 = 261.6255653005986;
  auto just = unwrap(Chord::create(FrequencyHz{c4},
                                   {FrequencyRatio{1.0}, FrequencyRatio{1.25},
                                    FrequencyRatio{1.5}},
                                   "Cmajor-just"));
  auto tet = unwrap(Chord::create(
      FrequencyHz{c4},
      {FrequencyRatio{1.0},
       FrequencyRatio{std::pow(2.0, 4.0 / 12.0)},
       FrequencyRatio{std::pow(2.0, 7.0 / 12.0)}},
      "Cmajor-12tet"));
  const auto jf = unwrap(just.frequencies());
  const auto tf = unwrap(tet.frequencies());
  REQUIRE(jf.size() == 3);
  CHECK(jf[0] == FrequencyHz{c4});
  CHECK(jf[1] == FrequencyHz{c4 * 1.25});  // exact binary arithmetic
  CHECK(jf[2] == FrequencyHz{c4 * 1.5});
  CHECK(nearlyEqual(tf[1].value(), 329.6275569128699, 1e-9, 1e-12));
  CHECK(nearlyEqual(tf[2].value(), 391.99543598174927, 1e-9, 1e-12));
  // Same root, different systems: audibly different thirds.
  CHECK_FALSE(nearlyEqual(jf[1].value(), tf[1].value(), 1e-9, 1e-12));
  CHECK(Chord::create(FrequencyHz{c4}, {}).error() ==
        QuantityError::InvalidArgument);
  CHECK(Chord::create(FrequencyHz{0.0}, {FrequencyRatio{1.0}}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("scale resolves steps through its tuning", "[structures]") {
  auto scale = unwrap(Scale<TwelveTET>::create(TwelveTET{FrequencyHz{440.0}, 0},
                                               -9, {0, 4, 7}, "Cmajor"));
  // Root degree -9 from A440 is C; steps give C/E/G.
  CHECK(nearlyEqual(unwrap(scale.degreeFrequency(0)).value(),
                    261.6255653005986, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(scale.degreeFrequency(1)).value(),
                    329.6275569128699, 1e-9, 1e-12));
  CHECK(nearlyEqual(unwrap(scale.degreeFrequency(2)).value(),
                    391.99543598174927, 1e-9, 1e-12));
  CHECK(scale.degreeFrequency(3).error() == QuantityError::InvalidArgument);
  CHECK(scale.size() == 3);
  // Audit F1: sums leaving int range are defined errors, not overflow.
  auto big = unwrap(Scale<TwelveTET>::create(TwelveTET{FrequencyHz{440.0}, 0},
                                             INT_MAX, {1}, "big"));
  CHECK(big.degreeFrequency(0).error() == QuantityError::InvalidArgument);
}

TEST_CASE("rhythm derives onsets from cumulative durations", "[structures]") {
  const Rhythm r{TimePointSeconds{1.0},
                 {DurationSeconds{0.5}, DurationSeconds{0.75},
                  DurationSeconds{0.25}},
                 "r"};
  const auto onsets = r.onsets();
  REQUIRE(onsets.size() == 3);
  CHECK(onsets[0] == TimePointSeconds{1.0});
  CHECK(onsets[1] == TimePointSeconds{1.5});
  CHECK(onsets[2] == TimePointSeconds{2.25});
}

TEST_CASE("transpose moves roots, keeps relationships", "[transforms]") {
  auto chord = unwrap(Chord::create(FrequencyHz{440.0},
                                    {FrequencyRatio{1.0}, FrequencyRatio{1.5}},
                                    "fifth"));
  auto up = unwrap(transposeChord(chord, FrequencyRatio{2.0}));
  CHECK(up.root() == FrequencyHz{880.0});
  CHECK(up.ratios() == chord.ratios());
  CHECK(up.formula() == "T(fifth)");
  // Inputs are never mutated.
  CHECK(chord.root() == FrequencyHz{440.0});
  auto byCents = unwrap(transposeChordCents(chord, Cents{1200.0}));
  CHECK(byCents.root() == FrequencyHz{880.0});
}

TEST_CASE("melody transpose and invert preserve timing", "[transforms]") {
  const Melody m = scale012();
  auto up = unwrap(transposeMelodySemitones(m, 12));
  CHECK(up.notes[0].pitch.midi == 72);
  CHECK(up.notes[0].start == m.notes[0].start);
  CHECK(up.notes[2].duration == m.notes[2].duration);
  CHECK(up.formula == "T(scale012)");

  auto inv = unwrap(invertMelody(m, FrequencyHz{440.0}));
  CHECK(nearlyEqual(inv.notes[0].pitch.frequency.value(),
                    440.0 * 440.0 / m.notes[0].pitch.frequency.value(), 1e-9,
                    1e-9));
  CHECK(inv.notes[1].start == m.notes[1].start);
}

TEST_CASE("retrograde is an involution preserving span", "[transforms]") {
  const Melody m = scale012();
  const Melody r = retrograde(m);
  CHECK(r.notes.size() == 3);
  CHECK(r.notes[0].pitch == m.notes[2].pitch);
  CHECK(r.notes[0].start == TimePointSeconds{0.0});
  CHECK(r.notes[0].duration == DurationSeconds{1.0});
  CHECK(r.notes[2].pitch == m.notes[0].pitch);
  // Total span preserved: 0.0 .. 2.0.
  CHECK(r.notes[2].start == TimePointSeconds{1.5});
  // Involution holds on notes; provenance accumulates monotonically.
  const Melody rr = retrograde(r);
  CHECK(rr.notes == m.notes);
  CHECK(rr.formula == "R(R(scale012))");
  CHECK(retrograde(Melody{}).notes.empty());
}

TEST_CASE("stretch composes to identity", "[transforms]") {
  const Melody m = scale012();
  const auto s2 = unwrap(makeTimeStretch(2.0));
  const auto shalf = unwrap(makeTimeStretch(0.5));
  const Melody stretched = stretchMelody(stretchMelody(m, s2), shalf);
  for (std::size_t i = 0; i < m.notes.size(); ++i) {
    CHECK(nearlyEqual(stretched.notes[i].start.value(),
                      m.notes[i].start.value(), 1e-12, 1e-12));
    CHECK(stretched.notes[i].duration == m.notes[i].duration);
  }
  const Rhythm r{TimePointSeconds{0.0},
                 {DurationSeconds{0.5}, DurationSeconds{0.5}}, "r"};
  const Rhythm rs = stretchRhythm(r, s2);
  CHECK(rs.durations[0] == DurationSeconds{1.0});
  CHECK(makeTimeStretch(0.0).error() == QuantityError::InvalidArgument);
  CHECK(makeTimeStretch(-1.0).error() == QuantityError::InvalidArgument);
}

TEST_CASE("quantize snaps onsets to the grid", "[transforms]") {
  CHECK(unwrap(quantize(TimePointSeconds{0.53}, DurationSeconds{0.25},
                        TimePointSeconds{0.0})) == TimePointSeconds{0.5});
  CHECK(unwrap(quantize(TimePointSeconds{0.63}, DurationSeconds{0.25},
                        TimePointSeconds{0.0})) == TimePointSeconds{0.75});
  CHECK(quantize(TimePointSeconds{0.5}, DurationSeconds{0.0},
                 TimePointSeconds{0.0})
            .error() == QuantityError::InvalidArgument);
  Melody m = scale012();
  m.notes[1].start = TimePointSeconds{0.53};
  auto q = unwrap(quantizeMelody(m, DurationSeconds{0.25}));
  CHECK(q.notes[1].start == TimePointSeconds{0.5});
  CHECK(q.notes[1].duration == m.notes[1].duration);  // durations untouched
}
