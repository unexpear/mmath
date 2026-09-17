# D9 — Frequency-Dependent Loss on the 1-D Wave System (complete)

Status: implemented · 207/207 on MSVC Release+Debug, UCRT64 GCC
Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan
(zero reports) · Probes: 10/10

Adds damping to the D8 object, so strings and pipes decay from the model
rather than from an output fade. Solver compatibility is explicit: modal
and FDTD support loss, analytical and waveguide reject it.

## Model

    u_tt + 2σ₀·u_t = c²·u_xx + 2σ₁·u_txx        σ₀ ≥ 0 [1/s], σ₁ ≥ 0 [m²/s]

This is the two-parameter frequency-dependent loss form used in
physical-modeling synthesis (Bilbao, *Numerical Sound Synthesis*, 2009,
lossy string). The constants below are **derived here**, not transcribed.
Each mode φₙ obeys `q'' + 2σₙq' + ωₙ²q = 0` with `σₙ = σ₀ + σ₁kₙ²`, so its
amplitude decays as `e^{−σₙt}` and **T60ₙ = 3·ln10/σₙ** (−60 dB = 10⁻³ in
amplitude).

## New and changed code

| Header | Change |
|---|---|
| `physics/wave1d.h` | `LossModel{σ₀, σ₁}` on `WaveSystem1D` (lossless by default); `withLoss`; `lossFromT60` (exact two-point fit); `modeDecayRate`, `modeT60`; `modeFrequency` returns the damped frequency, and `DomainError` for non-oscillating modes; `ModalSolution` uses exact damped mode factors (under-, critically and overdamped); `analyticField` rejects lossy systems |
| `physics/fdtd1d.h` | Implicit centered loss scheme (one Thomas tridiagonal solve per step, preallocated, O(M)); the lossless path is unchanged, so D8's exactness still holds |
| `physics/waveguide1d.h` | `create` rejects lossy systems (loop filters deferred) |
| `math/constants.h` | `ln10` |

## Evidence rules applied (VALIDATION.md)

- **No material defaults**: nothing in the library claims to be "a guitar
  string's loss". Parameters come only from caller data via `lossFromT60`.
  The demo's T60 pairs are labeled *illustrative, not measured*.
- **Unrepresentable data is reported, not clamped**: if treble would ring
  longer than bass (σ₁ < 0), or the fit gives σ₀ < 0, the result is
  `DomainError`.
- **Stated limits**: the fit takes the lossless kₙ = 2πf/c (the damping
  shift is not inverted, which is negligible for light damping and pinned
  < 1e-5 in the tests). A lossless mode has no finite T60 (`DomainError`),
  and a critically or overdamped mode has no frequency (`DomainError`).
- **Model status**: still verified against the model and unvalidated
  against measurement. The loss form is a common synthesis model, not a
  derived physical mechanism (air drag, internal friction and radiation
  are not separated).

## Numerical scheme and its stability (derived)

    (1+s)u^{n+1} − r·D u^{n+1} = 2u^n + λ²·D u^n − (1−s)u^{n−1} − r·D u^{n−1}
    s = σ₀Δt,  r = σ₁Δt/Δx²,  D = second difference with the D8 ghosts

Von Neumann analysis with D → −p, p ∈ [0, 4] gives the amplification
polynomial `(1+s+rp)z² − (2−λ²p)z + (1−s−rp)`. The Jury conditions reduce
to `λ²p ≤ 4`, i.e. **λ ≤ 1 for any σ₀, σ₁ ≥ 0**, the same condition as the
lossless scheme.

> **Correction (found in D12).** This record first gave the constant term
> as `(1−s+rp)`. The u^{n−1} terms are −(1−s)·u^{n−1} − r·D·u^{n−1}, and
> with D → −p the second becomes **+rp**, so the constant is `1−s−rp`.
> D12 caught it when FDTD decay rates, measured by FFT, disagreed with a
> prediction built on the wrong polynomial (mode 2: predicted 2.000/s,
> measured 2.560/s, continuous model 2.561/s). The stability conclusion is
> unchanged: with the corrected term a + c = 2 and |c| ≤ a always, so the
> condition is still exactly λ²p ≤ 4. No D9 code or test depended on the
> wrong term. The explicit variant with a backward difference on the σ₁
term would need `λ² + 4σ₁Δt/Δx² ≤ 1` and is only first order in time for
that term, so it is not used. Energy balance: with the D8 discrete energy,
the step changes it by `−sΣw² − rΣ(δw)² ≤ 0` (w = u^{n+1} − u^{n−1}), so
energy never increases.

## Evidence

- **T60 fit round trip**: fitting (240 Hz, 4 s) and (1200 Hz, 0.8 s) on a
  string gives `modeT60` of exactly 4 s for mode 1 and 0.8 s for mode 5
  (1e-12). `e^{−σ₁·T60}` = 10⁻³ (1e-12). T60 strictly decreases over
  modes 1–12. The mode-1 pitch drops, but by < 1e-5 relative.
- **Rejections**: inverted decay data gives `DomainError`; equal
  frequencies, zero T60, negative or NaN σ give `InvalidArgument`; a
  lossless `modeT60` gives `DomainError`.
- **Damped modal factor vs RK4** (an independent integrator in the test,
  20000 steps): underdamped (σ = 1), critically damped (σ = π = ω exactly)
  and overdamped (σ = 5) all agree to 1e-10. The damped frequency is
  `√(π²−1)/2π` (1e-14); critical and overdamped give `DomainError`.
- **Lossy FDTD converges to the exact damped modal solution at second
  order**: σ₀ = 0.8, σ₁ = 0.01, Courant 0.5, M = 40/80/160 → error ratios
  **4.004 and 4.001**.
- **Stability where the explicit variant would fail**: at σ₁ = 0.05 and
  Courant 1 and 0.7, the explicit stability quantity is 8.69 and 5.87
  (limit 1). The implicit scheme stays finite, with energy monotone
  non-increasing over 5000 steps and falling below half its start value.
- **Free-end ghost rows of the Thomas solve**: Free-Free, Free-Fixed and
  Fixed-Free agree with the damped modal solution to 3e-9 – 1.2e-7
  (asserted < 1e-6) at M = 400.
- **Reset**: a lossy Free-Fixed run replays bit-identically.
- **Compatibility**: `analyticField` and `Waveguide1D::create` return
  `DomainError` for lossy systems, while modal and FDTD accept them.
- **Regression**: every D8 test passes unchanged (the lossless path is
  untouched).
- **Audible**: `loss_demo` renders the D8 string (illustrative T60 3 s at
  240 Hz and 0.4 s at 2400 Hz) and stopped pipe (1.2 s at f₁ and 0.15 s at
  2 kHz), each by lossy FDTD and by 60 damped modes. No fade is applied.
  String |FDTD − modal| = 9.6e-6 (modal bound 8.4e-5), with a tail at 2 s
  of 0.0012. The pipe tail is 2e-6. Output is `loss.wav` (768044 bytes,
  all clean).

## Caught during implementation

- **Silent test runs**: several verification commands printed nothing.
  Exit code 127 showed the MinGW test binary had failed to launch (the
  UCRT64 runtime DLLs were not on PATH), not that it had passed. Rerun
  with the runtime on PATH, the numbers above were read from actual
  output. Lesson recorded: an empty result is not a pass.
- **A threshold too loose to mean anything**: the free-end agreement was
  first asserted at < 1e-5 while measuring ≤ 1.2e-7. It was tightened to
  < 1e-6 so the check can actually fail.
- **MSVC Release only: C4723 "potential divide by 0" under `/WX`**. Eight
  configurations were green, but MSVC Release failed to build.
  `lossFromT60` rejected T60 = 0 through a range-`for` over an
  `initializer_list`. After inlining the test's zero-T60 call, the
  optimizer could not connect that guard to the later `3·ln10/T60`
  division. The code was correct, but the build broke. Fixed with plain
  per-value `if` guards that visibly dominate each division, then the
  whole matrix was rerun from clean. This is exactly the kind of result
  the multi-compiler matrix exists to catch.

## Deferred, with reason

Waveguide loop filters (they must be designed to match σ₀ and σ₁ per
frequency), stiffness on the grid (D1's κ term, which needs a fourth
difference and new stability analysis), measured loss data for real
instruments (needs sources), and separate physical loss mechanisms.
