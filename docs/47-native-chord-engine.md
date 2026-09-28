# Our chord engine and public-domain verification

September 23, 2026. The desktop uses our native recognizer. Chordino's selector,
automatic discovery and command-line switch have been removed. Its adapter was
moved from `app` to `tools/real_audio`, where the old comparison results and
separate helper remain available for study. The desktop does not call them.

## Findings and research

The former detector compares one chromagram with ideal major/minor triads. Real
guitar spectra contain harmonics, unequal string levels and inversions. The FMP
[chord-template discussion](https://www.audiolabs-erlangen.de/resources/MIR/FMP/C5/C5S2_ChordRec_Templates.html)
describes learned templates and statistical models as alternatives to ideal
binary templates. We implemented our own compact, transposition-shared acoustic
model using four frequency bands. No Chordino source, predictions or parameters
are used in the model. Python/SciPy are used for development only; inference is
our C++ code and existing FFT/HMM implementation.

The initial unrestricted learned model failed its reserved 30-recording test:
major/minor 57.85% to 47.33%, full quality 26.49% to 18.44%, and quality plus bass
22.47% to 13.93%. All seven simple public-domain examples passed nevertheless.
This candidate was rejected. Passing those seven examples would have hidden a
substantial generalization failure.

## Accepted changes

- The original guitar chroma and triad emissions remain the root/third detector.
  A steadier offline Viterbi path changes self-transition probability from 0.9
  to 0.995 in the desktop wrapper. Public library defaults are unchanged.
- The learned model can only choose qualities and bass inversions consistent
  with that path. Effective outputs are major, minor, dominant seventh, major
  seventh, minor seventh, major sixth and minor sixth, with chord-tone basses.
  It cannot turn an established major root into a different root or minor third.
- Acoustic features use the full 80–2500 Hz band plus separate 80–300, 300–1000
  and 1000–2500 Hz bands. Each is compressed and normalized. Shared weights rotate
  with each candidate root. No file name, reference label, key, lead sheet or
  progression is passed to recognition.
- The small model is a regularized multinomial linear classifier, fitted to
  stable interiors from 90 microphone recordings. The final constrained decoder
  uses the same fitted weights as the rejected unrestricted decoder. It was not
  refitted to the failed test recordings. An independent NumPy implementation
  agrees with every native label and timestamp on all 120 development recordings
  and all 30 new reserved recordings.
- Recognition remains offline recording analysis. Playback, microphone pitch
  tracking and calibration calculations were not changed.

## Reserved real-recording results

After rejecting the first candidate, its 30 test recordings became development
data, alongside the previous 90. The constrained approach was selected on those
120 consumed recordings. Detail penalties 0, 1, 2 and 4 were considered; zero
performed best in both consumed groups. Triad persistence 0.9, 0.98 and 0.995 was
then compared, and 0.995 selected. These are development comparisons, not fresh
accuracy evidence.

A different 30-recording set was selected by deterministic hash ordering,
one per player/style, excluding all 120 consumed recordings and documented
annotation issues. Parameters and executable hashes were frozen before scoring.
The acceptance rule required better full-quality and quality-plus-bass scores
with no major/minor loss, and no lost correct public-domain clip decisions.

| Same 30 reserved recordings | Previous native engine | Improved native engine |
|---|---:|---:|
| Major/minor agreement | 68.30% | **71.19%** |
| Full within-octave chord quality | 43.19% | **46.02%** |
| Chord quality plus bass | 31.96% | **33.85%** |

These are duration-weighted [mir_eval comparison rules](https://mir-eval.readthedocs.io/latest/api/chord.html).
Major/minor eligibility is 671.015 of 945.113 annotated seconds. The richer
comparisons cover 940.222 seconds. Unsupported labels and unanalysed edges do not
receive invented credit. Quality does not certify every note's octave or voicing.
These are new recordings by the same six players, with shared progressions;
they do not establish general performance on unseen players, bands or phones.
Scores from the prior report's different 30 recordings are not directly comparable.

The maximum native recognition time was 2.412 seconds for a whole recording on
this Windows machine during concurrent validation. This is not live latency.

## Public-domain recordings

Seven original acoustic-guitar recordings by WBOSITG, published in 2008, were
downloaded from Wikimedia Commons. Each file page contains the author's explicit
public-domain dedication, rather than merely CC0 metadata. These are real
recordings, not synthesized chords. Saved originals, decoded WAVs, source-page
snapshots and SHA256 hashes are in `build_real_audio/public-domain-chords`.

| Reference | Source |
|---|---|
| A major | [A-major.ogg](https://commons.wikimedia.org/wiki/File:A-major.ogg) |
| B major | [B-major.ogg](https://commons.wikimedia.org/wiki/File:B-major.ogg) |
| C major | [C-major.ogg](https://commons.wikimedia.org/wiki/File:C-major.ogg) |
| D major | [D-major.ogg](https://commons.wikimedia.org/wiki/File:D-major.ogg) |
| E major | [E-major.ogg](https://commons.wikimedia.org/wiki/File:E-major.ogg) |
| F major | [F-major.ogg](https://commons.wikimedia.org/wiki/File:F-major.ogg) |
| G major | [G-major.ogg](https://commons.wikimedia.org/wiki/File:G-major.ogg) |

Both engines match all seven clip labels and all 589 analysed frames under the
quality comparison. They were never used for fitting weights or parameter
selection. The second run is regression evidence: they had already been examined
with the rejected candidate. The uploader supplies whole-clip chord labels, not
verified bass, per-note pitches or precise onset boundaries. Seven easy major
chords from one source cannot establish near-perfect overall recognition.

The desktop's recording list now includes `PD-A-major` through `PD-G-major`.
Alternatively open `build_real_audio/public-domain-suite.tsv`. The source and
annotation limitations appear in the desktop, and missing note annotations are
shown as unavailable rather than as silence.

Additional deterministic tests made the same clips 20 dB quieter or added white
noise at whole-clip SNRs of 20 and 10 dB. Quiet copies pass 7/7. With either noise
level, our improved engine passes 6/7 while the old detector passes 7/7: the B
major clip gains an incorrect seventh. These transformed copies are not new
recordings or real-room tests. This regression is disclosed; noise robustness
and conservative extension decisions remain work to do. No 98% claim is justified.

The existing training/evaluation corpus is separately licensed:
[GuitarSet 1.1.0](https://zenodo.org/records/3371780), CC BY 4.0, by Qingyang Xi,
Rachel M. Bittner, Johan Pauwels, Xuzhou Ye and Juan P. Bello (2018).
The model parameters were fitted from those recordings and annotations. This
corpus is not labeled public domain. The seven new verification recordings are.

## Verification and reproducibility

Both MSVC and GCC core suites pass **440 cases / 666,813 assertions**. New contracts
check transposition consistency, silence, gain invariance, invalid inputs and
label bounds. The existing Python evaluation suite passes **17 tests**. MSVC and
GCC desktop builds pass with warnings treated as errors.

Six desktop output checks cover tuner, calibration, mute, analyzer, our guitar
reference recognizer and the public-domain reference demo. Playing output is
finite, nonzero and unclipped; mute is exactly zero. SDL disk output verifies the
application path, not physical speaker audibility. Native screenshots were
inspected. The seven public-domain scores were independently recomputed through
the actual desktop measurement adapter. The original `D:\dev\f3` was not edited.

Key artifacts:

- `build_real_audio/native-chords/reserved-report.json`: failed unrestricted model.
- `build_real_audio/native-chords/frozen-probe.exe`: preserved first candidate.
- `build_real_audio/native-chords-v2/selection.json`: frozen inputs and acceptance.
- `build_real_audio/native-chords-v2/reserved-report.json`: accepted fresh test.
- `build_real_audio/native-chords-v2/stress/report.json`: disclosed noise regression.
- `build_real_audio/public-domain-chords/manifest.json`: source and license evidence.
- `build_real_audio/ui-audio/report.json`: native desktop playback checks.

Development tools: `native_chord_model.py`, `develop_anchored_chords.py`,
`prepare_native_chords_v2.py`, `export_native_chord_model.py`,
`verify_native_chords.py`, `public_domain_chords.py`, and `stress_public_chords.py`
under `tools/real_audio`. The verification tool refuses to overwrite a reserved
report and checks frozen model, executable and manifest hashes. Keep the reports;
future tuning needs new independent recordings and evaluation data.
