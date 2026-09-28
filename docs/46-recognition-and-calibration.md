# Desktop recognition and device calibration

Update: [report 47](47-native-chord-engine.md) supersedes the chord-engine choice
below. Chordino was subsequently removed from the desktop at the user's request;
the fine tuner and calibration work described here remains in place.

September 23, 2026. The desktop now uses fine pitch tracking after acquisition,
offers microphone clock correction, and selects the installed Chordino helper by
default for reference chords. The basic detector remains selectable. The new
experimental built-in harmonic/bass recognizer failed reserved-recording testing
and is **not shipped in the desktop's recognition path**.

## Findings and research

More arithmetic precision did not help the previous chord experiment. This work
instead tests signal interpretation. The published
[NNLS Chroma description](https://isophonics.net/nnls-chroma) motivates harmonic
note fitting, tuning compensation, separate bass information and a richer chord
dictionary. The existing Chordino helper already supplies these together. Its
settings, host, plugin and transform hashes are unchanged from the
[community comparison](44-community-recognition-comparison.md).

The tuner retains the current short-window pYIN detector for acquisition. An
additional past-only phase estimate follows the principle of
[demodulated sinusoidal phase advance](https://www.dsprelated.com/freebooks/sasp/Phase_Vocoder_Sinusoidal_Modeling.html).
It is allowed only after four voiced readings within 15 cents, with at least two
strong harmonics agreeing within 5 cents. Three overlapping windows support a
local linear-frequency extrapolation to the output time. The selected-string and
A4 controls never enter this estimator. This is a measured engineering choice,
not a claim that guitar strings have perfectly harmonic or constant-frequency
spectra.

## Frozen evaluation

Development used 30 previously measured accompaniment recordings and 36 previously
measured solo recordings. An independent NumPy/SciPy implementation was compared
with C++: all 30 chord timelines match exactly, and all 36 pitch timelines agree
within 0.000001 Hz with identical voicing decisions. SciPy's
[active-set NNLS solver](https://docs.scipy.org/doc/scipy/reference/generated/scipy.optimize.nnls.html)
and the experimental C++ coordinate solver are independent implementations.

Before new predictions, 30 additional chord recordings and 30 additional solo
recordings were reserved, one of each per player/style. Input and annotation
hashes are checked. These are new recordings by the same six GuitarSet players,
not unseen-player, voice, room-noise or mixed-band validation. References never
enter the recognition executables. Chord recognition is offline and can use
the whole recording; tuner windows contain no future samples.

The native candidate executable was frozen before scoring this set:
`eefe048320482f4452afe81a6bc06704bf4c37848f30642a338fabe8878ad3fc`.
The acceptance rule required better chord quality plus bass without a major/minor
loss; pitch required better within-5-cent agreement and lower sustained-note
median error, with at most 0.5 percentage point loss within 50 cents. No detector
parameters changed after this freeze. The scripts prevent a second held-out run
from overwriting the assessment.

### Chords on the 30 new recordings

Scores use exact duration-weighted overlap under
[mir_eval 0.8.2 rules](https://mir-eval.readthedocs.io/latest/api/chord.html).

| Method | Major/minor reduction | Full within-octave quality | Quality plus bass |
|---|---:|---:|---:|
| Basic built-in detector | 66.79% | 38.65% | 31.84% |
| New built-in harmonic candidate — rejected | 66.79% | 39.03% | 29.69% |
| Existing, unchanged Chordino configuration | 81.88% | 50.74% | 41.31% |

Major/minor scores cover 578.60 of 883.59 annotated seconds; the richer comparisons
cover 878.83 seconds. Unsupported reference qualities are excluded only by the
selected comparison rule, equally for every method. No missing edges are filled
with invented predictions. Quality means pitch-class quality, not precise guitar
voicing or every note's octave. **81.88% is not full chord accuracy.**

The new native candidate used harmonic NNLS features to add sixths, sevenths and
inversions while preserving the established triad path. It improved quality plus
bass from 22.50% to 25.96% on development, but failed on reserved recordings.
Earlier broad-dictionary and long-window pitch attempts also failed development.
These rejected attempts are retained as validation experiments, not silently
substituted into the product.

After the native candidate failed, the already frozen Chordino configuration was
evaluated on the same new recordings, with no settings search. Its native desktop
adapter was then checked against the independent Python predictions for all 30
recordings: full labels, boundaries, major/minor projections and scores match.
Failed-helper and cancellation checks pass. This set is now comparison evidence
and must not be reused as untouched acceptance data for future tuning.

### Tuner on the 30 new solo recordings

Both methods use the same 15 Hz causal replay grid and the same conservative
single-note eligibility rule. The 4,437 eligible moments must have one annotated
note across the preceding 4,096 samples. Unvoiced output counts as a miss; medians
are conditional on voiced output. Both methods voice 4,327 of these moments.

| Measure | Previous tuner | Fine tracking |
|---|---:|---:|
| Median absolute error, all eligible voiced moments | 5.38 cents | **5.22 cents** |
| Within 5 cents, all eligible moments | 46.77% | **47.37%** |
| Within 10 cents, all eligible moments | 64.01% | **64.21%** |
| Within 50 cents, all eligible moments | 87.47% | **87.47%** |
| Median error after at least 250 ms of the same note | 4.69 cents | **4.27 cents** |
| Within 5 cents after at least 250 ms | 50.17% | **51.57%** |

The sustained-note group contains 1,788 eligible moments, 1,721 voiced. It is
defined by annotation age, not by whether the algorithm chose fine tracking;
vibrato or bends can still occur. The remaining 2,649 eligible moments form the
early-note group. There, within-5-cent agreement changes from 44.47% to 44.55%,
while within-10-cent agreement falls slightly, from 63.57% to 63.46%.

An additional 2,734 transition moments have a clear current reference pitch but
cross a note boundary or ambiguity in the longer preceding window. They are
reported separately, not silently discarded. Within-50-cent agreement is unchanged
at 60.39%; median voiced error changes from 7.46 to 7.50 cents, and within-5-cent
agreement from 29.48% to 29.41%. Overall 90th-percentile voiced error also changes
slightly adversely, from 52.14 to 52.22 cents. This is a modest improvement focused
on sustained notes, not an improvement to every reading or a universal guarantee.

Fine tracking activates on 750 updates across these recordings. The largest
per-recording 95th-percentile processing time is 12.88 ms on this Windows test
machine, below the 66.67 ms interval between ideal 15 Hz updates. This is processing
time, not microphone-to-display latency, a mobile performance claim, or a battery
measurement. The app still uses the short pYIN path immediately for acquisition.

## Changes in the desktop

- **Fine tracking** appears below the tuning verdict when the harmonic estimate
  is active. Frequency and period remain consistent. Silence and missing new
  input clear the refinement history; stale buffers cannot complete calibration.
- **Device calibration**, under the tuner settings area, accepts an independent
  reference frequency from 100 to 1,000 Hz. It requires 45 consecutive usable
  readings, a central spread no greater than 0.2 cents, sufficient signal level,
  and a correction within 1,000 ppm. An unusable source times out after 150
  attempted updates. Prior correction survives a cancelled/failed attempt.
- The correction is `known / measured`, applied only to microphone readings.
  It does not change playback, the chosen A4 standard, or recognition of files.
  The uncorrected frequency is available under Signal details. Source changes and
  recording-device add/remove/format events clear correction. This uses the
  documented [SDL audio-device events](https://wiki.libsdl.org/SDL3/SDL_AudioDeviceEvent).
- Calibration is session-only. The UI explicitly states that absolute accuracy
  is unverified. The user has no independently calibrated source available;
  therefore physical clock calibration and absolute sub-cent certification could
  not be performed. Synthetic clock offsets test the correction formula only.
- Reference chords select **Chordino - extended chords** when its optional local
  helper is installed. The basic method remains selectable; `--basic-chords`
  explicitly selects it for comparisons. If a selected Chordino run fails, the
  app reports the error rather than silently swapping detectors. No GPL code is
  linked into the core and no audio is uploaded.

## Verification and reproduction

MSVC and GCC desktop builds pass with warnings treated as errors. Both core suites
pass **438 cases / 660,706 assertions**; the Python evaluation suite passes
**17 tests**. New contracts cover clock-correction direction, file bypass,
unstable/silent references, acquisition preservation, consistent frequency/period,
silence/reset behavior and the experimental chord vocabulary. Synthetic signals
in those contracts contribute no real-recording accuracy evidence.

Six native desktop checks cover tuner, calibration view, mute, analyzer, basic
reference and default Chordino reference. Final-device audio is finite, unclipped
and nonzero in all playing cases; mute is exactly silent. Native screenshots were
visually inspected. SDL's disk output verifies the application path; it does not
prove sound came from speakers during these runs. The user's earlier audible
playback confirmation remains separate evidence.

All 38 copied tuner files and their originals in `D:\dev\f3` retain their recorded
hashes. That project was not edited.

Main artifacts in `build_real_audio/recognition-improvement`:

- `selection.json`, `pitch-holdout.json`; chord selection is the existing
  `build_real_audio/math-improvement/holdout.json`.
- `native-development-report.json`, `native-holdout-report.json` and raw outputs.
- `chordino-reserved-report.json`; native adapter verification is in
  `build_real_audio/community/native-check-reserved/verification.json`.
- The final audio checks and screenshots are in `build_real_audio/ui-audio`.

From the prepared real-audio environment:

```powershell
cmake --build build_app_msvc --config Release --target mmathen_app recognition_probe
build_real_audio/venv/Scripts/python.exe tools/real_audio/recognition_experiment.py chords
build_real_audio/venv/Scripts/python.exe tools/real_audio/recognition_experiment.py pitch
build_real_audio/venv/Scripts/python.exe tools/real_audio/verify_recognition.py
# These two refuse to overwrite their existing reserved-set reports:
build_real_audio/venv/Scripts/python.exe tools/real_audio/verify_recognition.py --heldout
build_real_audio/venv/Scripts/python.exe tools/real_audio/verify_chordino_reserved.py
build_real_audio/venv/Scripts/python.exe tools/real_audio/verify_chordino.py --reserved
build_real_audio/venv/Scripts/python.exe tools/real_audio/check_desktop_audio.py
```

Read the saved reserved reports instead of deleting them to rerun acceptance.
Future recognition changes require a new reserved set, preferably new performers
and recording conditions. Independent hardware calibration remains a separate
measurement task when a suitable reference becomes available.
