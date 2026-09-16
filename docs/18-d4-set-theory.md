# D4 — Set Theory, Voice-Leading, Euclidean Rhythm (complete)

Status: implemented · 132/132 on MSVC Release+Debug, GCC-13 Release,
Clang-18 Release, GCC ASan+UBSan (zero reports) · Probes: 10/10

Depth milestone (research-grade) for the symbolic domain: Forte primes
implemented (not Rahn), verified row-by-row against an independent
implementation; voice-leading as an explicit simplified cost model;
Euclidean rhythms via true Bjorklund construction.

## New code

| Header | Contents |
|---|---|
| `music/forte_table.h` | Full catalogue, 352 rows: number+variant, prime, interval vector; Forte spellings throughout (see table note below) |
| `music/pcset.h` | `PitchClassSet` (sorted unique 0..11); `transpose`/`invert`; `intervalClassVector`; `normalOrder` (min span, packed-left ties); `primeForm` (Forte packed-left rule); `forteNumber` ("3-11B"-style with A/B side detection + DomainError canary); exact `isTranspositionOf`/`isInversionOf`/`sameSetClass` |
| `music/voiceleading.h` | `voiceLeadingCost` (L1 motion + fixed parallel-fifth/octave penalty, ours, documented); voice-matched MIDI arrays; permutation search deferred |
| `music/euclid.h` | `euclideanRhythm` (recursive Bjorklund + canonical rotation + rotation param); `swingDelay` (triplet grid, amount in [0,1]) |

## Verification story (the point of this milestone)

- music21 cross-checked all 352 transcribed rows (number, prime, vector):
  **0 violations** after adjudication. music21 normalizes B inputs to A
  primes (expected), collapses Z numbers (its limitation, documented), and
  skips the empty set (definitional row kept).
- **Found inconsistency, temp-table era**: the source list's 5-20A row
  prints prime [0,1,4,6,8] with vector <2,2,1,3,1,1> — but that prime yields
  <1,2,1,3,2,1> (it is 5-30A's prime) and that vector belongs to 5-13A
  (music21-confirmed both). Stored pair: prime [0,1,3,7,8] / vector
  <2,1,1,2,3,1>, consistent and confirmed. Do not "fix" the source from
  here; the proof is in this paragraph.
- C++ suite re-verifies the catalogue mechanically: every row's vector
  recomputed, every prime recomputed, per-cardinality counts
  (1,1,6,12,29,38,50,38,29,12,6,1,1) asserted.
- Canonical Euclidean forms match Toussaint 2005 (tresillo, cinquillo,
  Ruchenitza) exactly; initial Bresenham-style attempt produced a rotated
  variant and was replaced (documented in code history via the algorithm).

## Evidence

- Famous classes resolve (3-11B major, 3-10, 3-12, 4-27B, 4-28, 7-35, 6-35,
  5-35); T/I equivalences exact; creation errors.
- Voice costs exact (C→G motion 3, parallels detected, oblique/static
  excluded); Euclidean patterns + rotations + errors; swing grid anchors.
- Audible: `voice_demo` renders smooth C–F–G–C (costs 3/6/3, no parallels)
  plus a tresillo click coda (`voices.wav`, 499244 bytes, clean).
