# D8 — The 1-D Wave Equation on Four Solver Rungs (complete)

Status: implemented · 199/199 on MSVC Release+Debug, UCRT64 GCC
Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan
(zero reports) · Probes: 10/10

First solvers of roadmap step 3 (waves → pressure). One object — the
bounded, lossless 1-D wave equation — solved by four methods, per the
ladder's rule "object stays constant; only solver swaps". Strings
(displacement) and pipes (pressure) are the same object with different
boundaries.

## New code

| Header | Contents |
|---|---|
| `physics/wave1d.h` | `WaveSystem1D` (L, c, Fixed/Free ends) + objects `idealString`, `openPipe`, `stoppedPipe`; `modeFrequency`; `InitialShape` (pluck, single mode; zero initial velocity; carries its system); **analytical** `analyticField` (d'Alembert with reflections); **modal** `ModalSolution` with `truncationBound` |
| `physics/fdtd1d.h` | **FDTD** `Fdtd1D` (explicit leapfrog, Courant ∈ (0, 1], mirror-ghost Free ends, reset, energy access); `gridIntervalsForRate` |
| `physics/waveguide1d.h` | **Waveguide** `Waveguide1D` (two rails, −1/+1 end reflections) |

## Model card (VALIDATION.md)

- **Model**: u_tt = c²u_xx on [0, L]; linear, lossless, dispersionless,
  uniform c. Fixed = Dirichlet (string end; open pipe end, p = 0).
  Free = Neumann (closed pipe end, rigid wall).
- **Supported**: any L, c > 0; initial shapes pluck/single-mode, zero
  initial velocity. Generic shapes and initial velocity are deferred (each
  needs its own exact reference first).
- **Status**: VERIFIED against the model. UNVALIDATED against physical
  measurement. Known deviations of real instruments, not modeled: damping,
  stiffness (D1), end corrections (D2), radiation, nonlinearity.

## Contract points

- **Solver names are methods, not ranks**. Here, analytical, FDTD at
  Courant 1 and the waveguide are all *exact* at grid points. Modal is
  approximate with a stated bound. FDTD below Courant 1 is second-order.
- **Modal coefficients in closed form**. Integrating by parts twice with
  φ'' = −k²φ gives the pluck coefficients for all four boundary pairs
  from one formula. With it comes the bound
  |aₙ| ≤ 4|h|/(kₙ²x₀(L−x₀)), and tail sums L²/(π²N) or 2L²/(π²(2N−1)).
- **Stability is a rejection, not a warning**: Courant > 1 is
  `DomainError` (CFL) and never runs.
- **Audio-rate honesty**: `gridIntervalsForRate` returns `DomainError`
  unless L·fs/c is an integer, because a rounded grid would silently
  retune the object. Fractional delay is deferred.
- **Stateful processors** under the D5 contract: buffers allocated at
  creation, allocation-free O(M) `step()`, bit-identical `reset()`.
  Output is frame-indexed, not a continuous `Signal`.
- **Stated, not hidden**: pluck corners put energy above Nyquist in the
  sampled field (aliasing), and the lossless model rings forever. The
  demo applies a T60 fade at the output as a listening convenience,
  labeled as not physics.
- Deferred with reason: losses/loop filters (need a validated damping
  model), fractional delay, stiff strings on the grid, 2-D/3-D, FEM,
  coupling between objects.

## Evidence

- **Cross-domain agreement**: `modeFrequency` equals D2's independent
  `stringMode` / `openPipeMode`, and stopped-pipe mode n equals D2's
  `stoppedPipeMode(2n−1)`, to 1e-12.
- **Closed-form coefficients vs quadrature**: composite Simpson written
  in the test, split at the kink, n = 1..10 on all four boundary pairs.
  Agreement to 1e-9, and the Free-Free constant term is exactly h/2.
- **d'Alembert vs separation of variables**: mode 3 on all four boundary
  pairs, A·φ₃(x)·cos(ω₃t) to 1e-12.
- **Periods and reflections**: at t = L/c, same-type ends mirror the
  shape (negated for Fixed ends, not for Free). Mixed ends negate at
  2L/c and repeat at 4L/c.
- **Modal**: for N = 10, 100 and 1000, the error against d'Alembert is
  always within the stated bound and falls with N. A single-mode shape is
  exact (bound 0) once its mode is included.
- **Grid exactness**: FDTD at Courant 1 and the waveguide match
  d'Alembert on all four boundary pairs over 1000 steps to ≤ 1e-14, and
  each other to ≤ 1e-14.
- **Second order below Courant 1**: at Courant 0.5 with M = 40/80/160,
  error ratios fall in [3.5, 4.5].
- **Discrete energy conserved**: at Courant 0.7 over 5000 steps, the
  relative drift stays below 1e-10.
- **Odd harmonics as a time-domain fact**: the stopped pipe satisfies
  u(t + 2L/c) = −u(t) to 1e-14. The string at its half period is *not* a
  negation (difference > 1e-3), because its even harmonics are present.
- **Reset**: bit-identical 300-step replays on both stateful solvers.
- **Retuning refused**: an M = 100 string grid is accepted, while
  L = 0.651 m is `DomainError`.
- **Audible**: `solver_demo` renders a 240 Hz string (M = 100 at 48 kHz)
  through the waveguide, FDTD at Courant 1 and 40 modes, then a 133.3 Hz
  stopped pipe through FDTD (`solvers.wav`, 384044 bytes, all clean).
  Printed: max |waveguide − FDTD| = 2.4e-16 m; max |waveguide − modal| =
  1.7e-5 m against a stated bound of 1.3e-4 m.

## Caught during implementation

1. **Convergence blind spot**: the first order test sampled at t = L/c,
   where ωt = 3π sits on a cosine extremum. There the O(dx²) phase error
   shows up squared, and the measured ratio was 16 instead of 4, which
   looked like fourth order. Sampling at ωt = 1.5π, where the cosine has
   full slope, measures the true second order. The test now documents why.
2. **Encoding**: a Python-scripted edit wrote an em dash as a cp1252 byte
   into a UTF-8 source. Found by decoding every touched file as UTF-8
   before the matrix run (an MSVC `/WX` build could fail on it) and
   repaired. Edits are now checked for encoding.
