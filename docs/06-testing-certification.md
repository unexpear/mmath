# Testing + Certification Plan

Status: locked · Correctness-first · First release claims only:
**"a verified mathematical signal kernel with offline PCM export"**

## Principle

Compare implementation against **analytical results**, not "it sounds right."
Separate **verification** (code solves the chosen model) from **validation**
(model agrees with physical observation) — the MVP verifies a sine + export
pipeline; it validates no instrument.

## Phase 0 tests — units + math identities

Compile-time (quantity arithmetic only — C++20 has no portable constexpr
`sin/cos/log2/pow`, so transcendental checks are runtime tests):

```cpp
static_assert(std::same_as<decltype(unwrap(period(FrequencyHz{440.0}))),
                           DurationSeconds>);
```

Negative operator cases are proven by isolated `try_compile` probes in
`tests/compile_fail/` (same headers, same C++20 config, diagnostics
preserved on violation) with a compiling `valid_control` companion:
`Frequency+Duration`, `Frequency+AngularFrequency`, `TimePoint+TimePoint`,
`Frequency*Duration`, `scalar*TimePoint`, `FrameCount+FrameCount`,
`Ratio+Ratio`, `SampleRate->Frequency`, `Quantity->double`.
In-TU `requires` probing is not used for negative operator checks (MSVC
emits hard errors instead of substitution failures there).

Runtime anchors (independent literals, not round trips alone):

```
T(440) = 1/440 s ~ 0.0022727272727272727 s (definitional check vs 1.0/440.0)
w(440) = 880*pi ~ 2764.601535159018 rad/s (explicit conversion fn)
cents(1) = 0, cents(1/2) = -1200, ratio(0) = 1, ratio(-1200) = 1/2 (exact)
cents(3/2) ~ 701.9550008653874 (documented tolerance; libm varies)
2, 3/2, 5/4 stored exactly = binary-exactness pin, NOT a general rational system
octave/fifth/major-third ratio checks renamed accordingly
```

Runtime, per-quantity tolerances (no global epsilon; see NUMERICS.md):
see anchor table above, plus the validity-error table (SampleRate{0},
ratio<=0, cents(r<=0), NaN/inf inputs -> defined errors) and the counter
rules (zero accepted; negative/fractional rejected pre-conversion;
increment-overflow and decrement-below-zero defined failures).

## Phase 1 tests — continuous functions

- `constant(440_Hz).at(any t)` returns 440 Hz for `t = 0.0, 0.000021, 14.239813`.
- Lambda sweep `220 + 220·t` matches closed form at sampled `t`.
- Functions carry `formula()` string (inspectability snapshot test).

## Phase 2 tests — signals + phase + state

- Constant-frequency sine matches `sin(2π·f·t + φ₀)` to tolerance.
- Linear sweep `f(t) = f_0 + a·t` matches `φ(t) = φ_0 + 2π·(f_0·t + ½·a·t²)`.
- **FM discriminator test**: integrated-phase FM vs naive `sin(2π·f(t)·t)` —
  implementation must match integrated version, demonstrably differ from naive.
  Example: `f(t) = 440 + 10·sin(2π·5·t)`.
- Composition: `(osc * env).at(t) == osc.at(t) * env.at(t)`.
- Stateful processors: reset behavior defined + tested; backward seek without a
  supported method returns unsupported-operation (never silent reset).
- **Block-size invariance**: identical input rendered in blocks of 64, 127, 512
  frames incl. final partial block → identical sample sequence within declared
  tolerance.

## Phase 2.5 — golden `a4.wav` certification

Fixture (peak 0.25 — a test setting, not a physical default):

```cpp
FrequencyHz frequency{440.0};       // strictly below fs/2 — the only certified audio path
SampleRateHz sampleRate{48000.0};
FrameCount frames{48000};           // authoritative; duration wrapper rounds documentarily
auto signal = sine(constant(frequency), AmplitudeLinear{0.25});
renderWav(signal, sampleRate, frames, "a4.wav");
```

Automated checks (no human listening), float-buffer checks before quantization
plus separate PCM-conversion checks (quantized samples never expected to equal
the unquantized identity exactly):

```text
RIFF/WAVE valid · PCM metadata valid (fields + data length independently verified)
48000 frames · 1 channel · covers t_0 … t_0+47999/48000
no NaN · no Inf · no unexpected overload (fail, don't hide)
mean ≈ 0 · RMS ≈ 0.25/√2 ≈ 0.1767767 on float buffer pre-conversion
estimated frequency ≈ 440 Hz (zero-crossing count first; FFT later)
known sample-value encoder fixtures pass · independent-decoder cross-check passes
write-failure injection → failed render, never partial-file success
max supported render length render passes
```

Determinism tiers (NUMERICS.md): (1) same build/config + deterministic inputs →
identical output; (2) different supported compilers/platforms → agreement within
documented tolerances; (3) byte-identical WAV required only for fixed encoding
fixtures. Foundation uses `double`, no fast-math in correctness builds; compiler
+ FP settings recorded with results. Later (post-MVP): FFT spectral-peak check.

## NullSink checks (every render)

Frames == expected `N` exactly; NaN/Inf count == 0; overload counted and
flagged; two renders in same build/config → identical stats. Null and WAV
(and test memory sink) consume the same generated buffers.

## MVP acceptance criteria (completion conditions, not features)

| Area | Required evidence |
|---|---|
| Quantities | Valid ops compile; invalid combos fail compile; invalid values → defined errors |
| Time | Exact frame counts, declared duration rounding, correct endpoint (`(N-1)/fs`), tested limits |
| Functions + phase | Constant + linear-sweep agree with analytical references |
| State | Reset defined; block-size changes don't change intended result |
| Numerics | Per-quantity tolerances + supported configs documented; non-finite detected |
| Rendering | Null, memory-test, WAV paths consume same samples |
| WAV | Known fixtures, correct metadata/lengths, independent decode, write-failure tests |

## Test layout

```text
tests/math/    scalar, constants, comparison
tests/units/   dimensions, conversions, compile-fail guards
tests/signal/  functions, sine closed-form, FM discriminator, composition
tests/sink/    null stats, wav header/frames/stats/determinism
```

Framework: Catch2 (or minimal stdlib harness if bootstrapping before dep fetch —
core itself stays zero-dep regardless).
