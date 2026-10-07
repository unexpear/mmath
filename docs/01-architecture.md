# Architecture — Layered Dependency Design

Status: planning · Rule: **domains are compatible, not chained. Adapters connect them; nothing requires the full stack.**

## Dependency structure (branched, not one giant chain)

Supersedes the earlier single `math → … → music → dsp → renderer` chain,
which wrongly implied a tuning system requires a room simulator or an
oscillator requires musical notes.

```text
Shared mathematical foundation (math, units, numerics, functions)
    │
    ├── Time, functions, and signals (continuous t, Function1D, Signal graph)
    ├── Symbolic music and tuning (pitch/tuning/notes/rhythm — selectable definitions)
    └── Physical models and acoustics (waves/media/objects/modes/spatial/hearing)
                    │
    Explicit adapters connect domains (e.g. pitch→frequency, note→excitation,
    pressure→digital sample with declared scale)
                    ↓
    Rendering consumes resulting signals (offline buffers → Null / WAV / later native)
```

Supporting systems (side-car, depend inward only):

```text
measurement, analysis, serialization, visualization, testing, plugins
```

## Layer map (condensed from full 30-layer spec)

| Layers | Module dir (proposed) | Contents |
|---|---|---|
| L0 | `math/`, `units/` | scalar policy, constants, comparison, dimensions, quantities, complex, interpolation, tolerance |
| L1–L3 | `time/`, `signal/` | continuous `t ∈ R`, `Function1D`, `Signal<A>`, phase accumulator `dφ/dt = 2πf(t)`, osc primitives |
| L4–L6 | `waves/`, `medium/`, `pressure/` | `y(x,t)=A·sin(kx−ωt+φ)`, `k=2π/λ`, `ω=2πf`, `v=fλ`, `c=√(γRT)`, wave eq `∇²p−1/c²·p_tt=0`, solver interface |
| L7–L11 | `objects/`, `materials/`, `coupling/`, `modes/` | String/Bar/Plate/Membrane/Tube/Cavity/Reed/Hammer/Bow/Pickup/Mic/Speaker; geometry ≠ material; `f1=1/2L·√(T/μ)`; `H(f)` chains; `x(t)=Σ An·e^(−αn·t)·sin(2πfn·t+φn)` |
| L12–L13 | `spatial/`, `hearing/` | pose/directivity, `t=d/c`, Doppler, reverb/HRTF; thresholds, masking, loudness, roughness (physical amp ≠ loudness) |
| L14–L25 | `music/` | Frequency≠Pitch≠PitchClass; `r=f2/f1`; `cents=1200·log2(r)`; `n→f` tunings; `NoteEvent` as event; `BPM(t)`; rational durations; transforms `T_k(P)`, `I_a(p)=a−p`, `t′=k·t` |
| L26–L27 | `compose/`, `instruments/` | graph nodes (not built); instrument = assembly of objects + materials + excitation. Assembly so far: violin, viola, cello, double bass, a concert pedal harp, a French double harpsichord, a six-string classical guitar, a six-string electric guitar, a five-string Chabassier piano bank, steady Helmholtz bow, a sum of open-loop driving-point body modes, a cavity Helmholtz frequency, a mutual body-mode stiffness, a compact-monopole far field, Schelleng's minimum bow force from a caller bridge resistance, a time-stepped stick-slip capture on rigid ends, wound-string bending stiffness from a caller core, a string re-solved on a caller bridge mobility or resistance, hard narrow hammer with a first-image sum and felt compliance, quasi-static reed with a cylindrical bore and reed inertia, a clarinet cylinder on that open bore, an open-open flute with an embouchure jet, fingered stop as a shorter vibrating length, Yamaha Tour Custom drum shells with circular-membrane modes, and a SABIAN XSR cymbal set on free circular plates (`docs/48`) |
| L28–L29 | `electronics/`, `dsp/` | `V(t), I(t), Z(ω)`; FIR/IIR/FFT-conv/delay/sat/resample/add/sub/FM/granular; blocks declare latency, fs-assumptions, state, realtime-safety |
| L30 | `renderer/`, `sink/` | `MathSignal → Sampler(t_n=n/fs) → DSPGraph → Buffer → Sink`; `IAudioSink: Null/Wav/Wasapi/...` |

## MVP slice of this map

Phases 0–2.5 implement only:

```text
math → units → time/functions → signal/oscillation → sink (Null + WAV)
```

Everything else is **designed for but not built**. No forward references
from core into music/physics/instruments.

## Invariants

1. Sampling is a **boundary operation** (`t_n = t_0 + n/fs`), never the signal representation.
2. **Two contracts, never one `sample(t)` for everything:** stateless time functions
   evaluate at any `t` without hidden mutation; stateful processors advance owned,
   initialized state sequentially (explicit init/reset; backward seek only via supported
   method or unsupported-operation error).
3. Frequency modulation integrates to phase — never `sin(2π·f(t)·t)`.
4. Signal graph does not name sinks: `renderer.render(graph, sink)`, never `signal.playOnWasapi()`.
5. Every object is inspectable: `formula()` / `toExpression()` / `toJson()` preserved through transforms.
6. Musical conventions are **selectable definitions**, never laws of acoustics.
   Physics and music are compatible via adapters; neither requires the other.
7. Realtime-capable path reserves its contract now (prepare-beforehand, process-into-supplied-buffers,
   bounded work, no alloc/file/blocking in callback); OS backends come later.
8. No universal solver interchange promise: solver↔object compatibility is explicit per model.
