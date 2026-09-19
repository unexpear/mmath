# D31–D32 — Loudness, True Peak, True-Peak Limiting, WAV Export (complete)

Status: 368/368 test cases (640,468 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports) · Probes: 10/10

ITU-R BS.1770-5 (11/2023) loudness (LUFS) with EBU R128 windows and EBU Tech 3342
loudness range, plus 4× oversampled true peak. The desktop app shows all of
them for its output.

## New code

| Header | Contents |
|---|---|
| `analysis/bs1770.h` | `kWeighting(rate)`; `TruePeakInterpolator` (one channel of the 4× filter); `TruePeakDetector` (running max, `clearPeaks`); `LoudnessMeter` (momentary, short-term, integrated with both gates, loudness range, true and sample peak) |
| `dsp/dynamics.h` (extended) | `LimiterParams::truePeak`, `Limiter::kTruePeakExtra` |
| `sink/wav_writer.h` (D32) | `WavWriter`: 1–8 channels, 16/24-bit PCM, overload count; mono 16-bit byte-identical to `WavSink` |

## Method

- **K-weighting**: the standard publishes coefficients for 48 kHz only.
  They are derived for any rate (8–384 kHz) from the analog prototype
  constants that reproduce the published values. The test checks them
  against BS.1770-5 Tables 1 and 2 (identical to -4) to 1e-9, so a wrong constant can't
  pass.
- **Blocks**: 100 ms sub-blocks; momentary = 400 ms, short-term = 3 s;
  integrated uses 400 ms blocks at 75% overlap with the −70 LUFS
  absolute gate and the −10 LU relative gate. The L, R and C weights are
  1.0 and the surround weights 1.41; LFE is excluded by the standard.
- **LRA** (Tech 3342): short-term values at 10 Hz, gates of −70 LUFS
  absolute and −20 LU relative, then the 95th minus the 10th percentile.
- **True peak**: 48-tap Kaiser (β = 5) windowed sinc, 4 phases, each
  normalised to unit DC gain. Measured in steady state over all sine
  phases: over-read ≤ 0.02 dB (0.0150 measured); under-read ≤ 0.17 dB up
  to fs/4, ≤ 0.5 dB up to 0.45 fs, and 0.68 dB at 0.49 fs (the known 4×
  limit). Kaiser β = 5 was chosen by measurement over Blackman-Harris
  (0.20 dB at fs/4, 0.37 dB at 0.4 fs) and β = 4 (0.05 dB over-read). A
  hard onset from silence overshoots on reconstruction, up to about 1 dB,
  and is reported as such, since a converter would produce it too.
- **Against the standard's own example filter.** BS.1770-5 Annex 2 prints
  a 48-tap, 4-phase example interpolator. Measured on the same harness
  (7 frequencies × 48 phases, steady state), it under-reads less (worst
  −0.108 dB) but **over-reads up to +0.204 dB**, partly because two of
  its phases have a DC gain of 0.973, not 1. Ours under-reads up to
  −0.163 dB and over-reads at most +0.015 dB. For a limiter, erring
  toward under-read keeps it from reacting to overs that aren't there,
  and the true-peak mode's measured residual already accounts for the
  under-read. This is a recorded choice, not a claim that ours is better
  overall.
- **Revision.** Checked against the BS.1770-5 (11/2023) text: the
  K-weighting coefficients, −0.691 constant, gates, block timing and
  channel weights are all the same as in revision 4.

## Evidence (`[d31lufs]`, 8 cases)

- K-weighting equals the published 48 kHz coefficients to 1e-9.
- The standard's anchor: 0 dBFS 1 kHz in one channel reads −3.01 LKFS
  within 0.01, at 48 and at 44.1 kHz.
- Stereo −23 dBFS 1 kHz reads −23.0 within 0.05 for momentary,
  short-term and integrated; LRA < 0.05 LU; 197 blocks from 20 s.
- Gates: a −36/−23/−36 programme integrates to −23.0 (relative gate);
  a programme with two levels 5 LU apart is not gated and matches the
  ungated mean; everything at −75 gives no reading; leading silence
  doesn't dilute the result.
- LRA of 20 s at −20 plus 20 s at −30 dBFS: 10.0 within 0.2 LU.
- True peak of a fs/4 sine sampled 45° off its crests: the sample peak is
  3.01 dB low, the true peak within 0.17 dB; the stated bounds hold over
  7 frequencies × 32 phases; an onset overshoot is reported.

## D32 — WAV export

`WavWriter` uses the certified sink's policies: symmetric scaling,
round-half-away-from-zero, overloads counted, and `close()` fails on a
short write. The certified mono `WavSink` is left untouched. Tests
(`[d32wav]`, 3 cases): mono 16-bit output is byte-identical to `WavSink`;
stereo 16- and 24-bit round-trip exactly through `parseWav`; overloads,
including NaN, are counted; bad configurations are refused.

The app's **Export processed WAV** renders the whole track through a
private `Engine`, running the same `chain()` as playback with its
crossfades settled, and compensates the limiter latency. The result is
exactly what you hear, sample-aligned with the source. Verified with
every stage off: exporting `eq.wav` reproduces it at the same length,
with a worst difference of 6.6e-8. That is half a 24-bit step plus the
float rounding of the app's track buffer, which rules out any latency
offset.

## In the app

The engine now hands the UI whole **stereo** frames. It never splits a
frame, because a split would swap left and right for the rest of the
session. The meter measures the actual stereo output, or one channel for
the microphone. The readout shows M, S, I, LRA and TP, with TP in red
when it is above the limiter's ceiling. That's how the limiter's stated
limit shows up in practice: the limiter bounds sample peaks, while TP can
read higher.

## True-peak limiting (added after the meter showed the problem)

With heavy compression, an export whose sample peaks sat exactly at the
−1 dBFS limiter ceiling measured **+0.3 dBTP**: inter-sample peaks above
full scale. `LimiterParams::truePeak` fixes that:

- Detection runs on `TruePeakInterpolator`, the same 4× filter as the
  meter. It is factored out of `TruePeakDetector`, whose behaviour and
  tests are unchanged.
- Its points describe the signal about 6 samples back, so the look-ahead
  and the sliding-minimum window grow by `kTruePeakExtra` = 8 samples.
  Each gain applied to x[m] is then bounded by the requirements seen at
  steps m … m+8, which include the intervals either side of m (seen at
  m+5 … m+7).
- Sample peaks stay **guaranteed** (the requirement includes |x[m]|).
  True peaks are bounded **approximately**, because the gain change itself
  reshapes the waveform between samples. So the bound is measured against
  an independent 16× reference (128-tap Kaiser-8 sinc), not against the
  limiter's own filter.

| Signal, ceiling −1 dB | Plain mode (16× reference) | True-peak mode |
|---|---|---|
| fs/4 sine at 45°, +6 dB | +2.01 dBTP (3.0 dB over) | −0.84 dBTP |
| 1.2 kHz square, +6 dB | +1.16 dBTP | −0.87 dBTP |
| clipped sine, +12 dB | −0.64 dBTP | −1.05 dBTP |

The 0.13–0.16 dB residual matches the 4× detector's under-read near fs/4.
Tests pin: plain mode more than 1 dB over, true-peak mode less than 0.2 dB
over, sample peaks within the ceiling in both modes, latency exactly
L + 8, and `setParams` can't change the mode. In the app, the same
heavily compressed export reads +0.3 dBTP in plain mode and **−1.0 dBTP**
in true-peak mode, at an identical −13.1 LUFS.

## Limits

- The meter keeps block history for gating, so `push` allocates
  occasionally. It runs on the UI thread, never in the audio callback.
- Rates that are not a multiple of 10 Hz get 100 ms sub-blocks rounded to
  whole samples.
- True-peak limiting is approximate (≤ 0.2 dB residual, measured on the
  signals above), not a guarantee.
