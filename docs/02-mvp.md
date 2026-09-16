# MVP Scope — Phases 0–2 + 2.5

Status: locked · Zero third-party deps in core · C++20

## Definition of MVP

```text
Phase 0   — Math + Units
Phase 1   — Continuous Functions + Time
Phase 2   — Signals + Oscillation
Phase 2.5 — Minimal Offline Sinks (Null + WAV)
```

WAV is **outside the mathematical kernel**: a deterministic sink, not an audio-engine architecture.

```text
Signal<double>
  ↓
sample(t)
  ↓
PCM conversion
  ↓
WAV file
```

End-to-end proof: `equation → samples → audible file`.

## Phase 0 — Math + Units

Directories:

```text
src/
  math/   scalar.h, constants.h, numeric_limits.h, comparison.h
  units/  dimension.h, quantity.h, unit.h, physical_units.h, music_units.h
tests/
  math/
  units/
```

Deliberately **excluded**: Vector/Matrix/Quaternion. Add structures only on real demand
(music/acoustics-driven, not game-engine-driven).

Initial math scope:

```text
scalar numeric policy, strong quantities, dimensions, ratios,
complex, constants, safe math fns, interpolation,
numeric comparison/tolerance, range/domain checking
```

Certified identities (see `06-testing-certification.md`):

```
1 Hz = 1/s · T = 1/f · ω = 2πf
cents(r) = 1200·log2(r) · ratio(c) = 2^(c/1200)
cents(2) = 1200 · ratio(1200) = 2
```

Example surface:

```cpp
constexpr FrequencyHz A4{440.0};
constexpr TimeSeconds second{1.0};
constexpr SampleRateHz cdRate{44100.0};
constexpr FrequencyRatio octave{2.0};
constexpr Cents octaveCents{1200.0};
```

Exit criteria: `Hz + s` fails to compile; all identity tests pass; zero deps.

## Phase 1 — Continuous Functions + Time

Introduce:

```cpp
template<typename Output>
class Function1D;   // f: R → X
```

Time function: `f: t → X`.

```cpp
auto frequency = constant(FrequencyHz{440.0});
auto sweep = [](TimeSeconds t) {
  return FrequencyHz{220.0 + 220.0 * t.value()};
};
```

Principle: **values may be constant, but architecture assumes continuous variation**
(modulation, tempo curves, envelopes, automation later).

Exit criteria: constant + lambda functions queryable at arbitrary `t`
(e.g. `0.0`, `0.000021`, `14.239813` s) independent of any sample rate.

## Phase 2 — Signals + Oscillation

Signal: `S: t → A`, initially `Signal<AmplitudeLinear>`, composable:

```cpp
signalA + signalB, signalA * signalB
delay, integrate, differentiate, filter → new Signals
```

Oscillator architecture (not `SineOscillator(float)`):

```text
Frequency signal → Phase accumulator → Periodic fn → Amplitude signal
```

Math:

```
dφ/dt = 2π·f(t)
x(t) = A(t)·sin(φ(t))
constant f: φ(t) = φ₀ + 2π·f·t
```

Exit criteria: constant-frequency sine matches closed form; FM case uses
integrated phase (fails if naive `sin(2π·f(t)·t)` used — test covers this).

## Phase 2.5 — Null + WAV Sinks

Tiny deterministic sinks only. No threads, clocks, latency, drivers.

```cpp
renderWav(signal, "440hz.wav", SampleRateHz{48000}, FrameCount{48000});
// mono · 48000 Hz · 48000 frames · 440 Hz sine @ peak 0.25 · deterministic (Tier 1)
// (duration-based overload allowed with documented rounding; frame count authoritative)
```

`NullSink` first (CI, benchmarks, determinism, sample counting,
NaN/Inf + clipping detection, profiling), then `WavSink`.

Renderer shape:

```cpp
renderer.render(graph, sink);
```

Exit criteria: golden `a4.wav` certification passes (see `06-testing-certification.md`;
acceptance table: quantities / time / functions+phase / state / numerics / rendering / WAV).
Canonical contract source: `FOUNDATION.md`, `NUMERICS.md`, `VALIDATION.md` (accepted/proposed/deferred).

## Explicitly out of MVP

MIDI, chords/scales/harmony, instruments, room physics, FFT framework,
plugin system, GUI, realtime device output. These must not shape Phase 0–2 APIs.
