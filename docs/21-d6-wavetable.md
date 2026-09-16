# D6 — Wavetable Oscillator with Bounded Interpolation (complete)

Status: implemented · 169/169 on MSVC Release+Debug, UCRT64 GCC
Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan
(zero reports) · Probes: 10/10

Closes the wavetable item of roadmap step 2 (signal depth). Remaining step-2
items, not in this milestone: phase modulation (PM) beside the existing FM,
and non-linear envelope segments beyond ADSR.

## New code

| Header | Contents |
|---|---|
| `signal/wavetable.h` | `Wavetable::fromHarmonics` (integer partials, N > 2K, known spectrum) and `fromSamples` (arbitrary cycle, spectrum unknown, K = floor((N-1)/2)); `wavetableValue` (checked unit-phase lookup, linear or 4-point Lagrange cubic, periodic wrap); `interpolationErrorBound`; `wavetableFrequencyLimit` (fs/(2K), exclusive); `wavetableTone` / `wavetableSweep` over the analytic phase shapes of `phase.h` |

## Contract points

- **Continuous-time, honestly**: interpolation runs on unit phase, which is
  continuous, so the oscillator is a real `Signal` evaluated at any `t` —
  unlike PolyBLEP (D5), whose correction spans samples and must stay
  frame-indexed.
- **Stated error bounds** (h = 2π/N, harmonic tables only):
  linear `h²/8 · Σ aₖk²`, cubic `3/128 · h⁴ · Σ aₖk⁴` — the polynomial
  interpolation remainder, with 9/16 = max |(x+1)x(x-1)(x-2)| on [0,1].
  For a single sine both are *attained* at interval midpoints:
  `1 − cos(h/2) = h²/8 − h⁴/384` and
  `1 − (9/8·cos(h/2) − 1/8·cos(3h/2)) = 3/128·h⁴ − h⁶/512`.
  Sample tables have no known spectrum → `DomainError`, never a guessed bound.
- **Bandwidth contract**: table content spans f..K·f, so f < fs/(2K)
  (sweeps: peak instantaneous frequency). Interpolation error is periodic
  in phase, so it only adds harmonics of f, each ≤ 2·bound in amplitude —
  stated, not measured; the spectral proof awaits the FFT milestone, as
  for PolyBLEP.
- **Rejections**: non-integer ratios (a single cycle cannot hold them),
  N ≤ 2K, N outside [4, 2²⁴], non-finite samples, phase outside [0, 1),
  non-positive tone frequency — all `InvalidArgument`.
- Non-finite phase propagates as NaN for `tryAt()`/render stats to flag.
- Deferred with reason: mipmapped/crossfaded table sets (needs a policy for
  choosing K per frequency band — design, not math), table morphing.

## Evidence

- Binary-exact anchors on `{0, 1, 0, −1}`: nodes exact for both
  interpolators; linear midpoints ±0.5; cubic midpoints ±0.625 including the
  wrap-around stencil (nodes 2, 3, 0, 1).
- Harmonic table samples match hand-written sin sums (1e-14).
- **Bounds pinned from both sides** (single sine, N = 64, 20011-point prime
  grid): measured worst ≤ bound and ≥ 0.9·bound for linear and cubic; cubic
  worst < linear worst / 100. Multi-partial 1/n table (K = 8, N = 256):
  measured ≤ summed bound for both.
- Tone vs independent `additive()` (K = 8, N = 2048) and sweep vs
  independent `sineSweep()`: |diff| ≤ amplitude·bound + 1e-11 (slack is
  phase roundoff only; an earlier 1e-9 slack was rejected as comparable to
  the cubic bound itself and therefore able to mask a real violation).
- Renders block-invariant (64/127/512 exact) and clean.
- Frequency limit anchors: K = 8 at 48 kHz → exactly 3000 Hz; 64-sample
  table → 48000/62.
- Audible: `wavetable_demo` bakes a 16-partial 1/n table (N = 2048; linear
  bound 1.600e-4, cubic 3.841e-8, f < 1500 Hz) and renders linear tone,
  cubic tone, cubic sweep 110→880 Hz (`wavetable.wav`, 288044 bytes,
  peaks 0.4396, all clean) through the certified offline path — no
  hand-rolled render loop (D5 lesson applied).
