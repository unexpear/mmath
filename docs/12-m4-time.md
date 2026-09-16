# M4 — Musical Time: Tempo Curves + Meter (complete)

Status: implemented · Suite: 96/96 (MSVC Release + Debug) · Probes: 10/10

The scheduling gap is closed: everything no longer lives in absolute
seconds. Tempo maps both directions (beats <-> seconds); meter stays
subdivision math. Piecewise/multi-segment maps deferred.

## New headers

| Header | Contents |
|---|---|
| `time/tempo.h` | `TempoCurve` concept (`beatsAt`/`secondsAt`); `ConstantTempo::create` (exact inverse); `LinearRampTempo::create` (ramp over `[t0, t0+span]`, constant extension outside; quadratic integral with closed-form inverse, root selected in-span; strictly increasing so the inverse always exists) |
| `music/meter.h` | `Meter::create` (positive ints); `measureBeats`/`beatBeats` (quarter-based: 1 Beat = 1 quarter); `measureDuration(bpm)`; 1-based `measureBeatToBeats`; `AdditiveMeter` (`{3,2,2}`-style groups + unit, `totalBeats`) |

## Contract points

- Creation validates (non-positive/non-finite tempos, non-positive span);
  evaluation failures are `RangeError`/`DomainError`, never defaults.
- Constant round trip is same-arithmetic-reversed (tight tolerance, not
  `==`: flagged as bitwise-unsafe during authoring).
- Meter never touches clocks: `(measure, beat) -> beats`, composed with a
  tempo curve by the caller (tested end-to-end: m.2 b.1 4/4 @120 = 2.0 s).

## Evidence

- Constant: 120 BPM anchors both directions + round trip.
- Ramp 60->120/60 s: beats(30) = 37.5, beats(60) = 90, extension
  beats(-10) = -10 / beats(70) = 110; inverse secondsAt(45) =
  34.868329805051374 (Python-verified literal); round trips in all three
  regions; decelerando 120->60 inverts (negative quadratic coefficient).
- Meter: 4/4 @120 = 2.0 s exact; 7/8 = 1.75 s; (5,3) -> 18 beats;
  rejection tables; additive 3+2+2/8 = 3.5 beats.
- Audible: `tempo_demo` renders an accelerating click track (beats 0..8 at
  shrinking intervals 0.944 -> 0.590 s) in one certified pass
  (`metronome.wav`, 591058 bytes, peak exactly 0.25, clean).
