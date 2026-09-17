# D15 — Damping and Stiffness in the Waveguide (complete)

Status: implemented · suite total recorded in the D13–D16 matrix run (see
`31-d16-measured-data.md`) · Probes: 10/10

Closes the step-3 item "damping and stiffness in the waveguide solver".
D8's `Waveguide1D` remains the **exact** lossless rung and still refuses
lossy or stiff objects. `TunedWaveguide` is an **approximating** rung, and
its approximation error is computed exactly and reported per partial.

## Structure

Two rails of M intervals (Δt = 1/fs). Both ends reflect with −1 (a
string). At the bridge end, the arriving wave passes through
F(z) = T(z)·D(z)^K·H(z):

| Section | Transfer function | Role |
|---|---|---|
| T | (a + z⁻¹)/(1 + az⁻¹) | Thiran fractional delay (D14) |
| D | (q + z⁻¹)/(1 + qz⁻¹), K stages, q ∈ (−1, 0) | stiffness dispersion |
| H | g(1−b)/(1 − bz⁻¹) | frequency-dependent loss |

## Exact loop partials

Each partial is a root of **1 − F(z)·z^(−2M) = 0**. `loopPole(k)` finds it
by complex Newton, seeded from the lossless phase condition
2Mω − arg F(e^(iω)) = 2πk. It uses the analytic log-derivative of F and
checks the residual and the branch. Frequency = arg z · fs/2π; decay =
−ln|z| · fs. These are what the rendered waveguide does, which the FFT
confirms. They are **not** assumed to equal the model.

## Design against the continuous model (`designTunedLoop`)

- **Fundamental**: exact. Thiran delay plus secant correction on the
  exact pole.
- **Loss**: the one-pole's b and g are solved so the exact poles' decay
  equals the model's σ₀ + σ₁k² **at partials 1 and P**. It iterates,
  because the loss filter's phase moves the tuning and its magnitude
  moves the poles.
- **Stiffness**: K identical allpasses. q is found by a coarse scan plus
  golden refinement to **minimize the worst cents error** over partials
  1…P. It's an approximation; the residual is reported.
- The **design report** lists every fitted partial's exact pole frequency
  and decay with its cents and relative decay error against the model.

## Evidence

- **Lossy loop** (0.6523 m, illustrative T60 3 s at 240 Hz and 0.4 s at
  2400 Hz, P = 10):
  - Fundamental exact (< 1e-6 cents).
  - Decay matched at partials 1 and 10 (< 1e-5 relative).
  - The **worst decay error in between is 3.2%**: the one-pole's shape
    can't follow σ₀ + σ₁k² exactly. Stated, and asserted < 4%.
  - FFT-measured decay of partials 1 and 4 equals the exact pole decay
    (asserted 1%).
- **Stiff loop** (B = 4·10⁻⁴, P = 10):
  - Worst partial error **2.40 cents with K = 2** and **1.07 cents with
    K = 8** (asserted: K = 8 is better and < 1.2 cents). Partial 8 tracks
    the model's +21.9-cent stretch to within 1.2 cents.
  - Rendered partials 1, 4 and 8 equal the exact poles to < 5e-5 Hz.
- **Reset**: 500-step lossy replays are bit-identical.
- **Validation**: a gain > 1 is rejected; P < 2 is rejected; a stiff
  system with no dispersion stages is refused (it would ignore the
  stiffness); pipes are refused (the loop is a string); k = 0 is refused.

## Model card

The loop reproduces the continuous model **approximately**. Accuracy is
the stated per-partial error. It's also verified against its own exact
poles, and not validated against a physical string. Filters are lumped
at one termination, so pickup positions are rail-grid positions, not
exact physical points of the tuned length.

## Deferred

Higher-order Thiran (flatter delay, lower partial stretch), a
higher-order loss filter (closer σ₁k² tracking), and dispersion filter
design by established optimizers (e.g. Abel–Smith) rather than a 1-D
search.
