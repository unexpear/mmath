# D13 — Clamped and Free Stiff Ends (complete)

Status: implemented · suite total recorded in the D13–D16 matrix run (see
`31-d16-measured-data.md`) · Probes: 10/10

Closes the step-3 item "clamped or free stiff ends". D10 supported only
hinged ends, where the modes are closed-form sines. Real strings and bars
are clamped (e.g. a string's end in a bridge pin, approximately) or free
(a bar's ends).

## Model

Per end, a `StiffEnd`:

| End | Boundary | Conditions |
|---|---|---|
| Hinged | Fixed | u = 0, u_xx = 0 |
| Clamped | Fixed | u = 0, u_x = 0 |
| Free | Free | u_xx = 0 and κ²u_xxx − c²u_x = 0 (zero moment, zero net shear with tension) |

## Continuous modes: the boundary determinant

X(x) = a₁e^(−αx) + a₂e^(−α(L−x)) + a₃cos βx + a₄sin βx solves
κ²X⁗ − c²X″ = ω²X, with α² = β² + c²/κ² and ω² = κ²β⁴ + c²β². The two
conditions at each end form a 4×4 determinant in β.
`stiffModeWavenumbers` scans it for sign changes (step π/64L), then
bisects to machine resolution. The **decaying-exponential basis** keeps
every entry bounded; cosh/sinh overflow and are ill-conditioned at piano
values of αL ≈ 100.

`modeFrequency` uses these roots for clamped and free ends. Only σ₀ loss
separates by mode there, so σ₁ > 0 on those ends is `DomainError`.

## FDTD: ghost rows

Two ghost nodes per side, second-order centered:

| End | Ghost values |
|---|---|
| Hinged | u₋₁ = −u₁, u₋₂ = −u₂ |
| Clamped | u₋₁ = +u₁, u₋₂ = +u₂ |
| Free | u₋₁ = 2u₀ − u₁; u₋₂ = u₂ − 2u₁ + 2u₋₁ − q(u₁ − u₋₁), q = c²Δx²/κ² (mirrored at the right end) |

The free-end node is an unknown. Under σ₁ loss, its implicit row is just
(1+s)·u₀, because D·u = 0 there by the boundary condition. **These rows
aren't derived from an energy identity**, so their stability is
established by long runs and their accuracy by FFT-measured convergence,
as below. The hinged path is untouched (bit-identical to D10).

## Solver compatibility, stated by rejection

`ModalSolution` (it needs closed-form sin/cos modes), `fdtdDiscreteMode`
(no exact discrete eigenvectors), analytical and waveguide all return
`DomainError` for clamped/free stiff ends. `modeFrequency`/`modeDecayRate`
reject σ₁ loss there.

## Evidence

- **Beam limit vs classical Euler–Bernoulli roots**, computed
  independently in the test by bisection on cos x·cosh x = ±1 and
  tan x = tanh x (tension 10⁻⁹ of the bending scale), matching to 1e-12:
  - cantilever: 1.87510407, 4.69409113
  - clamped-clamped and free-free: 4.73004074, 7.85320462
  - pinned-free and clamped-pinned: 3.92660231, 7.06858275

  The first roots are also checked against the textbook literals.
- **Hinged through the determinant** equals nπ/L for n = 1–12 to 1e-12.
- **Fletcher 1964, clamped-clamped**: the determinant against
  n·f₀·√(1+Bn²)·(1 + 2√B/π + 4B/π²). The residual grows about 31× per
  decade of B (asserted 25–40×), **the B^1.5 order of the terms Fletcher
  drops**, and is 1.1e-5 at B = 10⁻³. The clamping correction over hinged
  ends matches 2√B/π within 5%.
- **FDTD convergence** (c = 20, κ = 0.3, μ = 0.25 fixed, 4 s windows,
  M = 40/80/160), modes 2 and 3 of clamped-clamped, free-free and
  clamped-free: final error ratios **3.67–3.87** (asserted 3.3–4.7), with
  the finest-grid error below 0.5% of frequency.
- **Stability at the limit** (λ² + 4μ² = 0.99, 200,000 steps): clamped-clamped,
  free-free, clamped-free and hinged-free all stay bounded, with peak |u|
  ≤ 1.21e-3 from a 1e-3 start.
- **Loss**: σ₀ on clamped/free ends gives modeDecayRate = σ₀ and damped
  frequency √(ω² − σ₀²). A σ₀ + σ₁ run decays (the late peak is below
  half the early peak).

## Caught during implementation

1. **A quoting trap**: the C++ comment `X''''` broke the shell heredoc and
   would also have ended Python's `'''` strings. Patches were moved to
   script files with double-quoted strings.
2. **Refusals that were physics, not bugs**: the first convergence test
   had the estimator refuse partials.
   - The clamped-free mode-3 refusals occurred **at every pickup
     position**, so a node-placement explanation didn't fit. The spectrum
     was inspected before anything was changed: it showed only the
     determinant's modes, and no spurious ones. Clamped-free mode 3 sat
     at −81 dB because the pluck at 0.37 is on that mode's node (≈ 0.36).
   - A pickup at 0.75 sits near the free-free mode-2 node (≈ 0.78).

   Pluck 0.29 and pickup 0.65 clear all of them. The measured errors were
   identical across pickups, confirming they come from the grid, not the
   measurement.

## Deferred

Modal solutions for clamped/free ends (numerical mode shapes and
projections), σ₁ loss on those ends, and energy-derived boundary rows.
