# D1 — Inharmonicity + Consonance (complete)

Status: implemented · 114/114 on MSVC Release+Debug, GCC-13 Release,
Clang-18 Release, GCC ASan+UBSan (zero reports) · Probes: 10/10

First depth milestone (research-grade) and first `analysis/` module. Links
the timbre work to the tuning work: stiff spectra move consonance minima,
which is the physical mechanism behind stretch tuning (Giordano 2015).

## New code

| Header | Contents |
|---|---|
| `signal/inharmonic.h` | `StiffSpectrum{f0, B, count}` + factory; `partialFrequency` (`f_n = n·f₀·√(1+Bn²)`); `audibilityThresholdB` (Smith PASP Eq. 10.31, documented approximation); `stiffAdditive` (reuses oscillator + mix); `estimateB` two-partial closed form `B = (g_m−g_n)/(m²g_n−n²g_m)` (finite estimates returned as-is; undefined cases are `DomainError`) |
| `analysis/consonance.h` | `Partial` + factory; `criticalBandwidth` (Hz); `pairDissonance`; `dyadDissonance` (amplitude-weighted pairwise sum, O(n·m) documented); `Dissonance` score type in [0,1] |

## Model decisions (user-locked, with sources)

- Curve: Essentia's PL quintic fit read verbatim from source
  (`-6.58977878…+1.00026609`, Horner evaluation here), clamped [0,1],
  `C = 1` outside (0, 1.18]; the zero clamp is the spike fix per EPJP 2024.
  No Essentia code incorporated (AGPL) — published fit reimplemented with
  attribution; verified numerically against the excerpted behavior.
- CBW: Essentia's Bark formulation verbatim (`hz2bark` Schroeder fit +
  `52548/(z²−52.56z+690.39)`; denominator zeros at z ≈ 25.8/26.8, outside
  range — checked, no singularity).
- Documented deviations from Essentia: no dBA-weighting (synthesized linear
  amplitudes, not measured spectra), no loudness normalization, no 50 Hz /
  10 kHz gates (exact partial lists, not peak-picked audio).

## Evidence

- Partial anchors (B = 1e-3: f₁ = 100.0499875062461, f₂ = 200.3996007980056);
  estimator round-trips B = 2e-4 to 2e-14; JND(440) ≈ 1.0733e-4;
  CBW(440) ≈ 109.6726 Hz; unison D = 0 exact; far-field D = 0 exact.
- Harmonic fifth the deep minimum (0.043 vs 0.257 fourth, 0.308 tritone);
  stiff strings: same 3/2 rougher (0.0520 > 0.0431), local minimum shifted
  sharp to 1.501 on the fine grid — the Railsback mechanism, quantified.
- Audible: `stretch_demo` renders harmonic vs stiff A220 plus the printed
  dissonance tables (`plain.wav`/`stiff.wav`, both clean).
