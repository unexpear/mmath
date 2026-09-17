# D14 — Fine Pitch Tuning Between Grid Points (complete)

Status: implemented · suite total recorded in the D13–D16 matrix run (see
`31-d16-measured-data.md`) · Probes: 10/10

Closes the step-3 item "fine pitch tuning between grid points". An
audio-rate grid (Δt = 1/fs) can only hold integer M intervals or integer
delay lengths. D8 therefore refused off-grid strings, and D12 showed the
grid detunes. Two tools now tune continuously, one per solver.

## FDTD: pre-warping (`fdtd1d.h`)

`fdtdPrewarpForPitch(system, M, fs, n)` solves D12's exact dispersion
relation **backwards** for the Courant number that puts mode n exactly
on the model pitch, with Δt = 1/fs and Δx = L/M:

    λ² = (2 − μ²p² − 2·cos(ωΔt)·√(ac)) / p,   a = 1+s+rp, c = 1−s−rp

It returns the same object with wave speed c′ = λ·Δx·fs, which runs at
exactly Δt = 1/fs. Mode n is exact; other modes keep D12's dispersion,
now measured relative to an exact mode n. `DomainError` if λ ∉ (0, 1],
if λ² + 4μ² > 1, or if the ends are clamped/free (no closed-form discrete
modes).

## Waveguide: fractional delay (`tuned_waveguide.h`)

A first-order **Thiran allpass** T(z) = (a + z⁻¹)/(1 + az⁻¹) at the bridge
termination supplies the non-integer part of the loop delay. The
fundamental is tuned on the loop's **exact pole** (see D15), not on a
phase approximation.

## Evidence

- **Pre-warped FDTD**, off-grid 0.6523 m string, in ideal, stiff
  (B = 4·10⁻⁴) and stiff + lossy versions, on M = 60 and M = 70:
  - D12's prediction for mode 1 equals the model pitch to ≤ 5.2e-14
    relative (asserted 1e-12).
  - Δt·fs = 1 to 1e-12.
  - **FFT-measured mode 1 within ≤ 2.1e-7 Hz** of the model (asserted
    1e-5).
  - Mode 2 is left at −0.38 cents (M = 60) and −0.22 cents (M = 70).
  - Rejections: M = 140 needs λ > 1 (`DomainError`), and clamped ends are
    refused.
- **Pole-solver anchors**:
  - Thiran a = 0 is a pure one-sample delay: poles at exactly
    k·fs/(2M+1) (to 1e-12).
  - A pure loop gain g gives decay exactly −ln g · fs/(2M+1) (to 1e-9).
- **Tuned ideal loop** (0.6523 m):
  - The fundamental is exact (asserted < 1e-6 cents).
  - The first-order Thiran's phase delay falls with frequency, so the
    partials go **monotonically sharp**, stated at **+1.09 cents by
    partial 10**.
  - Rendered output agrees with the exact poles for partials 1–9 to
    ≤ 9.6e-6 Hz.

## Caught during implementation

1. **Measurement at a node again**: a pickup at node M/7 sits on mode 7's
   node, so partial 7 disagreed with its pole by up to 2.5e-3 Hz. The
   pickup moved to 0.1234·M.
2. **Leakage from strong partials**: even off-node, partial 7 (30+ dB below
   partial 1) was biased by 9e-5 to 1e-3 Hz at N = 16384. The hypothesis
   predicted a sharp drop with a 4× longer window, and it did: ≤ 1e-5 Hz
   at N = 65536. Frequency-vs-pole tests use long windows on lossless
   designs. Lossy higher partials decay below partial 1's leakage in long
   windows, and the estimator refuses them, as it should.
3. **The lossy fundamental** was first tuned on the lossless phase
   condition and missed by 6e-4 cents. It is now tuned by secant
   iteration on the exact pole.
