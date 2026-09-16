# Continuous Time + Signals + Oscillation Design

Status: planning (MVP Phases 1–2)

## Continuous time first

Music software goes sample-centric too early. We don't.

```text
t ∈ R
```

Sampling is discretization at the boundary, with frame count authoritative:

```
t_n = t_0 + n / f_s,   n = 0 … N-1,   e.g. t_n = t_0 + n / 48000 at 48 kHz
```

N frames at 48 kHz = exactly 48 000 frames for 1 s; the last sample is at
`t_0 + 47999/48000`, never `t_0 + 1` (that would be 48 001 frames).
Integer frame counters are the clock; evaluation times derive from them,
not from repeated float addition. Duration-based APIs are convenience
wrappers with a documented rounding policy. Frames ≠ channel samples:
a stereo frame holds two channel samples (block alignment follows from
channel count × sample width). Declare a supported maximum render length
and test it — a wide counter does not guarantee float-time precision.

Same oscillator queryable at `t = 0.0`, `0.000021`, `14.239813 s`
regardless of eventual 44.1 / 48 / 192 kHz render.

Tempo as function (design hook, full `BPM(t)` comes post-MVP):

```
t = 60·B / BPM            (constant tempo)
BPM = BPM(t)              (ritardando, accelerando, rubato later)
```

## Functions (Phase 1)

```cpp
template<typename Output>
class Function1D;  // f: R → X
```

Constant and varying values share the interface:

```cpp
auto frequency = constant(FrequencyHz{440.0});
auto fm = [](TimeSeconds t) { return FrequencyHz{440.0 + 10.0 * std::sin(2.0 * std::numbers::pi * 5.0 * t.value())}; };
// f(t) = 440 + 10·sin(2π·5·t) stays an object, not a one-shot evaluation
```

Equations are first-class: they remain inspectable (`formula()`) and
composable into modulation, envelopes, automation later.

## Two contracts: time function vs stateful processor (do not merge)

| Contract | Meaning | Rules |
|---|---|---|
| **Time function** | Evaluate a value at time `t` without changing hidden state | Pure, repeatable, seekable to any `t` |
| **Stateful processor** | Advance initialized owned state through an input sequence | Explicit init + reset; sequential processing; backward seek only via supported method or unsupported-operation error — never silent reset |

Filters, delays, and simulated instruments have memory: output depends on
state + input history, not just the timestamp. Never force both into one
universal `sample(t)` interface. Reserve now the realtime processing
boundary for later: prepare beforehand, process into caller-supplied buffers,
bounded work, no allocation/file access/blocking sync in the callback —
but build no OS backend in MVP.

Continuous-time does **not** mean mathematically continuous: piecewise
functions and declared discontinuities are allowed. No promise that every
function supports differentiation or integration.

MVP modulation test (no symbolic algebra / numerical integrator needed).
Linear sweep `f(t) = f_0 + a·t` (`a` in Hz/s) has closed-form phase:

```
φ(t) = φ_0 + 2π·(f_0·t + ½·a·t²)
```

Constant and linear-sweep cases carry analytical references in tests.

## Signals (Phase 2)

Fundamental concept: `x(t)` — a quantity varying over time. In MVP the
certified audio path is deliberately narrow: **constant-frequency sine with
f strictly below fs/2.** Time-varying phase still gets mathematical tests,
but arbitrary time functions are NOT labeled alias-controlled renderable.
The renderer distinguishes: supported-sampling / sampled-without-bandwidth-
guarantee / rejected configuration. No anti-aliasing framework in MVP —
and no API that promises one exists (naive saw/square/triangle sampling
aliases; digital versions need further treatment, deferred).

Signed audio values are distinct from non-negative amplitude magnitude,
gain, pressure, and voltage:

```cpp
Signal<SignedAudioValue> osc;   // x(t) takes ± values
AmplitudeLinear env{0.25};      // A ≥ 0 magnitude in x(t) = A·sin(φ(t))
```

Intermediate graph values may exceed the export range; overload is handled
explicitly at the export boundary, never by silent per-render normalization.
`0.5` (digital) ≠ `0.5 Pa` — pressure→sample needs a declared scale.
First fixture peak: **0.25** (test setting, not a physical default).

```cpp
Signal<float> osc, env, mod;
auto out = osc * env;   // new Signal
```

Operations produce new signals:

```cpp
signalA + signalB · signalA * signalB
delay(signal, t) · integrate · differentiate · filter
```

Signal graph is math, not audio code.

## Oscillation — phase accumulation done right

Sinusoid:

```
x(t) = A·sin(2π·f·t + φ)
```

Varying frequency — integrate, don't multiply:

```
φ(t) = φ₀ + 2π·∫₀ᵗ f(τ) dτ
x(t) = A(t)·sin(φ(t))
dφ/dt = 2π·f(t)
```

Wrong (fails FM): `sin(2π·f(t)·t)`. Tests must distinguish these.

Primitives (MVP: sine first; rest post-MVP or as trivial extensions):

```text
sine, square, triangle, sawtooth, pulse, arbitrary periodic,
wavetable, stochastic, physical (later)
```

Architecture:

```text
Frequency signal → Phase accumulator → Periodic function → Amplitude signal
```

## Post-MVP hooks (do not implement now)

- Waves `y(x,t)`, media `c=√(γRT)`, pressure `p(x,t)`, modal sums,
  coupling `H(f)` chains, excitation position → modal weights,
  spatial `t=d/c` + Doppler, hearing (loudness ≠ amplitude).
- Each gets its own doc when scheduled.
