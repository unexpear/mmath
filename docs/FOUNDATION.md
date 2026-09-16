# FOUNDATION — Canonical Base Decisions

Status: canonical · Labels: **[ACCEPTED] / [PROPOSED] / [DEFERRED]**
Supersedes any conflicting suggestion in `00–08` planning notes.

## Domains

- **[ACCEPTED]** Branched domains, not one chain: shared math foundation with
  `time/signals`, `symbolic music/tuning`, `physical models/acoustics` as siblings.
  Explicit adapters connect domains; rendering consumes resulting signals.
- **[ACCEPTED]** Musical conventions are selectable definitions, never acoustic law.
  Physics must not include music headers; music must not be required by physics.

## Contracts

- **[ACCEPTED]** Two contracts: **time function** (pure `at(t)`, no hidden state)
  vs **stateful processor** (owned initialized state, explicit reset, sequential
  processing; unsupported backward seek → error, never silent reset).
- **[ACCEPTED]** Continuous-time `t ∈ R` does not imply mathematical continuity:
  piecewise + declared discontinuities allowed; no universal diff/integ promise.
- **[ACCEPTED]** `dφ/dt = 2π·f(t)`; linear sweep closed form
  `φ(t) = φ_0 + 2π·(f_0·t + ½·a·t²)` is the MVP modulation reference.
- **[ACCEPTED]** Frame count `N` authoritative at low level;
  `t_n = t_0 + n/fs`, `n = 0…N-1`. Duration APIs wrap with documented rounding.
  Integer counters are the clock; frames ≠ channel samples; max render length declared + tested.
- **[ACCEPTED]** Graph never names sinks: `renderer.render(graph, sink)`.
  Null/WAV/test-memory sinks consume the same generated buffers.
- **[PROPOSED]** Processing boundary for future realtime: prepare beforehand,
  process into caller-supplied buffers, bounded work, no alloc/file/blocking in callback.
- **[DEFERRED]** All device backends (WASAPI first when scheduled), DSP framework,
  symbolic algebra / general numerical integrators, anti-aliasing framework.

## Units

- **[ACCEPTED]** Strict dimensions + semantic strong types; explicit cycles↔radians;
  distinct `Frequency / AngularFrequency / SampleRate / Phase / TimePoint /
  Duration / FrameIndex / FrameCount / FrequencyRatio / SignedAudioValue`.
- **[ACCEPTED]** Type correctness ≠ value validity: invalid values → defined errors,
  never silent defaults. `.value()` only at numerical/external boundaries.

## Sinks / formats

- **[ACCEPTED]** Order: Null → WAV → (later) native. MVP WAV fixed:
  mono, signed 16-bit PCM, little-endian, documented rounding/endpoint/overload/size policies.
- **[ACCEPTED]** Certification renders fail on non-finite, unexpected overload,
  or write failure. Independent-decoder cross-check required in tooling.
- **[ACCEPTED]** Certified audio path is narrow: constant-frequency sine, `f < fs/2`.
  Other signals: supported / no-bandwidth-guarantee / rejected — explicitly labeled.

## Scope claim

- **[ACCEPTED]** First release claims only **"a verified mathematical signal kernel
  with offline PCM export"**. No instrument-modeling claims. MVP scope frozen
  (Phases 0–2 + 2.5); later domains must not shape MVP APIs beyond the reserved
  contracts above (the "prohibition" is scoped: reserve contracts, defer builds).
