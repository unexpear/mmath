# D17 — 2-D/3-D, FEM, and Coupled Objects (complete)

Status: implemented · suite total recorded at the D17 matrix run (status
update at the end of this record) · Probes: 10/10

Closes the last step-3 item, "2D/3D, FEM, and coupling objects together",
with one verified instance of each. Together with D11–D16, **roadmap
step 3 (waves → pressure) is complete** at the scope stated in each
milestone's record.

## New code

| Header | Contents |
|---|---|
| `physics/fdtd_box.h` | `BoxGrid` (2-D or 3-D, equal spacing h, Dirichlet or Neumann walls); `boxModeFrequency` (Rayleigh); `boxDiscreteModeFrequency` (exact grid dispersion); `boxModeInitial`, `boxBumpInitial`; `FdtdBox` (leapfrog, CFL λ ≤ 1/√D enforced) |
| `physics/fem1d.h` | `femStringFrequencies` (linear elements, consistent mass); `femStiffFrequencies` (Hermite cubics: bending + tension + consistent mass, any `StiffEnd`); eigenvalues by Sylvester-inertia bisection on the banded pencil |
| `physics/coupled_string.h` | `BridgeOscillator{m, k, r}`; `makeCoupledString`; `coupledModeFrequencies` (transcendental equation); `CoupledStringFdtd` (half-cell bridge node, damping implicit, discrete `energy()`) |

## 2-D and 3-D (`fdtd_box.h`)

- **Objects**: a rectangular **membrane** (Dirichlet edges; transverse
  displacement) and a rectangular **room** (rigid Neumann walls; acoustic
  pressure).
- **Modes**: Rayleigh f = (c/2)·√(Σ(mᵢ/Lᵢ)²). **Exact grid dispersion**
  follows the D12 eigenvector argument per axis:
  cos(ωΔt) = 1 − (λ²/2)·Σ 4sin²(kᵢh/2).
- **Stability**: λ ≤ 1/√D, since pᵢ ≤ 4 on each axis. Larger values are
  `DomainError`.

## FEM (`fem1d.h`)

- **Linear elements**: K_e = (c²/h)[1 −1; −1 1], M_e = (h/6)[2 1; 1 2].
  The spectrum on a uniform mesh has the closed form
  ω² = (6c²/h²)(1 − cos kh)/(2 + cos kh).
- **Hermite cubics** (w, w′ per node): the bending, tension (geometric)
  and consistent mass matrices are in the header comment.
  - Hinged removes w; Clamped removes w and w′; Free removes nothing
    (natural conditions of the weak form).
- **Eigenvalues**: with M positive definite, the number of negative pivots
  of K − sM (banded LDLᵀ) equals the number of eigenvalues below s. Each
  eigenvalue is bisected to machine resolution; rigid-body zeros are
  skipped.
- FEM here is a **modal** rung (frequencies), which makes it an
  independent third method.

## Coupling (`coupled_string.h`)

- **Model**: a string fixed at x = 0, with its end at x = L attached to a
  mass–spring–damper bridge: m·y″ = −T·u_x(L) − k·y − r·y′, with y = u(L).
- **Modes (lossless)**: T·β·cos βL + (k − mω²)·sin βL = 0, with ω = cβ.
- **FDTD bridge node** carries half a string cell of mass,
  m_eff = m + μh/2. Damping is centered and implicit.
- **Stability, derived as a sufficient condition**: Gershgorin bounds the
  spectral radius by the largest row sum, which is 4c²/h² for interior
  rows (λ ≤ 1) and (2T/h + k)/m_eff for the bridge row. Creation requires
  Δt²·(2T/h + k)/m_eff < 4.
- **Discrete energy**: string kinetic + bridge kinetic (m_eff) + T/(2h)
  Σ D⁺uⁿ⁺¹·D⁺uⁿ + (k/2)·yⁿ⁺¹yⁿ.

## Evidence

**Box FDTD**
- **FFT measurement equals the exact grid prediction**: membrane modes
  (1,1), (2,1), (1,2) and room modes (1,0,0), (0,1,0), (1,1,0), (1,1,1) all
  to ≤ 6.3e-7 Hz (asserted 1e-5).
- **Second-order convergence**: the membrane's discrete-to-continuous gap
  shrinks by **4.00×** per halving of h, in all three modes (asserted
  within 1%).
- **Eigenvector preservation**: a (2,3) membrane mode after 555 steps is
  proportional to sin·sin at every node (< 1e-15).
- **Energy**: a membrane bump over 3000 steps drifts < 1e-10.
- **Cross-domain anchor**: the room's rigid-wall axial modes n·c/2L equal
  D2's `openPipeMode` for n = 1–3 (1e-12).
- **CFL**: accepted at 0.7071 (2-D) and 0.5773 (3-D); rejected at
  0.7072 and 0.5774.

**FEM**
- **Linear elements equal the closed-form dispersion** for modes 1–5 at
  E = 20 and 40 (1e-12; measured ≤ 5e-15). They converge from above at
  O(h²) (ratio 4.00).
- **Hermite elements vs D13's determinant** for clamped-clamped, free-free
  and clamped-free:
  - Errors are positive (Rayleigh–Ritz, from above).
  - Convergence ratios run **12.3 → 14.8 → 15.7**, approaching the Hermite
    rate **16 (h⁴)**. The rate appears only once h ≪ κ/c (the boundary
    layer); at E = 4–32 the ratios were 3.3–8.1. The test asserts
    14.5–17 at E = 128/256.
  - **Clamped and free stiff-end modes now agree across three independent
    methods** (determinant, FDTD, FEM), to about 1e-6 relative at E = 256.
- **Free-free linear FEM** skips the rigid translation, and its first
  elastic mode equals c/2L to 1e-3.

**Coupling**
- **Limits are exact**: k → ∞ gives 240/480/720 Hz (fixed-fixed); m = k = 0
  gives 120/360/600 Hz (fixed-free).
- **Mode repulsion**: a 5 g, 2·10⁴ N/m bridge (318.31 Hz alone) pushes
  the string's 240 Hz mode down to 236.07, pushes 480 up to 482.78, and
  adds a bridge mode at 321.09 Hz.
- **FDTD converges** to those four roots with ratios **4.00–4.01**
  (asserted 4 ± 5%).
- **Energy**: without damping, the drift is ≈ 1e-13 over 20,000 steps
  (asserted < 1e-10). With bridge damping it is monotone non-increasing and
  falls below half its start.
- **Stability rejection**: an over-stiff bridge (10⁹ N/m on 0.1 g) is
  refused, as is Courant > 1.

**Demo** `objects_demo`:
- Membrane partials are measured at 48 kHz against the grid prediction.
- The string+bridge note decays **through the bridge alone** (peak
  1.37e-3 → 2.83e-4, with no fade).
- The cantilever's βL = 1.875104, 4.694091, 7.854757 match between the
  determinant and FEM(64).
- Output: `objects.wav`.

## Model card

- **Membrane**: lossless, no stiffness, no air loading.
- **Room**: ideal rigid walls, no absorption.
- **Bridge**: a lumped single-degree-of-freedom oscillator, one
  polarization, no soundboard, no radiation.

All verified against their models; none validated against measurement.

## Deferred, with reason

- Circular and irregular membranes (they need staircase or embedded
  boundaries, with their own convergence evidence).
- Wall absorption and air loss in the room (ISO absorption from D2 exists,
  but a boundary-impedance model needs its own derivation).
- 2-D/3-D FEM, and FEM time integration (modal rung only here).
- Two-string coupling through a shared bridge, and soundboard models.
