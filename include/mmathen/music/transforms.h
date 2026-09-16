#pragma once

// M2 — pure musical transformations.
//
// Every op maps Structure -> Structure with fresh provenance; inputs are
// never mutated. Transposed/inverted pitches that leave the 12-TET grid are
// unspelled (see pitch.h). Quantize is mechanical onset snapping; musical
// quantization policy (duration repair, swing) is deferred.

#include <cmath>
#include <string>
#include <utility>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "pitch.h"
#include "structures.h"

namespace mmathen {

// Time-stretch factor: finite and > 0 (checked at creation).
struct TimeStretch {
  double factor = 1.0;
};

[[nodiscard]] inline Result<TimeStretch, QuantityError> makeTimeStretch(
    double factor) {
  using R = Result<TimeStretch, QuantityError>;
  if (!std::isfinite(factor) || !(factor > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(TimeStretch{factor});
}

// T_k(P): transpose a chord by ratio (root moves, relationships stay).
[[nodiscard]] inline Result<Chord, QuantityError> transposeChord(
    const Chord& chord, FrequencyRatio r) {
  using R = Result<Chord, QuantityError>;
  const double root = chord.root().value() * r.value();
  if (!std::isfinite(root) || !(root > 0.0))
    return R::failure(QuantityError::DomainError);
  return Chord::create(FrequencyHz{root}, chord.ratios(),
                       "T(" + chord.formula() + ")");
}

[[nodiscard]] inline Result<Chord, QuantityError> transposeChordCents(
    const Chord& chord, Cents cents) {
  auto r = ratio(cents);
  if (!r.ok())
    return Result<Chord, QuantityError>::failure(r.error());
  return transposeChord(chord, std::move(r).value());
}

[[nodiscard]] inline Result<NoteEvent, QuantityError> transposeNote(
    const NoteEvent& note, FrequencyRatio r) {
  using R = Result<NoteEvent, QuantityError>;
  auto p = transposeRatio(note.pitch, r);
  if (!p.ok()) return R::failure(p.error());
  NoteEvent out = note;
  out.pitch = std::move(p).value();
  return R::success(std::move(out));
}

[[nodiscard]] inline Result<NoteEvent, QuantityError> transposeNoteSemitones(
    const NoteEvent& note, int semitones) {
  using R = Result<NoteEvent, QuantityError>;
  auto p = transposeSemitones(note.pitch, semitones);
  if (!p.ok()) return R::failure(p.error());
  NoteEvent out = note;
  out.pitch = std::move(p).value();
  return R::success(std::move(out));
}

[[nodiscard]] inline Result<Melody, QuantityError> transposeMelodyRatio(
    const Melody& melody, FrequencyRatio r) {
  using R = Result<Melody, QuantityError>;
  Melody out;
  out.notes.reserve(melody.notes.size());
  for (const auto& n : melody.notes) {
    auto t = transposeNote(n, r);
    if (!t.ok()) return R::failure(t.error());
    out.notes.push_back(std::move(t).value());
  }
  out.formula = "T(" + melody.formula + ")";
  return R::success(std::move(out));
}

[[nodiscard]] inline Result<Melody, QuantityError> transposeMelodySemitones(
    const Melody& melody, int semitones) {
  using R = Result<Melody, QuantityError>;
  Melody out;
  out.notes.reserve(melody.notes.size());
  for (const auto& n : melody.notes) {
    auto t = transposeNoteSemitones(n, semitones);
    if (!t.ok()) return R::failure(t.error());
    out.notes.push_back(std::move(t).value());
  }
  out.formula = "T(" + melody.formula + ")";
  return R::success(std::move(out));
}

// I_axis(p) = axis^2 / p per note, timing untouched.
[[nodiscard]] inline Result<Melody, QuantityError> invertMelody(
    const Melody& melody, FrequencyHz axis) {
  using R = Result<Melody, QuantityError>;
  Melody out;
  out.notes.reserve(melody.notes.size());
  for (const auto& n : melody.notes) {
    auto p = invertPitch(n.pitch, axis);
    if (!p.ok()) return R::failure(p.error());
    NoteEvent m = n;
    m.pitch = std::move(p).value();
    out.notes.push_back(std::move(m));
  }
  out.formula = "I(" + melody.formula + ")";
  return R::success(std::move(out));
}

// Retrograde: durations reversed and re-laid from the original start, so the
// total span is preserved. Pitches follow their durations in reverse order.
[[nodiscard]] inline Melody retrograde(const Melody& melody) {
  Melody out;
  out.notes.reserve(melody.notes.size());
  TimePointSeconds t = melody.notes.empty()
                           ? TimePointSeconds{0.0}
                           : melody.notes.front().start;
  for (std::size_t i = melody.notes.size(); i-- > 0;) {
    NoteEvent m = melody.notes[i];
    m.start = t;
    t = TimePointSeconds{t.value() + m.duration.value()};
    out.notes.push_back(std::move(m));
  }
  out.formula = "R(" + melody.formula + ")";
  return out;
}

// Rhythmic scaling: starts (relative to rhythm start) and durations scale.
[[nodiscard]] inline Rhythm stretchRhythm(const Rhythm& rhythm,
                                          TimeStretch s) {
  Rhythm out;
  out.start = rhythm.start;
  out.durations.reserve(rhythm.durations.size());
  for (const auto& d : rhythm.durations)
    out.durations.push_back(DurationSeconds{d.value() * s.factor});
  out.formula = "S(" + rhythm.formula + ")";
  return out;
}

[[nodiscard]] inline Melody stretchMelody(const Melody& melody, TimeStretch s) {
  const TimePointSeconds anchor =
      melody.notes.empty() ? TimePointSeconds{0.0} : melody.notes.front().start;
  Melody out;
  out.notes.reserve(melody.notes.size());
  for (const auto& n : melody.notes) {
    NoteEvent m = n;
    m.start = TimePointSeconds{anchor.value() +
                               (n.start.value() - anchor.value()) * s.factor};
    m.duration = DurationSeconds{n.duration.value() * s.factor};
    out.notes.push_back(std::move(m));
  }
  out.formula = "S(" + melody.formula + ")";
  return out;
}

// Mechanical onset snap to the nearest grid multiple from origin.
// Grid must be finite and > 0; durations are untouched (documented).
[[nodiscard]] inline Result<TimePointSeconds, QuantityError> quantize(
    TimePointSeconds t, DurationSeconds grid, TimePointSeconds origin) {
  using R = Result<TimePointSeconds, QuantityError>;
  const double g = grid.value();
  if (!std::isfinite(g) || !(g > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double q = (t.value() - origin.value()) / g;
  if (!std::isfinite(q) || q >= 9.223372036854776e18 ||
      q <= -9.223372036854776e18)
    return R::failure(QuantityError::RangeError);
  const double snapped = origin.value() + std::llround(q) * g;
  if (!std::isfinite(snapped)) return R::failure(QuantityError::RangeError);
  return R::success(TimePointSeconds{snapped});
}

[[nodiscard]] inline Result<Melody, QuantityError> quantizeMelody(
    const Melody& melody, DurationSeconds grid) {
  using R = Result<Melody, QuantityError>;
  Melody out;
  out.notes.reserve(melody.notes.size());
  for (const auto& n : melody.notes) {
    auto q = quantize(n.start, grid, TimePointSeconds{0.0});
    if (!q.ok()) return R::failure(q.error());
    NoteEvent m = n;
    m.start = std::move(q).value();
    out.notes.push_back(std::move(m));
  }
  out.formula = "Q(" + melody.formula + ")";
  return R::success(std::move(out));
}

}  // namespace mmathen
