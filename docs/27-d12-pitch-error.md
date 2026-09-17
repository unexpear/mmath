# D12 — FDTD Pitch and Decay Error at Audio Rates (complete)

Status: implemented · 229/229 on MSVC Release+Debug, UCRT64 GCC
Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan
(zero reports, 17 examples clean) · Probes: 10/10

Closes the step-3 item "FDTD pitch error at audio rates, which needs an FFT
to measure". Every grid's per-mode pitch and decay error is now **predicted
in closed form** and **confirmed by measuring rendered solver output** with
D11's estimator.

## New code

| Header | Contents |
|---|---|
| `physics/fdtd1d.h` | `fdtdDiscreteMode(system, M, courant, n)` → exact numerical frequency and decay rate of mode n on that grid; `fdtdPitchErrorCents` → 1200·log₂(f_grid / f_model) |

## Derivation

On this grid the continuous mode shapes are **exact eigenvectors** of the
discrete operators:
- D·φₙ = −p·φₙ with p = 4·sin²(kₙΔx/2), for the sin family (Fixed left)
  and the cos family (Free left, mirror ghost) alike.
- D²·φₙ = p²·φₙ under the hinged ghosts.

Substituting uⁿ = zⁿ·φₙ into each scheme:

    lossless:  cos(ωΔt) = 1 − (λ²p + μ²p²)/2
    lossy:     (1+s+rp)z² − (2 − λ²p − μ²p²)z + (1−s−rp) = 0
               |z| = √((1−s−rp)/(1+s+rp)),
               cos(ωΔt) = (2 − λ²p − μ²p²) / (2√((1+s+rp)(1−s−rp)))

Frequency is ω/2π and decay rate is −ln|z|/Δt. For the ideal string at
λ = 1: cos(ωΔt) = cos(kΔx), so ω = ck exactly. That is D8's grid
exactness, recovered from the general formula.

## Evidence

- **Zero error where theory says zero**: the ideal string at M = 100,
  Courant 1, modes 1–20, is ≤ 7.6e-11 cents (asserted < 1e-9).
- **Eigenvector claim, directly**: after 777 steps, single-mode runs stay
  proportional to φₙ at every node (asserted < 1e-14), both for a stiff
  lossy Fixed-Fixed string and for a Free-Fixed system.
- **FFT measures the discrete frequency, not the continuous one**
  (Courant 0.7, M = 70 at 48 kHz): modes 1, 5 and 12 agree with the
  prediction to ≤ 6.6e-8 Hz (asserted < 1e-6 Hz). Mode 12 sits **18.0 Hz**
  below the continuous frequency, and its predicted error is below −1 cent.
- **Stiff audio-rate grid** (B = 4·10⁻⁴, M = 73): modes 1, 4 and 9 agree
  with the prediction to ≤ 2.7e-8 Hz.
- **Decay**: on a lossy M = 70 grid, amplitudes over two equal
  4096-sample windows 8192 frames apart match the predicted numerical
  decay:
  - Mode 2: predicted 2.560264/s, measured 2.560263/s.
  - Mode 9: predicted 13.199449/s, measured 13.199449/s.

  Asserted to 1e-4 relative. The continuous model gives 13.353/s for mode 9,
  so the grid's own decay error (−1.2%) is quantified, not assumed away.
- **Rejections**: n = 0; n = M on Fixed-Fixed (the zero grid function);
  Courant > 1; M < 2. n = M on Free-Free is accepted.
- **Demo** `pitch_error_demo`: renders one pluck per grid and measures
  partials 1–10. It fails its own run if any measured partial is more
  than 1 mHz off its predicted grid frequency; the worst is 4.0e-5 Hz.

  | Grid | Partial 1 | Partial 5 | Partial 10 |
  |---|---|---|---|
  | ideal M = 100, Courant 1 | 0.00 ¢ | 0.00 ¢ | 0.00 ¢ |
  | coarse M = 70, Courant 0.7 | −0.07 ¢ | −1.86 ¢ | −7.49 ¢ |
  | stiff B = 4·10⁻⁴, M = 73 | −0.06 ¢ | −1.58 ¢ | −6.54 ¢ |

  On the coarse grid, partial 7 is reported as *not isolated*: the pickup
  (node 10 of 70) sits exactly on mode 7's node, so that partial is absent
  from the signal and the estimator correctly refuses to invent it.

## Caught during implementation

1. **A derivation error from D9, found by measurement.** The lossy
   amplification polynomial's constant term had been written as `1−s+rp`
   since D9 and carried into D10. With D → −p the u^{n−1} terms give
   **+rp** on the right, so the constant is **`1−s−rp`**. Using the wrong
   term here predicted a mode-2 decay of 2.000/s, while the FFT measured
   2.560/s and the continuous model says 2.561/s. With the corrected term,
   prediction and measurement agree to 7 digits.
   - **Impact**: the stability conclusions and enforced limits did not
     change (with the corrected term a + c = 2 and |c| ≤ a always, so the
     condition is exactly λ²p + μ²p² ≤ 4). No earlier code or test used the
     term.
   - **Corrections applied**: the D9 and D10 records and the `fdtd1d.h`
     comments now carry explicit correction notes rather than silent
     rewrites.
2. **Long windows on fast decays**: mode 9 decays about 35 dB within a
   16384-sample window, which broadens its line so the D11 main-lobe check
   refuses it. That's correct behavior. The decay test uses 4096-sample
   windows.
3. **A crash in a scratch probe (not the library)**: after its buffer was
   shortened, a leftover block in the throwaway probe read past the end
   and segfaulted. It was noticed because the probe printed nothing, and
   the empty output was investigated rather than trusted.
4. **Thresholds 1000× looser than measured** (1e-4 Hz against ≤ 7e-8 Hz
   measured) were tightened to 1e-6 Hz so they can fail.

## What this means for sound

At 48 kHz, an explicit FDTD string that doesn't sit on an exact Courant-1
grid goes progressively flat: a few cents by the 10th partial on these
grids. The error is known in advance per partial, not discovered by ear.
