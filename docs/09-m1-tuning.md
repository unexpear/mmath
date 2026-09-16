# M1 — Tuning Systems (complete)

Status: implemented · Suite: 64/64 (MSVC Release + Debug) · Probes: 10/10

First post-MVP domain milestone. Tuning maps integer degrees to frequencies;
pitch labels (A4, C#) stay deferred to M2.

## Interface

`TuningSystem` concept: `degreeToFrequency(int) -> Result<FrequencyHz,
QuantityError>` + `formula() -> string`. Zero-cost, no virtuals.
`music/` includes the shared foundation only — never `signal/` or
`physics/`; examples wire frequencies to oscillators as adapters.

## Catalogue (`include/mmathen/music/tuning.h`)

| Type | Mapping | Validation |
|---|---|---|
| `EqualTemperament<N>` (+ `TwelveTET`, `NineteenTET`, `ThirtyOneTET`) | `f_ref * 2^((n-n_ref)/N)` | Non-positive/non-finite ref → `InvalidArgument`; overflow → `RangeError` |
| `JustIntonation` | Ratio period, octave-wrapping (`ratios_[n mod s] * 2^oct`) | `create()` rejects empty/non-positive tables, bad root |
| `CentsTuning` | Cent-offset period, octave-wrapping | Same creation policy; finite offsets |
| `CustomMap` | Pure callable + verbatim formula (e.g. `"220 * 2^(n/19)"`) | Callable's own responsibility |

## Evidence

- 12-TET: A440 table exact (0/±12); C-major `{0,4,7}` vs Python-literal
  anchors (261.6255653005986 / 329.6275569128699 / 391.99543598174927).
- 19-TET: period exact (19 → 440); n=7/18 vs literals; 31-TET period exact.
- Just: fifth/third exact, wrap both directions (deg 4 → 880, deg −1 → 440,
  deg −2 → 330); creation-error table green.
- Cents: C# anchor 554.3652619537442; wrap exact; empty-table error.
- Audible: `tuning_demo` renders `nineteenth.wav` (19 × 0.3 s) and
  `justmajor.wav` (4 × 0.5 s), all stats clean, sizes exact.
