# D7 — Phase Modulation + Segment Envelopes (complete)

Status: implemented · 184/184 on MSVC Release+Debug, UCRT64 GCC
Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan
(zero reports) · Probes: 10/10

Closes roadmap step 2 (signal depth): harmonics (M3), ADSR (M3), FM (D5),
wavetable (D6), and now PM and custom envelopes.

## New code

| Header | Contents |
|---|---|
| `signal/pm.h` | `SinusoidalPM` shape (analytic phase + instantaneous frequency); `pmTone` (fc, fm, β, θ); `pmPeakDeviation` (β·fm); `phaseModulate` — any `Signal` as phase modulator, output is a `Signal`, so operators stack |
| `signal/segment_envelope.h` | `SegmentEnvelope` (start level + segments: Linear, Exponential, Curved(c)); `envelope(shape, on)`; `gatedEnvelope(strike, release, on, off)`; `t60Decay(on, T60)` |

## Contract points

- **PM vs FM, stated precisely**: PM's peak deviation is β·fm (grows with
  the modulator rate); FM fixes the deviation d and its index d/fm falls.
  Sinusoidal FM of index I is *exactly* PM with β = I, θ = −π/2, φ₀ + I —
  the two are one family, pinned as an identity, not a resemblance.
- **Operator stacking without new machinery**: `phaseModulate` adds
  `index·m(t)` to the carrier phase for any signal `m`; stacks are ordinary
  composition. Feedback operators are deferred with reason: they need
  sample-delayed state, which is outside the pure-function signal contract.
- **Segment curves** (u = elapsed fraction of a segment, v0 → v1):
  Linear `v0 + (v1−v0)u`; Exponential `v0·(v1/v0)^u` (straight in dB);
  Curved `v0 + (v1−v0)·expm1(cu)/expm1(c)` (c > 0 slow start, c < 0 fast
  start, c = 0 exactly linear; `expm1` keeps small |c| accurate).
- **Exponential honesty**: a true exponential never reaches or leaves 0, so
  an Exponential segment touching level 0 is `DomainError` — including a
  release that would start from 0 because the note was released at its
  onset (checked when the gated envelope is built, since off is known).
- **Exact boundaries**: at and after a segment's end the value *is* its
  target; zero-duration segments jump; the final level holds.
- Envelopes stay stateless functions of absolute time with note-on/off as
  parameters, like `adsr()`; `adsr()` is kept (M3 contract), not replaced.

## Evidence

- PM vs inline textbook formula (1e-12).
- **Jacobi–Anger**: PM equals `Σ J_k(β)·sin(a + kψ)` with
  `J₋ₖ = (−1)ᵏJₖ`, k = −30..30, to 1e-12 — the Bessel sideband structure
  verified in the time domain, independent of the oscillator code.
- **Integral identity**: D5's adaptive Simpson on PM's instantaneous
  frequency reproduces the closed-form phase to 1e-8.
- **FM ≡ PM**: `fmTone(440, 5, I=2)` equals `pmTone(θ = −π/2, φ₀ = 2)` to
  1e-12; `frequencyAt(0)` = fc + β·fm exactly (540 Hz).
- `phaseModulate(sine)` equals `pmTone`; a 3-operator stack matches the
  hand-nested closed form; formulas pin `pm(pm(sine(const)))`.
- Linear segment anchors CHECK-exact (0.5, 1.0, 0.75, zero-duration jump,
  hold); exponential dB-linear to 1e-12 (−20 dB/s) with exact endpoint;
  curved midpoints at c = ±2·ln 3 are exactly 1/4 and 3/4 (1e-15), c = 0 is
  exactly linear, c = 1e-12 linear to 1e-12, monotone for both signs.
- **Cross-checks against independent code**: gated linear
  `{0.25→1, 0.5→0.5}` + release `{0.5→0}` reproduces `adsr()` to 1e-12 for
  a full note and a note released mid-attack; an Exponential 1 → 0.001
  segment reproduces `t60Decay` (1e-12 relative).
- Enveloped PM renders block-invariant (64/127/512 exact) and clean; the
  release is audible up to 0.1 s and the tail after it is exactly zero.
  (First draft of this test checked frame 4799, still inside the release —
  a test error caught by the run, corrected to check the tail after the end.)
- Audible: `pm_demo` renders a decaying 2-operator electric piano (T60
  envelopes), a 3-operator stack under a curved swell with exponential
  release, then FM and the identical PM back to back (`pm.wav`, 384044
  bytes, all clean).
