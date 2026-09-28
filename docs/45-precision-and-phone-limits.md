# Arithmetic precision and phone recognition limits

Research and experiment: September 23, 2026. This compares published phone
claims with our measured desktop results; no phone app or handset was benchmarked.

## Findings: claims and hardware

The strongest explicit phone tuner accuracy claim found in the reviewed primary
sources is **±0.01 cent**, from
[Nembrini Chromatic Tuner for iPhone/iPad](https://www.nembriniaudio.com/products/chromatic-tuner).
[Peterson iStroboSoft](https://www.petersontuners.com/support/iStroboSoft) claims
0.1-cent detection/display accuracy and describes calibration to guarantee that
reading. These are vendor specifications, not independently reproduced results
on our guitar recordings, nor a proven maximum achievable by all phones.
Display resolution, repeatability, and absolute accuracy are different quantities.

Our current streaming tuner has **4.70 cents median absolute error among voiced
outputs** on the previously frozen real-guitar comparison. It is within 5 cents
at 49.22% of the 4,447 eligible moments, including unvoiced estimates as misses.
This includes changing instrument pitch. GuitarSet's reference contours are not
certified to ±0.01 cent, and the phone vendors do not publish the same test protocol.
Dividing these numbers to claim a phone is hundreds of times better would be invalid.
See [the existing measured comparison](44-community-recognition-comparison.md).

[Chord ai](https://www.chordai.net/jet-popup/next-level-chord-recognition/) describes
on-device neural models and recognition of augmented, diminished, seventh and
suspended chords in addition to major/minor. That establishes a claimed feature
range, not a comparable accuracy percentage: the reviewed page supplies no shared
GuitarSet score or reproducible evaluation protocol. There is no verified 98%
phone result here for full chord quality and bass.

There is no single phone hardware ceiling expressed in cents or chord accuracy:

| Layer | Verified capability or constraint | Consequence for this engine |
|---|---|---|
| Arithmetic | The [Arm 64-bit ABI](https://github.com/ARM-software/abi-aa/blob/main/aapcs64/aapcs64.rst) defines IEEE double precision. | Modern Arm phones can use the same double arithmetic as our core. Wider vector registers do not imply equally wide scalar precision or a recognition guarantee. |
| Audio format | [Android AudioFormat](https://developer.android.com/reference/android/media/AudioFormat) documents 32-bit float PCM and API support for sample rates up to 192 kHz, with conversion as needed. | An accepted format is not proof of that microphone's native rate, effective bit depth, or clock accuracy. Android recommends matching the endpoint and ordinarily staying at or below 48 kHz. |
| Actual device | [Android device queries](https://developer.android.com/reference/android/media/AudioDeviceInfo#getSampleRates()) expose supported rates; [Apple's session guidance](https://developer.apple.com/library/archive/qa/qa1631/_index.html) says preferred rates are hints and to query the active rate. | There is no universal built-in microphone maximum established by these APIs. Device and audio route must be measured. The archived Apple document is used for API behavior, not current handset specifications. |
| Timing and noise | [Android's audio guidance](https://developer.android.com/ndk/guides/audio/audio-latency) warns that actual sampling clocks can differ from nominal rates and from each other; input processing adds delay. | Absolute tuning needs clock calibration and a suitable signal. Processing speed, sustained power use, and end-to-end delay require a specific-device benchmark. |

At A440, a 0.01-cent frequency difference is about **0.00254 Hz**, or **5.78 parts
per million**. This is calculated as `440 * (2^(0.01/1200) - 1)`. A clock bias of
that size alone can consume the accuracy allowance, regardless of extra digits in
the calculation. A tone played from an uncalibrated computer is not automatically
an absolute frequency standard. Higher audio sample rate or amplitude bit depth
does not automatically improve chord interpretation.

## Findings: our code and precision experiment

The built-in chord detector uses double arithmetic, an 8,192-sample FFT, and 25
states: 12 major, 12 minor, and no chord. It assigns spectral energy to the nearest
pitch class, combines octaves, and matches equal-weight root/third/fifth templates.
It has no separate bass feature, harmonic note separation, or automatic tuning
estimate. Thus it cannot name many annotated qualities even with exact arithmetic.
The optional Chordino backend is separate and was not modified in this experiment.

At 44.1 kHz, this FFT has 5.38 Hz bin spacing and spans 186 ms. Bin spacing is not
a hard frequency-estimation accuracy limit. The tuner already estimates fractional
periods using interpolation; its pYIN path retains continuous refined frequency
after selecting a pitch bin. Its readout is not limited to 10-cent increments.

An isolated validation executable separates two hypotheses:

1. **More arithmetic precision:** recompute the window, FFT, powers, pitch-class
   assignment, compression and normalization using GCC `long double` with 64
   mantissa bits instead of double's 53. Convert the features to double before the
   unchanged template scoring and HMM. A separate double implementation is the
   control and must reproduce the production features and decisions.
2. **Different spectral interpretation:** use local peaks with quadratic
   interpolation of log power, with either the original FFT or 2× zero padding.
   The method follows [quadratic peak interpolation](https://www.dsprelated.com/freebooks/sasp/Quadratic_Interpolation_Spectral_Peaks.html).
   Only peaks above -40 dB relative to the maximum in-band power contribute.
   [Interpolation bias](https://www.dsprelated.com/freebooks/sasp/Bias_Parabolic_Peak_Interpolation.html)
   remains possible; zero padding provides denser interpolation, not new samples
   or additional physical resolving power.

Both hypotheses use the desktop guitar profile: 80–2,500 Hz, A440, compression 10,
hop 2,048, -60 dB gate, and the existing templates and HMM. No annotation enters
the recognition executable.

The original 30 accompaniment recordings are development data. Scores use exact
duration-weighted overlap and [mir_eval 0.8.2](https://mir-eval.readthedocs.io/latest/api/chord.html).
The acceptance rule, written before predictions, requires an improvement in full
within-octave quality plus bass without reducing major/minor agreement.

| Development variant | Changed frames / 19,552 | Major/minor | Full quality | Quality + bass |
|---|---:|---:|---:|---:|
| Production baseline | 0 | 61.87% | 32.20% | 22.50% |
| Independent double control | 0 | 61.87% | 32.20% | 22.50% |
| Extended arithmetic | **0** | 61.87% | 32.20% | 22.50% |
| Interpolated peaks | 5,677 | 63.20% | 32.05% | 21.89% |
| Interpolated peaks, 2× padding | 5,676 | 62.87% | 32.06% | 21.98% |

The double control reproduced all features exactly. The largest extended-arithmetic
change to a normalized feature was **5.13 × 10⁻¹⁶**. Extra arithmetic precision
did not change any chord decisions in this experiment. This audits the spectral
front end; it does not claim to audit every decoder operation or the tuner with
extended precision.

Major/minor scoring covers 601.25 / 912.92 annotated seconds (65.86%); the richer
metrics cover 907.99 / 912.92 seconds (99.46%). The same coverage is used for every
variant. These are different recordings from
the previous community comparison, so their percentages cannot be compared as an
engine regression. Full quality here means pitch-class quality, not exact voicing
or every played note's octave. Both peak variants fail the acceptance criterion:
a modest gain on the simplified task hides a loss on the richer task.

Before predictions, 30 additional accompaniment recordings were frozen, one per
player/style, excluding both previous selections and known bad recordings. These
are new recordings by the same six players, not unseen performers. **They remain
unscored** because neither candidate passed development. Keep them reserved for
the next candidate; do not repeatedly tune against them.

## Plan based on the evidence

1. For chords, test a richer vocabulary with separate bass evidence and harmonic
   note separation. Compare each change on development data before combining it.
   Chordino's existing gains motivate this direction, but do not isolate which
   individual component caused the improvement.
2. For the tuner, separate attacks, transitions and stable held notes in the
   evaluation. Then test longer stable-note tracking with harmonic consistency,
   preserving the fast acquisition path and measuring added delay. Numerical
   contract tests alone cannot establish real-instrument accuracy.
3. Establish an independently calibrated reference and capture-clock correction
   before claiming 0.1-cent or 0.01-cent absolute tuning. The existing A4 reference
   control chooses a tuning standard; it does not calibrate the hardware clock.
4. Validate an accepted development candidate once on reserved recordings, and
   report full quality, bass, error distribution, abstentions and time coverage.
   Benchmark actual phones separately if a specific handset comparison is needed.

## Changes and verification

Added only the offline `chroma_math_probe` target, the real-recording evaluation
script, and this report. Neither rejected candidate changes the production engine,
tuner or audio playback. The original `D:\dev\f3` is untouched.

The probe builds with both GCC 16.2.0 and MSVC 19.43 under the project's
warnings-as-errors policy. GCC release uses `-fno-fast-math -ffp-contract=off`.
Every arithmetic-audit recording confirms a 64-bit extended mantissa; the Python
runner refuses a development audit if the compiler provides no extra precision.
It also refuses held-out evaluation when development selects the baseline or the
probe binary changes after selection.
All 30 development runs pass the independent-control check, all 30 reserved
recordings have verified hashes and disjoint identities, and the 17 existing
Python evaluation tests pass. Python syntax and Git whitespace checks pass.
The production desktop executable retains its pre-experiment SHA-256:
`d39108a19099c1cf27ce6edd404e261278e53cfed4d4b18d11a52aee0dfe7186`.

Reproduce after preparing the existing real-audio environment:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake -S . -B build_math_gcc -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe `
  -DCMAKE_MAKE_PROGRAM=C:/Users/micha/AppData/Local/Microsoft/WinGet/Links/ninja.exe `
  -DBUILD_TESTING=OFF -DMMATHEN_BUILD_EXAMPLES=OFF -DMMATHEN_BUILD_VALIDATION=ON
cmake --build build_math_gcc --target chroma_math_probe --parallel 4
build_real_audio/venv/Scripts/python.exe tools/real_audio/evaluate_chroma_math.py
```

Local reports are under `build_real_audio/math-improvement`: per-recording
predictions in `development`, `development-report.json`, `selection.json`
(selected variant: `baseline`), and the untouched `holdout.json` manifest.
Measured probe SHA-256:
`274e568de76712d7e49f17a7e606f3f703571d6bb911acf89dfd229e47f1d500`.
