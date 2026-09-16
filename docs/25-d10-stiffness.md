# D10 — Hinged Bending Stiffness on the Solver Grid (complete)

Status: implemented · 215/215 on MSVC Release+Debug, UCRT64 GCC
Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan
(zero reports) · Probes: 10/10

Joins D1 (stiff-string *spectra*, signal domain) to D8/D9 (wave *solvers*,
physics domain): the physics now produces the stretched partials that D1
described, and the two agree exactly.

## Model

    u_tt + 2σ₀u_t = c²u_xx − κ²u_xxxx + 2σ₁u_txx      κ² = EI/(ρA)  [κ in m²/s]

Supported ends are **Fixed-Fixed only, read as hinged** (simply
supported): u = 0 and u_xx = 0. The mode shapes stay sin(nπx/L), and

    ωₙ² = c²kₙ² + κ²kₙ⁴   ⇒   fₙ = n·f₀·√(1 + B·n²),  f₀ = c/2L,  B = κ²π²/(c²L²)

which is **exactly** D1's Fletcher formula. Helpers: `withStiffness`,
`stiffnessFromB` (κ = √B·c·L/π) and `inharmonicityB`.

## Evidence rules applied

- **Boundary honesty**: clamped ends (real piano strings) add Fletcher's
  correction factor, and stiff free ends need u_xx = u_xxx = 0. Neither is
  built. Stiffness on any non-Fixed-Fixed system is `DomainError`.
- **No material defaults**: the demo's B = 4·10⁻⁴ sits inside the piano
  range D1 cites, and is labeled illustrative for this string, not
  measured.
- **Solver compatibility**: analytical and waveguide reject stiff systems.
  Stiffness makes waves dispersive, so there is no d'Alembert form and no
  plain delay line. Modal and FDTD accept them.
- **Model status**: verified against the model, unvalidated against
  measurement.

## FDTD scheme and stability (derived)

The explicit term −μ²·D²uⁿ, with μ = κΔt/Δx², is added to both the
lossless and the implicit-loss schemes. Odd ghosts u₋₁ = −u₁ and
u_{M+1} = −u_{M−1} give D u = 0 at the end nodes. The start step adds
−μ²/2·D²u⁰, from u_tt(0) = c²u_xx − κ²u_xxxx.

Von Neumann with D → −p, p ∈ [0, 4]: lossless, z + 1/z = 2 − λ²p − μ²p²,
which is bounded iff λ²p + μ²p² ≤ 4, i.e.

    λ² + 4μ² ≤ 1

This is equivalent to Bilbao's Δx² ≥ (c²Δt² + √(c⁴Δt⁴ + 16κ²Δt²))/2. With
loss the Jury condition relaxes to λ²p + μ²p² ≤ 4 + 2rp, so the same
condition suffices. It is enforced in `Fdtd1D::create` (`DomainError`).

**No exact Courant-1 grid exists for a stiff string**, so D8's
`gridIntervalsForRate` cannot apply. `stableGridForRate` instead returns
the finest grid that is stable at Δt = 1/fs. It repeats `create`'s own
arithmetic, so a grid it accepts is never rejected by roundoff.

## Evidence

- **Cross-domain anchor**: a hinged stiff string built from B = 4·10⁻⁴
  has `modeFrequency` equal to D1's `partialFrequency` for n = 1..20 to
  1e-12. `inharmonicityB` round-trips to 1e-12, and mode 10 is stretched
  to 2447.5 Hz (ideal 2400).
- **Scope**: stiffness on a stopped pipe or Free-Free system is
  `DomainError`; κ = 0 is accepted anywhere; negative or NaN κ or B give
  `InvalidArgument`.
- **Stiff modal vs hand dispersion relation**: mode 4, A·sin(kx)·cos(ωt)
  with ω = √(c²k² + κ²k⁴), to 1e-12.
- **Second-order convergence** (refined at fixed μ = 0.25, Δt ∝ Δx², equal
  end times): error ratios **3.67 then 3.92**, approaching 4 as the grid
  refines. The coarsest grid, M = 40, is not yet fully asymptotic, so the
  band [3.5, 4.5] is asserted.
- **Energy with the bending term** (written in the test): lossless drift
  1.2e-12 over 5000 steps at λ² + 4μ² = 0.9. With loss it is monotone
  non-increasing and ends far below half its start value.
- **Stiff + lossy FDTD vs the exact stiff damped modal solution**:
  3.8e-6 (asserted < 1e-5).
- **Stability limit at its boundary**: λ* = 1/√(1 + 4a²) is accepted at
  λ*(1 − 1e-12) and rejected at λ*(1 + 1e-9). The same Courant number is
  fine without stiffness.
- **Audio-rate grid**: the stable grid for the B = 4·10⁻⁴ string at
  48 kHz is M = 73 (Courant 0.73, Δt = 1/48000 to 1e-12). M + 1 is
  unstable. With no stiffness, the helper returns D8's M = 100.
- **Regression**: every D8 and D9 test passes unchanged (the non-stiff
  code paths are literally untouched).
- **Audible**: `stiff_demo` prints the stretch table (+0.35 cents on
  partial 1 up to +21.88 cents on partial 8) and renders ideal lossy FDTD,
  stiff lossy FDTD (M = 73) and 60 stiff damped modes, all picked up at
  the same position. Output is `stiff_string.wav` (576044 bytes, all
  clean). |stiff FDTD − modal| = 8.8e-5 m on a 2 mm pluck. That mixes the
  modal truncation bound (8.4e-5) with FDTD discretization at this coarse
  grid, and is printed without separating the two.

## Caught during implementation

1. **Output collision**: the first demo wrote `stiff.wav`, which D1's
   `stretch_demo` already produces. Running both would silently overwrite
   one. The output is now `stiff_string.wav`.
2. **An apples-to-oranges comparison**: the first demo picked up the ideal
   string at 0.0845 m but the stiff string at 0.13 m (the pluck point).
   The stiff note's larger peak (0.298 vs 0.195) came from the pickup
   position, not from stiffness. Both are now read at the same position
   (the nearest node, with the offset printed), and the peaks are 0.192
   and 0.195.
3. **An overclaim in printed output**: "the rest is FDTD dispersion"
   attributed an unseparated error to one cause. It now says what is
   known: truncation bound plus discretization, not separated.
4. **Build hazards removed before the matrix run**: an unused variable
   left behind by the modal edit (fatal under `-Werror`), and the D9
   model-card text, which had been joined into a run-on line and still
   said "no damping, stiffness". It was rewritten.

## Deferred, with reason

Clamped and free stiff ends (different boundary rows and a Fletcher
correction to verify), an implicit stiffness scheme or higher-order
stencils to cut dispersion at audio rates (needs its own convergence and
tuning evidence), per-partial tuning error of FDTD at audio rates
(awaits the FFT milestone to measure), and stiffness in waveguides
(dispersion filters).
