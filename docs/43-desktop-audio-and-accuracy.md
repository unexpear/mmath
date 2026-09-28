# Desktop sound, interface, and real-recording accuracy

Measured 2026-09-19. This revision adds visible listening volume and mute,
repairs microphone-to-file playback, and applies a measured desktop guitar
profile. The public library's chord defaults and the copied f3 source are unchanged.

## Findings and research

The previous test window was launched with `--mute`; there was no unmute control.
A separate failure existed in the Recording selector: `stopMic()` set the engine
to `kNone`, so `Engine::playback` returned before producing any audio. Microphone
capture also rebuilt processors at 48 kHz without restoring a loaded file's rate.
`restoreFileSource` now restores both the stream format and processors, preserves
the reader position and loop marks, clears queued audio, and returns paused.

SDL applies stream gain while retrieving converted audio, after the analysis tap.
The listening controls therefore do not alter recognition or exported audio.
Research: [SDL stream gain](https://wiki.libsdl.org/SDL3/SDL_SetAudioStreamGain),
[clear stream](https://wiki.libsdl.org/SDL3/SDL_ClearAudioStream),
[get converted samples](https://wiki.libsdl.org/SDL3/SDL_GetAudioStreamData), and
[disk output driver](https://github.com/libsdl-org/SDL/blob/fa2c02bb6e21974a89ea9824bc53c9932abe5f9c/src/audio/disk/SDL_diskaudio.c).

The pitch hypothesis was temporal averaging: a 4096-sample window covers about
93 ms at 44.1 kHz, including older portions of changing guitar notes. The chord
hypothesis was excessive emphasis on weak pitch classes from strong log compression.
These were tested as configuration choices, not claimed as complete explanations
for all errors. Primary background: [pYIN parameters and two-stage decoding](https://librosa.org/doc/0.11.0/generated/librosa.pyin.html)
and [FMP chord templates and chroma processing](https://www.audiolabs-erlangen.de/resources/MIR/FMP/C5/C5S2_ChordRec_Templates.html).

## Selection and held-out results

Before any new predictions, `improve_accuracy.py` froze 60 unused GuitarSet 1.1.0
microphone recordings: the first unused lexicographic comp and solo per player/style,
excluding the three documented problem recordings. This covers six players and
five styles. Files and annotations are SHA-256 pinned in `build_real_audio/accuracy/holdout.json`.
These are different recordings by the same players, not a test of unseen players.

Development used only the existing 30 comp and six Rock solo recordings. Tested:
pitch windows 2048/3072/4096, transition limits 60/120 bins; chord upper frequency
1200/2500/5000 Hz and compression 1/10/30/100. All other settings stayed fixed.
Selected 2048 samples, 60 bins, and chord 2500 Hz/compression 10 before inspecting
holdout scores. No settings were retuned on the holdout.

| Held-out metric | Previous | Updated |
|---|---:|---:|
| Major/minor chord agreement, duration weighted | 54.92% | 71.55% |
| Correct chord seconds / supported seconds | 286.89 / 522.40 | 373.78 / 522.40 |
| Tuner within 50 cents | 85.81% | 87.02% |
| Tuner within 10 cents | 61.55% | 64.83% |
| Tuner within 5 cents | 44.79% | 49.22% |
| Median absolute error among voiced outputs | 5.54 cents | 4.70 cents |
| Voiced outputs / eligible pitch moments | 4235 / 4447 | 4236 / 4447 |

Pitch is scored on an identical causal 15 Hz grid using the conservative old
4096-sample eligibility window for both configurations. That entire window must
lie inside exactly one annotated note. Expected frequency is the nearest voiced
string contour at the output time, within half the annotation sampling interval.
Unvoiced outputs count as misses in every within-cents percentage. No future
audio, selected strings, nominal targets, or reference notes enter the detector.

Chord scoring uses exact interval overlap and mir_eval major/minor reductions.
522.40 of 758.75 annotated seconds are supported (68.85% coverage); 231.32 seconds
contain unsupported chords, with the remaining 5.03 seconds uncovered at analysis
edges. Suspended, diminished, extended, or mixed passages are not generally solved
by this major/minor vocabulary. Results are not full-song transcription accuracy.

On development data, chord agreement rose 53.11% to 61.87%; tuner within 10 cents
rose 58.10% to 62.28%, and within 50 cents 85.06% to 86.08% (790 common moments).
The modest pitch gain still leaves many errors. A green display is a tolerance
indicator, not a promise of precision-tuner performance.

## Changes

- Native navigation, readable system font, larger pitch display, six string tiles,
  cents meter, chord cards and clearer finger/barre diagrams; advanced settings collapse.
- Mute/unmute and listening volume remain visible across the three views.
- The streaming detector retains the same pYIN candidate and history logic, with a
  shorter input window. Higher source rates allocate enough samples for the 50 Hz
  lower search bound, including interpolation neighbours.
- Fresh reference chord analysis uses the selected desktop profile. Saved baseline
  scores remain labeled as baseline. Reference pitch graphs still use their separate
  offline YIN baseline; the live tuner result must not be conflated with that graph.

## Verification and reproduction

`playback_check` drives the production callback through a real SDL stream using
the copied CC0 A2 guitar recording. It checks nonzero finite output, mute/unmute,
the analysis tap while muted, 48 kHz microphone-to-44.1 kHz file restoration,
preserved position/loop, seek, pause/resume, and exact half-volume scaling.

`check_desktop_audio.py` captures the actual native application's final SDL device
output with SDL's disk driver, plus screenshots in analyzer, tuner, muted, and
reference views. Non-muted output is nonzero and unclipped; muted output is exactly
zero. These automated runs intentionally do not sound through speakers. Normal
playback was then reopened without the disk driver or `--mute`, and the user
confirmed hearing the repeated real A2 guitar recording.

`verify_accuracy.py` compares the production desktop helpers' predictions against
the frozen experiment outputs, independently of labels. Reports, per-track outputs,
and executable hashes are under `build_real_audio/accuracy/`. The earlier f3
comparison remains a frozen comparison of the previous 4096-sample configuration.

```powershell
cmake -S . -B build_real_audio/cpp -DBUILD_TESTING=OFF -DMMATHEN_BUILD_VALIDATION=ON
cmake --build build_real_audio/cpp --config Release --target accuracy_probe
build_real_audio/venv/Scripts/python.exe tools/real_audio/improve_accuracy.py
build_real_audio/venv/Scripts/python.exe tools/real_audio/improve_accuracy.py --pitch 2048-60 --chords 2500-10
build_real_audio/venv/Scripts/python.exe tools/real_audio/verify_accuracy.py
```

Rerunning the last command verifies shipped behavior; rerunning the first two
regenerates development/holdout predictions with the already frozen manifest.
Keep existing result files if retaining the original experiment executable hashes.
