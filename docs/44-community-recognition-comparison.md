# Comparable community methods: real-recording evidence

Experiments prepared September 19, 2026; desktop integration verified September 23.
There is no evidence here for 98% recognition at this application's complexity.
Chordino improves recognition on the frozen recordings, including richer chord
qualities, but substantial errors remain. The tested CREPE configurations do not
improve the current streaming tuner. No measurements use synthetic audio as their
accuracy evidence.

## Findings and research

[Chordino / NNLS Chroma](https://isophonics.net/nnls-chroma) is a close architectural
fit: native C++, harmonic note separation, tuning estimation, spectral whitening,
bass and treble chroma, a chord dictionary, and sequence decoding. It recognizes
named roots and several extended qualities, rather than merely deciding whether
a recording sounds major or minor. Its [source](https://github.com/c4dm/nnls-chroma)
is GPL-2.0-or-later. We use it as an optional, separate local process through
[Sonic Annotator](https://github.com/sonic-visualiser/sonic-annotator).

The combination of harmonic separation, tuning compensation and bass-aware
matching is useful to study. This comparison does **not** isolate the contribution
of each component: attributing the entire gain to NNLS alone would be unjustified.
The plugin's frozen defaults are used; only Harte label syntax is enabled.
It analyzes a complete recording, with global information. Its result is not a
claim about causal live-microphone recognition or end-to-end latency.

[CREPE](https://github.com/marl/crepe) is relevant to monophonic pitch. The
[paper](https://arxiv.org/html/1802.06182) distinguishes a 50-cent threshold from
fine tuning accuracy, and reports evaluation using resynthesized datasets.
Its high scores cannot be transferred to our real guitar recordings. We compared
MIT-licensed tiny and full models from the
[ONNX port v1.1.0](https://github.com/yqzhishen/onnxcrepe/releases/tag/v1.1.0)
locally on CPU; neither configuration beat the current tuner.

For contrast, the [98% chord-classification example](https://github.com/woodenikki/chord-classification)
predicts two classes, major and minor. That does not establish 98% accuracy for
chord roots, changes, extensions, bass notes, or tuning. We cannot identify the
specific unnamed project the user heard about.

## Frozen evaluation and limitations

The 60-recording GuitarSet selection and hashes from
[the desktop accuracy experiment](43-desktop-audio-and-accuracy.md) are unchanged:
30 accompaniment recordings for chords and 30 solo recordings for pitch, covering
six players and five styles. The selection was made without looking at these
predictions. Recognition receives the audio, not expected notes or chord labels.
The reference is the performed, manually verified chord annotation, rather than
the instructed progression. The native integration was then checked against the
independent Python scoring path and frozen output labels.

This remains one dataset, different recordings by the same players as development.
It is not an unseen-player, noisy-room, voice-pitch, or mixed-band evaluation.
The pretrained models' training overlap has not been established. These results
can guide the next change, but now that they have informed engine selection this
set is a comparison set, not an untouched final acceptance test for future changes.

### Chords: same audio and time coverage

All scores are exact duration-weighted overlap using
[mir_eval 0.8.2 rules](https://mir-eval.readthedocs.io/latest/api/chord.html).
The Chordino timeline is clipped to the built-in detector's existing time coverage.
No edge extrapolation or invented no-chord labels fill missing coverage.

| Comparison rule | Current built-in engine | Chordino | Scored / annotated time |
|---|---:|---:|---:|
| Major/minor reduction (`majmin`) | 71.55% | 86.90% | 522.40 / 758.75 s (68.85%) |
| Triad quality including diminished/augmented/suspended (`triads`) | 49.59% | 60.23% | 753.72 / 758.75 s (99.34%) |
| Full within-octave chord quality (`tetrads`) | 37.97% | 49.58% | 753.72 / 758.75 s (99.34%) |
| Same quality plus bass (`tetrads_inv`) | 31.28% | 40.40% | 753.72 / 758.75 s (99.34%) |

The simpler 86.90% figure is **not** full chord accuracy. It excludes 231.32
seconds of unsupported reference qualities. The richer comparisons include these
passages; only 5.03 seconds at analysis edges remain uncovered. A wrong or
unsupported prediction is a miss on a supported reference. Chord quality is
compared as pitch classes, not exact octave voicings or guitar finger positions.

Full quality plus bass by style:

| Style | Built-in | Chordino |
|---|---:|---:|
| Bossa nova | 20.58% | 42.78% |
| Funk | 7.86% | 15.90% |
| Jazz | 21.10% | 27.54% |
| Rock | 29.85% | 31.93% |
| Singer-songwriter | 63.94% | 72.50% |

The richer vocabulary is still limited. Suspensions, omitted notes, extensions,
and bass inversions need further work, especially on funk and jazz. One aggregate
number hides that weakness.

### Pitch: same 4,447 eligible output moments

| Method | Within 5 cents | Within 10 cents | Within 50 cents | Median absolute error, voiced |
|---|---:|---:|---:|---:|
| Current streaming pYIN tuner | 49.22% | 64.83% | 87.02% | 4.70 cents |
| CREPE tiny | 47.04% | 62.78% | 85.54% | 5.10 cents |
| CREPE full | 48.86% | 63.80% | 86.46% | 4.76 cents |

Unvoiced output counts as a miss. All methods use the same causal 15 Hz output
grid and conservative single-note eligibility rule. CREPE takes 1,024 samples at
16 kHz, resampled independently from each past-only input window, with zero
boundary padding. No future audio enters these windows. The graph normalizes its
input internally. Decoding is weighted argmax over nine bins, confidence 0.5,
level gate -55 dB, search range 50–1500 Hz. No offline Viterbi smoothing is used.
These are tested configurations, not a claim that all possible CREPE settings
would lose. Batch inference timing is not desktop latency. Four synthetic tones
check the model input/output contract only; they do not contribute to these scores.

## Changes

- Optional **Chordino - extended chords** in the desktop Reference recordings
  view. The default built-in engine remains available. Full Harte labels survive
  the native process adapter and appear in the readout and timeline.
- The app score explicitly says **major/minor agreement** and shows supported
  seconds. The stricter collection results are reported above; they are not
  mislabeled as the app's current per-recording score.
- Work runs off the audio/UI thread, locally, without a shell or upload. The
  helper receives a private plugin path. It is bounded to 60 seconds, can be
  cancelled on app exit, rejects malformed timelines and checks the actual
  Windows process exit code before publishing a score. Failure has no silent
  fallback to a different engine.
- The live tuner stays on its measured pYIN configuration. Reference pitch plots
  retain their separately labeled offline YIN measurement. No new transcription
  model or verified voice-pitch truth is implied.
- Downloaded code and models stay in ignored build directories. The header-only
  core acquires no runtime dependency. The original f3 project is unchanged.

## Provenance and reproduction

`tools/real_audio/community-downloads.json` records exact download URLs and
SHA-256 hashes. Sonic Annotator is the official Windows 64-bit v1.7 release.
The plugin is the [community Windows 64-bit build v1.1](https://github.com/jotapemj/nnls-chroma-win64/releases/tag/v1.1-win64),
not a binary from the original Chordino authors. Its published build recipe uses
unpinned upstream source; the downloaded binary hash is reproducible, but an exact
upstream source revision for that binary is not established. Licenses from both
distributions are retained. This is a local optional installation, not a newly
relicensed or bundled copy of the GPL implementation in the core.

From the repository root, after preparing the real-audio environment:

```powershell
build_real_audio/venv/Scripts/python.exe tools/real_audio/setup_community.py --pitch
cmake --build build_app_msvc --config Release --target mmathen_app chordino_check playback_check
build_real_audio/venv/Scripts/python.exe tools/real_audio/community_chords.py
build_real_audio/venv/Scripts/python.exe tools/real_audio/community_quality.py
build_real_audio/venv/Scripts/python.exe tools/real_audio/community_pitch.py
build_real_audio/venv/Scripts/python.exe tools/real_audio/verify_chordino.py
build_real_audio/venv/Scripts/python.exe tools/real_audio/check_desktop_audio.py
```

Omit `--pitch` to install only the optional chord helper. Models are not used by
the app. `--target <folder>` supports preparing the helper under `community` next
to the executable; development builds also find `build_real_audio/community` in
the repository. The two verification scripts accept `--exe` for another build.

Reports and raw predictions:

- `build_real_audio/community/{chord,quality,pitch}-report.json`
- Per-recording CSV/JSON output and the frozen `chordino-harte.ttl` transform
- `build_real_audio/community/native-check/verification.json`
- `build_real_audio/ui-audio/report.json` and native screenshots

## Verification

Both MSVC and GCC builds pass with warnings treated as errors. The core suite
passes 435 test cases (660,617 assertions), and the Python evaluation suite passes
17 tests. The native Chordino verification and final-device audio checks pass.

The native check reruns all 30 chord recordings and compares complete labels,
interval boundaries, major/minor projections, and scores with the frozen Python
run. Failure tests feed a nonzero-exit helper plausible CSV and cancel a stalled
helper: neither may produce a score. Parser tests reject invalid numbers,
unordered times, incomplete timelines, and malformed labels. The slash-bass
projection is cross-checked against mir_eval, including added seconds.

The final-device audio check covers tuner, analyzer, built-in reference, Chordino
reference, and mute. SDL's disk driver verifies nonzero, finite, unclipped output
and exact silence when muted; it does not itself play through speakers. The user
separately confirmed audible playback during the prior normal desktop run.

Next useful experiment: develop a richer chord dictionary and compare note
separation/bass-aware matching on development recordings, then reserve new players
or recordings for the next acceptance test. For the tuner, measure stable held
notes, attacks, transitions and room noise separately before changing models.
