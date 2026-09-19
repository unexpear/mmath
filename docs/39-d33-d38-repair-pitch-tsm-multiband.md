# D33–D38 — Normalization, Noise Reduction, pYIN, Transient-Aware TSM, Multiband, ACGE (complete)

Status: 398/398 test cases (645,567 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports) · App: builds on MSVC /W4 /WX, UCRT64 GCC and Linux (WSL, GCC 13)

Six items from the survey of DAWs, editors (Audacity, Audition, Descript,
Resolve/Fairlight) and the research literature. Each fills a gap that
the survey found: the tools people use have it, and the papers describe
it well enough to implement and test. The seventh item, ML add-ons
(stems, voice isolation, transcription), is kept out of the core: it
needs model weights and a runtime, so it can only be an optional app
add-on. It isn't built yet (see the end of this record).

## New code

| Header | Contents |
|---|---|
| `dsp/loudness_normalize.h` (D33) | `NormalizeTarget`, `NormalizeReport`, `normalizeLoudness`: measure, gain, true-peak limit, re-measure, iterate |
| `dsp/denoise.h` (D34) | `special::i0e/i1e/e1`; `NoiseProfile`, `learnNoiseProfile`; `SpectralDenoiser` (spectral subtraction, MMSE-STSA, MMSE-LSA); `denoise`; `spectralGainRegion` (time–frequency box repair) |
| `analysis/pyin.h` (D35) | `PyinOptions`, `PitchCandidate`, `pyinCandidates`, `pyinDecode` (Viterbi), `pyinTrack` |
| `dsp/tsm.h` (D36) | `pghiStretch` (phase-gradient heap integration), `hpss` (median-filter harmonic/percussive split), `olaStretch`, `TsmMethod`, `timeStretch`, `pitchShift` |
| `dsp/multiband.h` (D37) | `MultibandCompressor` (3 bands, LR4, all-pass-compensated); `DynamicEqBand`, `DynamicEq` (de-esser) |
| `dsp/linear_phase.h` (D37) | `designLinearPhaseFir`, `linearPhaseFromSections`, `FftConvolver`, `midSideEncode/Decode` |
| `dsp/acge.h` (D38) | `makeOrfanidisPeak`, `AcgeDesign`, `designAcge` (Välimäki–Liski accurate cascade graphic EQ) |

Changed: `dsp/time_pitch.h`. TD-PSOLA now takes its pitch marks from
`pyinTrack` instead of frame-wise YIN, so an octave error no longer
places marks a whole period off.

## D33 — Loudness normalization

Streaming services and broadcast deliver to a loudness target (−14 LUFS
for streaming, −23 LUFS for EBU R128, −24 LKFS for ATSC A/85) under a
true-peak ceiling. `normalizeLoudness` measures integrated loudness with
D31's meter and applies the gain. If the true peak then exceeds the
ceiling, it runs D31's true-peak limiter (latency compensated). Limiting
lowers the loudness, so the function re-measures and repeats, up to 5
passes, until it is within 0.1 LU.

The report says what happened: input and output LUFS, gain, output true
and sample peak, passes, whether the limiter ran, and `reachedTarget`.
A target that is too loud for the ceiling comes back as not reached. It
is never faked.

Evidence (`[d33norm]`, 4 cases):
- A quiet, transient-rich programme reaches −14.0 LUFS within 0.1 LU at
  −1 dBTP. The limiter runs, sample peaks stay inside the ceiling, and
  true peak stays within D31's measured residual.
- When no limiting is needed, the output is a bit-exact pure gain, up or
  down.
- An unreachable target is reported as not reached.
- Mono input works. Silence and bad arguments are refused. Length is
  preserved.

## D34 — Noise reduction and spectral repair

This is the frequency-domain family behind Audacity's Noise Reduction and
Audition's Noise Reduction/Spectral Frequency Display. It runs on D26's
overlap-add. A noise profile (mean power per bin) is learned from a
noise-only selection, and the gain per bin comes from one of three
methods:

- **Spectral subtraction** (Boll 1979), with Berouti over-subtraction
  α = 4.
- **MMSE-STSA** (Ephraim & Malah 1984): the gain uses exponentially
  scaled Bessel functions I0 and I1.
- **MMSE-LSA** (Ephraim & Malah 1985): the gain uses the exponential
  integral E1.

The MMSE methods use the decision-directed a priori SNR (a = 0.98). The
gain is floored at −reduction dB. The special functions are implemented
here (Abramowitz & Stegun polynomials for I0/I1; a series or Lentz
continued fraction for E1) and checked against power series and tables.

`spectralGainRegion` applies a gain to a time–frequency box, with
crossfaded edges. It is the "select a region on the spectrogram and
attenuate it" repair.

| Tones in white noise | SNR gain (measured) |
|---|---|
| Spectral subtraction (α = 4) | +17.0 dB |
| MMSE-STSA | +14.2 dB |
| MMSE-LSA | +17.6 dB |

Tone levels are kept within 1 dB. The first subtraction attempt, without
over-subtraction, gave only +4.9 dB. Berouti's α fixed that.

Evidence (`[d34nr]`, 4 cases):
- The special functions agree with their series and tables.
- A 0 dB reduction is an exact delay.
- Every method raises SNR by more than 12 dB and keeps the tones.
- Noise-only input is pulled down, but by less than the floor: LSA
  reaches −19.3 dB against a −24 dB floor. This is how the
  decision-directed estimator behaves, and the test states it that way.
  The floor is a bound, not a target.
- Box repair attenuates a tone inside the box by the stated gain. A tone
  outside the box, and the time outside it, are untouched.

## D35 — pYIN

This is Mauch & Dixon (ICASSP 2014). YIN's threshold is replaced by a
Beta(2, 18) prior, which gives several pitch candidates per frame, each
with a probability. An HMM decodes them with Viterbi. The states are 10
bins per semitone, each voiced or unvoiced. Transitions allow a maximum
jump of 25 bins, and the voicing switch probability is 0.01. An energy
gate below −60 dBFS keeps silence from being called voiced at fMax.

| Noisy tone whose fundamental keeps fading (6 dB SNR), 278 frames | Gross errors (> 20 %) |
|---|---|
| Frame-wise YIN | 142 (it jumps to the octave) |
| pYIN | **0** |

On clean vibrato, pYIN equals YIN, and both follow the window-averaged
frequency. The remaining offset is at most 10.9 cents, at the fastest
pitch change, which is the window's averaging and not a tracking error.

Evidence (`[d35pyin]`, 4 cases): a clean tone is voiced throughout at
YIN's precision; voicing is correct across tone, silence, noise and tone;
the gross-error case above; vibrato; refusals.

## D36 — Transient-aware time-scale modification

D28's phase vocoder smears transients. Two remedies from the literature
are implemented and measured:

- **PGHI** (Průša, Balazs & Søndergaard 2017; Průša & Holighaus, "Phase
  vocoder done right", EUSIPCO 2017). Phase is integrated along the
  time–frequency gradient, starting from the largest-magnitude bins, using
  a max-heap. The frames are zero-phase (rotated by N/2). That detail
  mattered: without it the phase gradient was wrong, and clicks came out
  worse than the classic vocoder (concentration 0.096).
- **Hybrid** (Driedger, Müller & Ewert 2014). HPSS median filtering (17
  frames by 17 bins) splits the signal into harmonic and percussive parts.
  The harmonic part is stretched with PGHI and the percussive part with
  short-frame OLA (256 samples), then the two are summed.

Metric: the fraction of energy within ±1 ms or ±5 ms of each stretched
click, for a click train stretched by 1.5×. Higher is more compact.

| Method | ±1 ms | ±5 ms |
|---|---|---|
| Classic phase vocoder (D28) | 0.29 | 0.66 |
| PGHI | **0.53** | **0.77** |
| Hybrid | — | 0.77 |

On this material the hybrid matches PGHI. It is kept because it separates
by content, which matters on mixed material that this test doesn't cover.
PGHI's tone error is ≤ 0.018 Hz. HPSS sums back to its input to roundoff,
putting 99.7 % of the tone's energy in the harmonic part and 99.0 % of
the clicks' energy in the percussive part.

Evidence (`[d36tsm]`, 3 cases): HPSS exactness and separation; PGHI's
tone accuracy and transient compactness, with the hybrid; a PGHI pitch
shift moves a tone by exactly the ratio at the same length; refusals.

## D37 — Multiband compressor, dynamic EQ, linear-phase EQ, mid/side

- **Multiband compressor**: 3 bands split by Linkwitz–Riley 4th order
  (defaults 200 Hz and 2.5 kHz), each band with a D24 compressor. The low
  band is passed through a parallel LR4 LP+HP at the high split, so all
  three bands share the same phase. Then, with neutral compressors, the
  whole thing is exactly the all-pass chain AP_high(AP_low(x)).
- **Dynamic EQ**: an RBJ peaking band whose cut follows a compressor gain
  computer driven by the band-passed signal. Its range is limited, and
  it is recomputed every 32 samples. The de-esser preset sits at 6.5 kHz.
  `setBand` retunes the band while keeping the filter state.
- **Linear-phase EQ**: FIR by frequency sampling of any dB response,
  windowed by Blackman, with exact symmetry. `linearPhaseFromSections`
  samples a biquad cascade. `FftConvolver` is a uniform-partition
  overlap-save; its latency equals the block size.
- **Mid/side**: M = (L+R)/2, S = (L−R)/2, and the exact inverse.

Evidence (`[d37mb]`, 5 cases):
- The neutral multiband equals the all-pass reference within 1e-12, and
  a steady sine keeps its amplitude within 1e-9.
- A loud 80 Hz tone is compressed by more than 5 dB, while a quiet 6 kHz
  tone stays within 0.05 dB.
- The dynamic EQ is transparent below threshold to roundoff (< 1e-15).
  It is not bit-exact, because a 0 dB section's direct-form sums still
  round. Above threshold it cuts −15 dB, exactly as the gain computer
  predicts, and leaves 300 Hz within 0.1 dB.
- A 4097-tap FIR matches the cascade within 0.1 dB, and the convolver
  equals direct convolution within 1e-12.
- Mid/side round-trips within 1e-15. Side processing leaves mono content
  exactly unchanged.

## D38 — Accurate cascade graphic EQ, and what it found in D25

This is Välimäki & Liski (IEEE SPL 2017), built as an independent
cross-check of D25's graphic EQ solver:

- **Band filters**: Orfanidis peak filters. Each one's gain at its lower
  neighbour is c·g, with c = 0.3.
- **Bandwidth**: solved by bisection with the paper's lower-neighbour
  rule, so the design works at any rate. For low bands this reproduces
  B = 1.5 ωc.
- **Gains**: a least-squares solve over 19 points (10 centres and 9
  geometric midpoints), with interaction columns taken at the 17 dB
  prototype, then one iteration.

| Target (10 octave bands) | ACGE worst error, centres / midpoints | D25 |
|---|---|---|
| all +12 dB | < 1 dB / < 1 dB | exact at centres, **5.3 dB** ripple at midpoints |
| ±12 dB zigzag | 0.25 dB / < 1 dB | **not converged**: 3.0 dB residual, which D25 reports |
| smooth curve | < 1 dB / < 1 dB | exact at centres, 3.0 dB at midpoints |

The worst ACGE error across all four targets is 0.76 dB, which matches
the paper's claim of under 1 dB.

**Finding.** D25 does what it promises: exact gains at the band centres,
with an honest residual. But it leaves the response between the centres
unconstrained, and that is where the audible error is. D25 stays in the
library, and its tests are unchanged. The app's graphic EQ now uses ACGE
by default, with D25 available in a solver menu.

Evidence (`[d38acge]`, 3 cases): each Orfanidis section hits g at fc
(1e-9) and c·g at the neighbour (1e-6); zero gain is the identity; the
table above; refusals.

## In the app

| Where | Added |
|---|---|
| EQ tab | graphic solver menu (ACGE default, D25, plain); EQ target (stereo, mid, side) switched with fade-out and fade-in |
| Dynamics tab | de-esser (dynamic EQ) with a live cut readout; 3-band multiband with per-band threshold, ratio, on/off and GR |
| Pitch/Time tab | four methods: PSOLA (pYIN marks), classic vocoder, PGHI, hybrid |
| Repair tab (new) | learn the noise profile from a selection; reduce noise (three methods, reduction amount); shift+drag a box on the track spectrogram to attenuate it; undo |
| Tuner | streaming pYIN with a 24-frame Viterbi window |
| Export | optional linear-phase EQ (8191 taps, delay compensated, mid/side aware); optional loudness normalization with presets (−14, −16, −23, −24 LUFS) and a ceiling |

The engine chain is now: EQ (stereo, M or S) → de-esser → compressor →
multiband → volume → limiter. Every new stage is crossfaded on and off.

Verified by capture (MSVC Release, the 4 s demo):

| Scenario | Result |
|---|---|
| Export with `--normalize` | −14.0 LUFS, −1.0 dBTP |
| Flat export (every stage off) | worst difference to the source 6.55e-8 (as D32) |
| Minimum-phase vs linear-phase export, same EQ | both −28.0 LUFS; worst third-octave band difference 0.109 dB (bands above −90 dB) |
| PGHI pitch/time | worker render, track replaced (hybrid shares the path; not captured separately) |
| Repair tab; dynamics tab with de-esser and multiband sections | both render |
| Linux (WSL, GCC 13) | app builds |

## Limits

- **MMSE noise floor.** The MMSE methods don't reach the full floor on
  pure noise (−19.3 dB measured against −24 dB). That is how the
  decision-directed estimator behaves.
- **Musical noise.** Spectral subtraction trades it for depth, and no
  perceptual measure is claimed.
- **pYIN tuning.** pYIN's HMM parameters are the paper's. The tuner widens
  the jump limit to 60 bins so it can follow fast note changes.
- **Hybrid TSM.** It only equals PGHI on the click-train test. Its
  benefit on real mixed material isn't measured here.
- **Linear-phase latency.** Linear-phase EQ is offered on export only,
  because 8191 taps is 85 ms of latency at 48 kHz.
- **ACGE limits.** ACGE is octave-band only, which matches the paper. Its
  gains are limited to ±24 dB.
- **ML add-ons (item 7) are not built.** Stem separation (demucs), voice
  isolation and transcription (whisper) would need model weights of tens
  to hundreds of MB and a runtime. They fit only as an optional app build,
  and downloading any weights needs explicit permission first.
