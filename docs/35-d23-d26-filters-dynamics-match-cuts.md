# D23–D26 — Digital Filters, Dynamics, Match EQ, Smooth Cuts (complete)

Status: 368/368 test cases (640,468 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports) · Probes: 10/10

The processing half of the desktop app, built as tested library code
first. Every claim below is pinned by a test, and the measured values are
quoted from test runs.

## New code

| Header | Contents |
|---|---|
| `dsp/filters.h` (D23) | RBJ `makeBandpass` (0 dB peak), `makeNotch`, `makeAllpass`; bilinear first-order low/high-pass; `butterworthQ`, `designButterworth` (orders 1–8, LP/HP); `designLinkwitzRiley` (LR2/4/8); `transferResponse`, `cascadeResponse`, `cascadeResponseDb`; `BiquadCascade` (fixed capacity, allocation-free `setSections`, state kept) |
| `dsp/dynamics.h` (D24) | `CompressorParams`, `compressorCurveDb`, `autoMakeupDb`, `Compressor` (peak/RMS detector, soft knee, log-domain smooth branching, linked stereo); `LimiterParams`, `Limiter` (look-ahead, guaranteed ceiling) |
| `dsp/match_eq.h` (D25) | `solveGraphicEqGains`; `longTermSpectrum` (Welch); `MatchEqOptions`, `MatchEqDesign`, `designMatchEq` (closed loop; reference as a spectrum or as band levels) |
| `dsp/crossfade.h` (D26) | `FadeCurve` (linear, raised cosine, equal power), `crossfadeBlocks`, `GainRamp`, `SpliceReader<T>` (crossfaded jumps and loop seams) |
| `dsp/overlap_add.h` (D26) | `OverlapAdd`: streaming STFT, √Hann analysis and synthesis, hop N/2 or N/4, spectral callback |

## D23 — digital filters

- Butterworth order N is floor(N/2) RBJ sections with
  Q_k = 1/(2 sin((2k − 1)π/2N)), plus one first-order section when N is
  odd. Every section is prewarped at the same fc, so the cascade is
  **exactly** the bilinear transform of the analog prototype:
  |H|² = 1/(1 + (tan(πf/fs)/tan(πfc/fs))^2N).
- Linkwitz–Riley = Butterworth squared. LR4 and LR8 low- and high-pass
  sum to an all-pass (|LP + HP| = 1); LR2 needs the high-pass inverted.

Evidence (`[d23filt]`, 6 cases): every order 1–8, LP and HP, at three
cutoffs, matches the closed form to 1e-9 relative at 8 frequencies, and
sits at exactly −3.0103 dB at fc; band-pass, notch and all-pass anchors to
1e-12, with all-pass phase −π at f0; LR sums flat to 1e-10; every design is
stable on a grid from 20 Hz to 0.49 fs; `BiquadCascade` output equals the
sections in series bit for bit, keeps state across `setSections`, and
refuses oversize or unstable loads without changing.

## D24 — compressor and limiter

- **Compressor** (Giannoulis, Massberg & Reiss, JAES 2012): static curve
  with soft knee W (continuous, slope 1 below and 1/R above). Gain
  reduction is smoothed in dB by the smooth branching detector, with
  a = exp(−1/(τ·fs)). Linked stereo takes the larger channel's level.
  `autoMakeupDb` brings a 0 dBFS input back to 0 dBFS (a stated
  convention, not loudness matching).
- **Limiter**: required gain r = min(1, c/peak); h = sliding minimum of r
  over L+1 samples; g = box mean of h over L+1 samples; the audio is
  delayed by L. Every h in the mean is ≤ r at the delayed sample, so
  **|out| ≤ ceiling on every sample, by construction**. Release only ever
  lowers the gain, and a final clamp removes running-sum roundoff. Below
  the ceiling the limiter is an exact L-sample delay. The ceiling applies
  to sample peaks only; inter-sample peaks can exceed it (up to +3.0 dB
  measured). D31 later added a true-peak mode that holds them within
  0.2 dB (see `38-d31-loudness-true-peak.md`).

Evidence (`[d24dyn]`, 7 cases): the curve's knee edges and slopes;
attack and release equal the exact one-pole steps to 1e-9 dB, reaching
1 − 1/e at τ; steady state lands on curve + makeup (DC and RMS); linked
stereo gains are equal to 1e-12; ceiling held on +12 dBFS sines, square
waves and +20 dB impulses at three ceilings and two look-aheads (within
8 ulp); exact delay below the ceiling (L = 96 at 2 ms); linear attack
ramps; release recovers.

## D25 — gain solver and match EQ

- **Solver**: g ← clamp(g + (target − exact centre response)). It removes
  the D21 interaction: for smooth ±8 dB targets on octave and third-octave
  bands the residual is < 0.001 dB, where plain gains were off by more
  than 1 dB.
- **Match EQ**: Welch long-term spectra, band levels, correction =
  reference − source. With `normalize` on, the mean correction is removed
  (tone, not level). Then a **closed loop**: the source's band levels
  through the EQ are predicted exactly from its spectrum (bin power ×
  |H|²), and the targets are corrected until the prediction matches. This
  closes the gap that centre-point matching leaves on slopes and at the
  edge bands.
- Bands are left alone if either signal is below an absolute floor, more
  than `relativeFloorDb` (50 dB) below its own loudest band, or holds no
  bin. The relative floor was added after the app showed tonal material
  getting large boosts in bands that hold only window leakage.

Evidence (`[d25match]`, 5 cases): noise matched to the same noise through
a known EQ goes from 5.98 dB off to **0.064 dB** (worst third-octave band,
50 Hz–16 kHz); a 48 kHz source matched to a shaped **44.1 kHz** reference
gives shapes within 0.157 dB, the worst band being 63 Hz, which holds 3
bins. Below that, independent noise signals differ by chance by up to
1.4 dB in 1–2-bin bands, which no EQ can fix. Identity gives all-zero
gains; normalize and amount act exactly; the relative floor ignores
leakage bands of a tonal signal.

## D26 — smooth cuts

- **Curves**: linear and raised cosine keep gi + go = 1 (for correlated
  material); equal power keeps gi² + go² = 1 (for uncorrelated material).
- **SpliceReader**: a jump keeps the old position playing as an outgoing
  voice while the new one fades in. A loop seam starts F frames before the
  loop end, so the end and the start overlap. A jump during a fade drops
  the quieter voice, a bounded step that is stated. After a fade the
  output is the source exactly.
- **OverlapAdd**: √Hann × √Hann = Hann, and periodic Hann sums to N/(2H),
  so the output is scaled by 2H/N. With identity processing it reconstructs
  its input exactly from the first sample, with latency exactly N.

Evidence (`[d26fade]`, 6 cases): the curve identities to 1e-15; a jump
between opposite phases of a sine never steps more than the sine's own
slope plus the fade's (a hard cut steps by about 2, or 1 with a one-frame
fade); a mismatched loop seams smoothly and plays the source exactly
outside the seams; stereo interleave and end of file; the OLA is an exact
N-sample delay to 1e-12 at both hops and for any block sizes, and applies
spectral gains exactly.
