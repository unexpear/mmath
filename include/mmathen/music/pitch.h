#pragma once

// M2 — pitch: frequency + symbolic spelling.
//
// Labels follow scientific pitch notation (C4 = MIDI 60 = middle C); the
// Yamaha C3 = 60 and legacy Ableton variants are NOT used here.
//
// A Pitch is a sounding frequency with an OPTIONAL spelling. Parsed labels
// ("C#4", "Db4") carry MIDI degree + spelling; ratio-transposed or
// inverted pitches are unspelled (microtonal results have no 12-TET name).
// Equality is SOUNDING equivalence (exact frequency equality): C#4 == Db4
// is true; spellings compare separately. This keeps enharmonics honest
// without pretending notation and acoustics are the same thing.
//
// Labels are 12-TET spellings at the A440 reference in this milestone;
// other tunings' spellings are deferred. music/ still never includes
// signal/ or physics/.

#include <cmath>
#include <optional>
#include <string>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

struct Pitch {
  FrequencyHz frequency{0.0};
  std::optional<int> midi;  // 12-TET degree, 0..127; nullopt = unspelled
  std::string spelling;     // "" when unspelled

  // Sounding equivalence: exact frequency equality. Enharmonics (C#4/Db4)
  // compare equal here; spellings compare separately as strings.
  bool operator==(const Pitch& o) const {
    return frequency == o.frequency;
  }
};

// Canonical sharp spelling for a MIDI degree.
[[nodiscard]] inline Result<std::string, QuantityError> midiToLabel(
    int midi, bool useSharps = true) {
  using R = Result<std::string, QuantityError>;
  if (midi < 0 || midi > 127)
    return R::failure(QuantityError::InvalidArgument);
  static const char* const sharpNames[12] = {"C",  "C#", "D",  "D#", "E", "F",
                                             "F#", "G",  "G#", "A",  "A#", "B"};
  static const char* const flatNames[12] = {"C",  "Db", "D",  "Eb", "E", "F",
                                            "Gb", "G",  "Ab", "A",  "Bb", "B"};
  const int pc = midi % 12;
  const int octave = midi / 12 - 1;
  const char* name = useSharps ? sharpNames[pc] : flatNames[pc];
  return R::success(std::string(name) + std::to_string(octave));
}

[[nodiscard]] inline Result<Pitch, QuantityError> pitchFromMidi(
    int midi, bool useSharps = true) {
  using R = Result<Pitch, QuantityError>;
  if (midi < 0 || midi > 127)
    return R::failure(QuantityError::InvalidArgument);
  auto label = midiToLabel(midi, useSharps);
  if (!label.ok()) return R::failure(label.error());
  const double f = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(Pitch{FrequencyHz{f}, midi, std::move(label).value()});
}

// Parses "C4", "C#4", "Db4", "F##3", "Gbb2", "Gx5" (x = double sharp),
// octave signed (C-1 = MIDI 0 .. G9 = MIDI 127). Rejects unknown letters,
// empty accidentals-without-octave, and out-of-range results.
[[nodiscard]] inline Result<Pitch, QuantityError> makePitch(
    const std::string& label) {
  using R = Result<Pitch, QuantityError>;
  const auto fail = [] { return R::failure(QuantityError::InvalidArgument); };
  if (label.size() < 2) return fail();
  char letter = label[0];
  if (letter >= 'a' && letter <= 'g') letter = static_cast<char>(letter - 32);
  int base = -1;
  switch (letter) {
    case 'C': base = 0; break;
    case 'D': base = 2; break;
    case 'E': base = 4; break;
    case 'F': base = 5; break;
    case 'G': base = 7; break;
    case 'A': base = 9; break;
    case 'B': base = 11; break;
    default: return fail();
  }
  std::size_t i = 1;
  int acc = 0;
  std::string accSpelling;
  while (i < label.size() && (label[i] == '#' || label[i] == 'b' ||
                              label[i] == 'x')) {
    if (label[i] == '#') {
      acc += 1;
      accSpelling += '#';
    } else if (label[i] == 'b') {
      acc -= 1;
      accSpelling += 'b';
    } else {
      acc += 2;
      accSpelling += 'x';
    }
    ++i;
  }
  if (i >= label.size()) return fail();  // no octave digits
  int octave = 0;
  bool negative = false;
  if (label[i] == '+' || label[i] == '-') {
    negative = (label[i] == '-');
    ++i;
  }
  if (i >= label.size()) return fail();
  for (; i < label.size(); ++i) {
    if (label[i] < '0' || label[i] > '9') return fail();
    octave = octave * 10 + (label[i] - '0');
    if (octave > 20) return fail();  // far outside MIDI, reject early
  }
  if (negative) octave = -octave;
  const int midi = (octave + 1) * 12 + base + acc;
  if (midi < 0 || midi > 127) return fail();
  const double f = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  std::string spelling;
  spelling += static_cast<char>(letter);
  spelling += accSpelling;
  spelling += std::to_string(octave);
  return R::success(Pitch{FrequencyHz{f}, midi, std::move(spelling)});
}

// Unspelled pitch from a known-valid frequency (microtonal results).
[[nodiscard]] inline Result<Pitch, QuantityError> unspelledPitch(
    FrequencyHz f) {
  using R = Result<Pitch, QuantityError>;
  if (!std::isfinite(f.value()) || !(f.value() > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(Pitch{f, std::nullopt, ""});
}

// Grid transposition: MIDI shift with sharp respelling. Unspelled pitches
// cannot shift grids (DomainError); out-of-range results are InvalidArgument.
[[nodiscard]] inline Result<Pitch, QuantityError> transposeSemitones(
    const Pitch& p, int semitones) {
  using R = Result<Pitch, QuantityError>;
  if (!p.midi.has_value())
    return R::failure(QuantityError::DomainError);
  // Promoted before adding (audit F1); pitchFromMidi range-checks after.
  const long long shifted = static_cast<long long>(*p.midi) + semitones;
  if (shifted < 0 || shifted > 127)
    return R::failure(QuantityError::InvalidArgument);
  return pitchFromMidi(static_cast<int>(shifted), true);
}

// Ratio transposition: f' = f * r. Result is unspelled (it may leave any
// grid); non-positive/non-finite outcomes are DomainError.
[[nodiscard]] inline Result<Pitch, QuantityError> transposeRatio(
    const Pitch& p, FrequencyRatio r) {
  using R = Result<Pitch, QuantityError>;
  const double fv = p.frequency.value();
  const double rv = r.value();
  if (!std::isfinite(fv) || !std::isfinite(rv))
    return R::failure(QuantityError::InvalidArgument);
  const double f = fv * rv;
  if (!(f > 0.0) || !std::isfinite(f))
    return R::failure(QuantityError::DomainError);
  return R::success(Pitch{FrequencyHz{f}, std::nullopt, ""});
}

// Frequency inversion about an axis: f' = axis^2 / f (unspelled).
[[nodiscard]] inline Result<Pitch, QuantityError> invertPitch(
    const Pitch& p, FrequencyHz axis) {
  using R = Result<Pitch, QuantityError>;
  const double fv = p.frequency.value();
  const double av = axis.value();
  if (!std::isfinite(fv) || !std::isfinite(av))
    return R::failure(QuantityError::InvalidArgument);
  if (!(fv > 0.0) || !(av > 0.0))
    return R::failure(QuantityError::DomainError);
  const double f = av * av / fv;
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(Pitch{FrequencyHz{f}, std::nullopt, ""});
}

}  // namespace mmathen
