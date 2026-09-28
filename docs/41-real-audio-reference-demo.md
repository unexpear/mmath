# Real recordings: native reference demo

The desktop app now has a **Reference demo** view. It plays original recordings,
reruns MMathEn chord or YIN pitch recognition on a worker thread, and compares
the fresh output with published reference labels. The separate speech recognizer's
saved output and word errors are also shown. No core recognition parameters were
tuned or changed for this experiment.

## Findings

Measured on 2026-09-19, using 42 recordings totaling 1,096.18 seconds:

| Measurement | Result | Denominator / coverage |
|---|---:|---|
| Smoothed chords vs performed reference | **53.11%** | 601.254 scored seconds / 912.916 annotated seconds (65.86%) |
| Framewise chords, same reference | 42.49% | Same supported time |
| Smoothed chords vs instructed lead sheet | 43.25% | Secondary reference; not the main performed-chord score |
| YIN pitch within 50 cents, including voicing misses | **77.27%** | 4,855 / 6,283 eligible frames; 11,424 total solo frames |
| Whisper base.en word error rate | **5.65%** | 7 edits / 124 reference words; six clips |

306.739 seconds of performed chords are outside the major/minor scoring vocabulary.
Another 4.923 annotated seconds have no estimate coverage. These receive no credit.
`02_Jazz3-137-Eb_comp` has **no supported chord reference time**, so its accuracy is
unknown, not zero or perfect. Pitch eligibility covers 55.00% of solo frames.

Chord performance varies substantially by style in this fixed sample:

| Style | Agreement | Supported seconds |
|---|---:|---:|
| Bossa nova | 54.69% | 107.930 |
| Funk | 9.33% | 103.079 |
| Jazz | 36.33% | 97.102 |
| Rock | 83.03% | 124.917 |
| Singer-songwriter | 66.38% | 168.226 |

These are descriptive sample results, not population estimates. The next useful
model investigation is the low agreement on funk and jazz, especially harmonic
overtones, omitted chord tones, and extended voicings. These are hypotheses to
inspect in the recorded mismatches, not established root causes. Keep this
baseline frozen and use different recordings for parameter development.

## Data and reference quality

- **[GuitarSet 1.1.0](https://zenodo.org/records/3371780)**, Xi, Bittner, Pauwels,
  Ye, Bello (2018), CC BY 4.0. Original mono microphone WAV, 44.1 kHz.
  Selection was fixed before scoring: six players x five styles, progression
  `1 + player modulo 3`, first lexicographic accompaniment recording in each
  group; first Rock1 solo per player for the pitch demo. No score-based selection.
- **[GuitarSet annotation documentation](https://guitarset.weebly.com/)**:
  performed chords are inferred from string-note annotations with manual
  verification, using lead-sheet roots and boundaries. These are reference
  annotations, not perfect physical measurements. The provided pitch contours
  are also annotation estimates. The six pitch solos share one Rock progression.
- The three files in the published [timing issue](https://github.com/marl/GuitarSet/issues/5)
  and [duplicate-note issue](https://github.com/marl/GuitarSet/issues/4) are excluded
  by the selection rule. No annotations were edited to improve results.
- **[LibriSpeech test-clean](https://www.openslr.org/12)**, Panayotov, Chen, Povey,
  Khudanpur (2015), LibriVox readers, CC BY 4.0. First utterance of each of the six
  numerically lowest speaker IDs. FLAC to PCM16 WAV conversion preserves every
  integer sample (checked by decoding both). Speech is read English, not singing
  or vocals mixed with music. Reference transcripts have no verified word
  timestamps or voice-pitch labels; those properties remain unscored.

Both corpora are attributed in the app and the frozen manifest. Archives are
checked against their published MD5 sums; selected audio/reference files have
SHA-256 hashes. Model revision, model-file hashes, package versions, probe binary
hash, and source hashes are in the generated report. Reusing the downloaded audio
requires retaining its CC BY 4.0 attribution.

## Scoring and independence

- `probe.cpp` and the native worker receive audio only. Reference parsing and
  scoring happen separately, after predictions. No label, lead-sheet root,
  transcript, initial prompt, or hotword list is supplied to a recognizer.
- Chords use the existing defaults: Hann 8192, hop 2048, 80-5000 Hz,
  A4=440 Hz, compression 100, gate -60 dBFS, sharpness 10, N penalty 0.2,
  stay probability 0.9, uniform changes. Full-recording Viterbi is offline.
- Exact time-overlap weighting uses
  **[mir_eval 0.8.2 major/minor rules](https://mir-eval.readthedocs.io/latest/api/chord.html)**.
  These compare the root and triad through the fifth, allowing some extended
  chords to reduce to a major/minor triad and excluding others. Original labels
  remain visible. Each estimate covers one hop centered at its analysis-window
  center. No beginning/end labels or annotation gaps are filled with N.
- The native exporter writes each reference chord's independently computed
  supported state to TSV. The native app recomputes predictions and exact overlap
  scores in C++, independently of Python's interval traversal.
- Pitch uses existing YIN defaults (50-2000 Hz, threshold 0.1), window 2048,
  hop 512. Only a window fully inside one note with no other overlapping note
  is eligible. Its reference frequency is the nearest voiced contour sample
  on that note's string, no farther than half the 256/44100-second reference
  spacing. No interpolation through gaps. Error is `1200 log2(predicted/reference)`;
  a hit needs native YIN voicing and absolute error <=50 cents. Octave errors and
  unvoiced decisions are misses. Notes can ring beyond annotated boundaries.
- Words use **[faster-whisper 1.2.1](https://github.com/SYSTRAN/faster-whisper/tree/v1.2.1)**,
  [base.en](https://huggingface.co/Systran/faster-whisper-base.en) revision
  `3d3d5dee26484f91867d81cb899cfcf72b96be6c`, CPU int8, four threads, beam 5,
  English, temperature 0, no VAD, no previous-text conditioning. Word timestamps
  are estimated. WER is minimum word edit distance divided by reference words;
  case/punctuation are ignored, internal apostrophes retained. No number or
  compound-word normalization: for example `FIFTEENTH` versus `15`, and
  `LOVE MAKING` versus `lovemaking`, count as word errors under this policy.

## Desktop implementation

- `app/reference_demo.h`: strict versioned TSV reader, asynchronous audio-only
  recognition, reference comparison, and explicit unknown score when no reference
  is supported. Malformed values, bad intervals, mismatched metadata, duration/grid
  differences, and paths leaving the bundle are rejected.
- `app/spectrum_app.cpp`: Reference demo button and full native view with original
  playback, seek slider, mismatch navigation, chord lanes, note/pitch plot,
  transcripts, word errors, detailed notes, and source/coverage explanation.
  The view bypasses playback effects while retaining the user's control settings;
  leaving it restores those settings. Fresh recognition uses the original WAV,
  not the transformed track or output tuner. Speech inference remains a separate
  offline tool; the app explicitly labels that saved result.
- `tools/real_audio/`: preparation, blind measurement, scoring, native bundle
  export, and independent native replay check. These are optional development
  tools. The header-only library retains its standard-library-only dependency
  contract; normal builds do not fetch datasets or speech models.

## Reproduce (PowerShell, from the repository root)

Verified environment: Python 3.13, MSVC 19.43 Release, Windows x64. The lock file
records the exact Python environment used. Downloads total about 1.04 GB for
the three data archives, plus the speech model and optional Python packages.

```powershell
python -m venv build_real_audio/venv
build_real_audio/venv/Scripts/python -m pip install -r tools/real_audio/requirements-lock.txt
build_real_audio/venv/Scripts/python tools/real_audio/prepare.py --download
cmake -S . -B build_real_audio/cpp -DBUILD_TESTING=OFF -DMMATHEN_BUILD_EXAMPLES=OFF -DMMATHEN_BUILD_VALIDATION=ON
cmake --build build_real_audio/cpp --config Release --parallel 4
build_real_audio/venv/Scripts/python tools/real_audio/evaluate.py
build_real_audio/venv/Scripts/python -m unittest discover -s tools/real_audio -p test_evaluate.py -v
build_real_audio/cpp/Release/reference_data_check.exe build_real_audio/reference-suite.tsv
cmake -S . -B build_app_msvc -DBUILD_TESTING=OFF -DMMATHEN_BUILD_EXAMPLES=OFF -DMMATHEN_BUILD_APP=ON
cmake --build build_app_msvc --config Release --target mmathen_app --parallel 4
build_app_msvc/Release/mmathen_app.exe --reference-demo build_real_audio/reference-suite.tsv
```

`evaluate.py --refresh` reruns predictions. Without it, only matching audio,
probe binary, model-revision/options and dependency-version cache keys are reused.
The desktop app always reruns chord/pitch recognition when a recording is selected.
No network is needed for prepared desktop playback. `--reference-track N` selects
the zero-based frozen-catalog entry, and existing `--capture`, `--seek`, `--seconds`,
`--mute` options support native capture verification.

Prepared outputs live under ignored `build_real_audio/`: `reference-suite.tsv`,
`reference_demo/`, `audio/`, `reference/`, `manifest.json`, `raw/`, `report.json`,
`summary.json`, `native-scores.tsv`, native screenshots, and the runnable app copy.
The app finds a suite beside its executable or under `build_real_audio/`; a file
picker handles other locations. Audio is not embedded in or committed with source.

## Verification

- 11 scorer tests pass: overlap weighting, boundary splitting, unknown/unsupported
  labels, uncovered edges and gaps, chord reduction rules, pitch eligibility,
  voicing and octave failures, word insertions/deletions/substitutions, and fixed
  balanced selection.
- Native replay reads and recomputes all 42 recordings. Scores and denominators
  agree with Python/mir_eval; maximum accuracy difference is
  **2.220446049250313e-16**. Six speech pitch scores correctly remain unknown.
- MSVC desktop build passes with warnings treated as errors. UCRT64 GCC also
  built the native integration successfully before the final plot-label adjustment.
- The existing MSVC core suite passes: **430 test cases, 660,300 assertions**.
- Native capture records include fresh score, eligible denominator, readiness,
  errors, audio position, clipping, and analysis activity. Native chord and pitch
  views were inspected, playback position advanced, and switching to guitar pitch
  produced the fresh expected score. The user stopped Computer Use with Escape
  during the mismatch-navigation check. Interactive seeking, the speech view,
  and return to the analyzer were therefore not fully verified in this session.

This establishes a reproducible real-recording baseline. It does not establish
general chord accuracy, polyphonic note transcription, speech recognition in the
MMathEn core, speech pitch accuracy, or live end-to-end recognition latency.
