# M2 — Musical Structures + Transforms (complete)

Status: implemented · Suite: 81/81 (MSVC Release + Debug) · Probes: 10/10

Second post-MVP domain milestone. Structures are derived from math, never
foundational: chords are root + ratios, scales are tuning + root degree +
offsets, notes are events (not sounds). Pitch labels arrive here; tempo maps
stay deferred (absolute time in events).

## New headers (`include/mmathen/music/`)

| Header | Contents |
|---|---|
| `pitch.h` | `Pitch{frequency, midi?, spelling}`; `makePitch` (12-TET/A440 labels, `#/b/x/##/bb`, C-1..G9); `pitchFromMidi`, `midiToLabel`, `unspelledPitch`; `transposeSemitones` (respells sharps), `transposeRatio`, `invertPitch` (axis²/f, unspelled) |
| `structures.h` | `NoteEvent`, `Chord::create` + `frequencies()`, template `Scale<Tuning>` (freq(i) = tuning(root + steps[i])), `Melody`, `Rhythm` (+ derived `onsets()`), `Progression` |
| `transforms.h` | `TimeStretch` + factory; `transposeChord[/Cents]`, `transposeNote[/Semitones]`, `transposeMelodyRatio/Semitones`, `invertMelody`, `retrograde` (span-preserving), `stretchRhythm/Melody`, `quantize[/Melody]` (mechanical onset snap) |

## Contract points

- `Pitch::==` is sounding equivalence (exact frequency equality): C#4 == Db4
  holds; spellings compare as strings. (Caught by test: defaulted `==`
  compared spelling — fixed to the documented contract.)
- Grid-leaving results are unspelled (`midi == nullopt`, `spelling == ""`).
- Transforms are pure with accumulating provenance (`T(...)`, `I(...)`,
  `R(...)`, `S(...)`, `Q(...)`); retrograde involution holds on notes.
- `quantize` guards `llround` range (RangeError); durations untouched.
- `music/` still includes the foundation only — no `signal/`/`physics/`.

## Evidence

- Label table (incl. doubles, negatives, 7 rejections); enharmonic equality;
  grid transpose respell + range errors; ratio/inversion anchors.
- Chord just-vs-12TET thirds differ audibly (386.3 vs 400 cents); creation
  errors; scale resolution through tuning; rhythm onsets; all transform
  identities (transpose keeps ratios/timing, stretch composes to identity,
  quantize anchors).
- Audible: `progression_demo` renders just then 12-TET C-major triads
  (192044 bytes, both clean, peaks 0.242056/0.249633 — beating differs).
