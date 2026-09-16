# M3 — Signal Depth: Harmonics + ADSR (complete)

Status: implemented · Suite: 88/88 (MSVC Release + Debug) · Probes: 10/10

First timbre milestone. No wavetable, FM stacks, or closed timbre systems:
recipes are data in tests/examples; the kernel holds one generic additive
builder and one stateless ADSR function.

## New headers (`include/mmathen/signal/`)

| Header | Contents |
|---|---|
| `harmonics.h` | `Harmonic{ratio, level}` data; `HarmonicSeries::create` (rejects empty tables, non-positive/non-finite ratios, negative/non-finite levels; levels > 1 allowed as headroom); `additive(f0, ampFn, series)` — one sine per partial at `f0*ratio`, amplitude-scaled, mixed via the existing operator |
| `envelope.h` | `ADSRSegments` + `makeADSR` (non-negative finite times, sustain in [0,1]); `adsr(segments, on, off)` — pure `A(t)`: silence, linear attack/decay, sustain hold, linear release ending exactly at `off+release`; short notes release from the value at `off`; `off < on` is `InvalidArgument` |

## Contract points

- Envelopes are stateless functions of `(t, on, off)` — no stateful
  processor smuggled in; custom shapes stay plain `wrap()` lambdas.
- Overload remains an export-boundary fact: the suite's two-partial recipe
  peaks at 1.5 unscaled (correctly flagged), scaled via `applyGain` for the
  clean-render test — never silently normalized.
- No new units, no new operators, no new probes needed.

## Evidence

- Additive matches hand-written sin sums; creation-error tables green.
- ADSR boundaries CHECK-exact (binary-exact segment values: attack 0.25,
  decay/sustain/release 0.5, on 0.0, off 2.0); short-note and error cases.
- Audible: `timbre_demo` renders pure | octave+fifth | 8-partial 1/n at
  220 Hz ADSR-shaped (`timbre.wav`, 288044 bytes, peaks
  0.249/0.277/0.335, all clean).
