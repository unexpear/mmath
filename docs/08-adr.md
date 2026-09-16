# ADRs — Locked Foundation Decisions

Status: locked · Changes require explicit ADR amendment

## ADR-001 — Language: C++20

Chosen for deterministic performance, `concepts`/`constexpr` for the
dimensional system, and eventual realtime-audio suitability.
CMake build. No language change without re-certifying all golden tests.

## ADR-002 — Zero third-party dependencies in core

`math`, `units`, `time`, `signal`, and the minimal WAV writer use only
the C++20 standard library. Test framework (Catch2) and later parsers
(exprtk-like), FFT, backends live outside core and never leak includes inward.

## ADR-003 — Strict units + semantic strong types everywhere

Both SI dimensions **and** domain tags enforced at compile time.
`FrequencyHz`, `TimeSeconds`, `PhaseRadians`, `FrequencyRatio`,
`AmplitudeLinear`, `Beat`, `BPM`, `Cents`, `SampleRateHz`, `SampleIndex`
are distinct types even when dimensionally identical.
`FrequencyHz{440} + TimeSeconds{1}` must not compile.
Boilerplate cost accepted.

## ADR-004 — Continuous time first, sampling at the boundary

Signals are `f(t)`, `t ∈ R`, queryable at arbitrary `t`.
`t_n = n/f_s` exists only in sinks/renderers. No sample-rate globals in core.

## ADR-005 — Phase integrates frequency

`dφ/dt = 2π·f(t)`, `x(t) = A(t)·sin(φ(t))`.
Naive `sin(2π·f(t)·t)` is a bug; FM discriminator test enforces this.

## ADR-006 — Sinks: Null first, WAV second, native later

Order: `NullSink` (tests/CI/benchmarks) → `WavSink` (first observable) →
WASAPI (first realtime, Windows) → CoreAudio/ALSA-PipeWire → JACK/ASIO.
Signal graph never names a sink: `renderer.render(graph, sink)`.

## ADR-007 — Minimal WAV in MVP, full renderer deferred

MVP includes a ~100-line deterministic `WavSink` (mono, 16-bit PCM, golden
`a4.wav`) purely as certification sink. No threads/clocks/drivers in MVP.
Sophisticated renderer (DSP graph, buffers, device clock) is post-MVP.

## ADR-008 — MVP scope freeze: Phases 0–2 + 2.5

Phase 0 (math+units) → Phase 1 (functions+time) → Phase 2 (signals+osc) →
Phase 2.5 (null+wav). MIDI, harmony, instruments, rooms, FFT framework,
plugins, GUI, realtime output are prohibited from shaping MVP APIs.

## ADR-009 — No premature generic math library

No Vector/Matrix/Quaternion in Phase 0. Math kernel grows only on
music/acoustics demand: scalar policy, complex, constants, safe fns,
interpolation, tolerance, domain checks first.

## ADR-010 — Inspectability + branched domains (amended)

Every object keeps `formula()`/`toExpression()`/`toJson()` through transforms.
Domains are **branched siblings** under a shared math foundation (time/signals,
symbolic music, physical models) joined by explicit adapters — replacing the earlier
single-chain sketch. Musical labels describe; physics defines; neither requires the other.

## ADR-011 — Time-function vs stateful-processor contracts

Two contracts, never one universal `sample(t)`. Time functions are pure and seekable;
stateful processors own initialized state, define reset, process sequentially, and report
unsupported backward seek as an error. Realtime processing boundary reserved
(prepare-then-process-into-supplied-buffers, bounded work) with no OS backend in MVP.

## ADR-012 — Frames authoritative; narrow certified audio path

Low-level render takes `FrameCount N` with `t_n = t_0 + n/fs`; duration overloads document
rounding; endpoint is `(N-1)/fs`; frames ≠ channel samples; max length declared + tested.
MVP certifies only constant-frequency sine with `f < fs/2`; other signals are labeled
supported / no-guarantee / rejected. Fixture peak is 0.25; RMS checked pre-quantization.

## ADR-013 — Tiered determinism, per-quantity tolerances, validity errors

Tier 1 (same build/config) identical; Tier 2 (other toolchains/platforms) within documented
tolerances; byte-identical WAV only for fixed encoding fixtures. No global epsilon.
Invalid values (e.g. `SampleRate{0}`, `cents(r ≤ 0)`) are defined errors; NaN/Inf fail renders.
`double`, no fast-math in correctness builds, toolchain/FP flags recorded.

## ADR-015 — Toolchain matrix: MSVC primary, GCC + Clang in CI from day one

Reference baseline: Visual Studio 2022 (v143 toolset family) on Windows.
Certification matrix: Windows/MSVC Debug+Release, Linux/GCC Debug+Release,
Linux/Clang Debug+Release plus one sanitizer configuration. GCC/Clang versions
pinned when CI is configured (deferred: no CI in repo yet). All certification
builds use explicit C++20 (never `c++latest`) with extensions disabled.
Rationale: discover portability problems while the quantity system is small.
Actual toolchain + FP flags are recorded per run (build/numerics.json).

Status update (M5, closed): fulfilled. CI workflow defines the full
matrix (MSVC Debug+Release, GCC-13 Debug+Release, Clang-18 Debug+Release,
ASan+UBSan job; pinned images; numerics artifacts). Locally verified:
MSVC 19.43 Debug+Release, UCRT64 GCC 16.2 Debug+Release, WSL Ubuntu 24.04
GCC 13.3.0 Debug+Release, Clang 18.1.3 Debug+Release, and GCC ASan+UBSan —
all green including probes, zero sanitizer reports. First GCC run caught a
real unspecified-evaluation-order bug invisible on MSVC (see 13-m5-ci.md).
Only hosted-runner execution remains unproven, which no local machine can
prove; the workflow mirrors the verified local runs exactly.

## ADR-016 — Catch2 v3, test-only, pinned

Core ships zero third-party dependencies. Tests use one pinned Catch2 v3
(commit hash, not moving tag; current pin recorded in CMakeLists.txt).
FetchContent only under BUILD_TESTING=ON; BUILD_TESTING=OFF bypasses all
fetching. CATCH2_SOURCE_DIR supports offline builds from local source.
No homegrown test framework alongside it. Warning policies are target-scoped
to our test executable, never imposed on Catch2 targets or consumers.

## ADR-017 — CMake 3.25 floor

Minimum 3.25 (try_compile SOURCES probes, NO_CACHE-class features); newer
installed versions kept. All settings target-scoped; no global flags.

## ADR-018 — Operator table, canonical storage, Result errors, uint64 counters

The tag alone does not define arithmetic: a fixed operator table lists input
types, output type, and failure conditions (TimePoint-TimePoint->Duration,
TimePoint+/-Duration->TimePoint, Duration+/-Duration->Duration,
Frequency/Frequency->FrequencyRatio, AngularFrequency x Duration->Phase,
toAngular(Frequency)->AngularFrequency). Unlisted operations have no overload —
notably NO Frequency x Duration -> Phase (would drop 2*pi) and NO
TimePoint+TimePoint. Each quantity has a documented canonical representation
(seconds, cycles/s, rad/s, radians, frames/s, positive ratios, signed cents,
uint64 frames); alternative units enter via named conversions only.
Fallible creation and conversion return project-owned
`Result<T, QuantityError>` (C++20 has no std::expected):
SampleRate<=0 and FrequencyRatio<=0 rejected at creation; period(f<=0) is a
domain error; every operation checks overflow/underflow (range error).
FrameCount/FrameIndex are uint64 with exact equality; negative/fractional
inputs rejected pre-conversion; increment-overflow and decrement-below-zero
are defined failures. Index->time conversion is approximate: the counter
maximum is not the maximum supported render length.
Comparison: == is exact; nearlyEqual uses |a-b| <= max(eps_abs,
eps_rel*max(|a|,|b|)) with unit-carrying absolute tolerance (a Duration for
TimePoints) and dimensionless relative tolerance; invalid tolerances throw;
non-finite operands compare true only when exactly equal. No global epsilon.

## ADR-014 — Canonical docs + verification-only claim

`FOUNDATION.md`, `NUMERICS.md`, `VALIDATION.md` are canonical with
accepted/proposed/deferred labels and override older notes on conflict.
First release claims only "a verified mathematical signal kernel with offline PCM export".
Solver names are methods, not accuracy ranks; solver↔object compatibility is explicit;
unknowns stay unknown. Independent-decoder verification allowed outside zero-dep core.
