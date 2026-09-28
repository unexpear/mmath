# Desktop tuner comparison and native controls

This is the frozen comparison before the shorter desktop input window was selected.
See [the subsequent sound and accuracy revision](43-desktop-audio-and-accuracy.md)
for the current desktop profile and its separate held-out recording results.

Measured 2026-09-19 on Windows x64, MSVC Release. **Keep the existing desktop
pYIN detector.** It was more accurate on this fixed set of real guitar recordings.
The f3 native engine required less processing time and made fewer octave errors,
but its other pitch errors and missed voiced notes outweighed that advantage.

## Findings

The six previously selected GuitarSet Rock solo recordings were replayed through
both engines without supplying either engine with notes, targets, or annotations.
Both were scored on the same 790 eligible moments on a common 15 Hz display grid.

| Metric | Existing desktop pYIN | f3 v1.1.3 native core |
|---|---:|---:|
| Within 50 cents, including unvoiced misses | 85.06% (672/790) | 74.81% (591/790) |
| Within 10 cents, including unvoiced misses | 58.10% | 41.52% |
| Within 5 cents, including unvoiced misses | 35.82% | 18.35% |
| Median absolute error among voiced outputs | 7.38 cents | 11.66 cents |
| Missed voicing | 2.91% | 7.34% |
| Octave errors | 15 | 9 |
| Median adjacent error change within a note | 8.70 cents | 10.70 cents |
| Stable response: successful notes / eligible notes | 47/54 | 40/54 |
| Median stable response among successes | 212 ms | 215 ms |
| Processing seconds per audio second, one run | 0.162 | 0.099 |

The 11 isolated Karoryfer Emily Guitar recordings copied from f3 were checked
separately. Agreement within 50 cents of the nominal labeled note was 100%
(451/451) for desktop pYIN and 94.68% (427/451) for f3. These labels are intended
notes, not independently measured pitch contours, so they do **not** certify
fine tuning accuracy. They are excluded from the GuitarSet table.

These measurements compare the engines' existing configurations, not algorithms
under an equal CPU budget. Six performances in one style are a limited sample.
The earlier 77.27% pitch score used offline YIN with different windows and scoring
times; it is not a before/after score for this streaming comparison.

## Research and configuration

- Inspected the exact installed f3 engine (1.1.3), its native pipeline, dispatcher,
  quality presets, and StandardTune's acoustic instrument profile and wrapper.
  [Upstream documentation](https://github.com/denizyesilirmak/react-native-tuner-engine)
  currently describes a newer fusion implementation; the copied installed version
  uses the YIN/pYIN/cepstrum ensemble. No upgrade was performed.
- Desktop: the original D35 pYIN configuration is now shared in
  `app/streaming_tuner.h`: 4096 samples, 15 updates/second, rolling 24-frame Viterbi,
  50–1500 Hz, max jump 60 bins, and -55 dBFS display gate. Initial partial windows
  are zero-padded as in the app. The helper preserves the original predictions.
- f3: 2048 samples, 1024-sample hop (balanced quality), 55–1400 Hz,
  40 Hz high-pass filter, -55 dBFS gate, confidence 0.75, A4=440,
  onset reset enabled, median-5, EMA 0.32, and three-frame note hysteresis.
  The native core is unchanged, including its treatment of overlapping frames.
  Its React wrapper's additional median, display hold, target mapping, and
  harmonic correction are **not** part of this native-engine comparison.
- Both receive the same original 44.1 kHz mono samples. The f3 core is configured
  to that rate instead of emulating a phone's 48 kHz microphone/resampler.
- Read the pinned [Dear ImGui API](https://github.com/ocornut/imgui/blob/f1cc2ae15e53a861a874c3034aae6798fde194ab/imgui.h),
  existing native drawing patterns, [SDL capture documentation](https://wiki.libsdl.org/SDL3/SDL_RenderReadPixels),
  and [CMake subdirectory documentation](https://cmake.org/cmake/help/latest/command/add_subdirectory.html).

## Scoring rules

Each output records its input-window start and the time it becomes available
(window end). At each desktop update, use the latest f3 result already available;
never choose a future result or interpolate predictions. The union of both current
input windows through the display time must lie fully within one annotated note,
with no other annotated note overlapping. The reference is the nearest voiced
GuitarSet string contour sample at that display time, within half its documented
256/44100-second spacing. Ineligible windows get no score; both engines have the
same denominator. Silence/unvoiced decisions on eligible notes are misses.

Error is `1200 log2(estimated/reference)`. Octave errors are within 50 cents of a
nonzero integer multiple of 1200 cents. Stability is the median absolute change in
this signed error between adjacent scored display ticks on the same note; it is
not simply movement of the played pitch. All valid voiced errors are included.

Stable response requires three consecutive correct display ticks within 50 cents
after an annotated onset. Eligible notes last at least 250 ms and do not overlap
another annotated note; same-pitch repetitions are excluded because continued
tracking cannot establish reacquisition. Report failures as well as the median
delay among successes. This is causal replay response, excluding microphone,
device, operating-system scheduling, and audio-output delay. Processing times
are one-run wall-clock diagnostics, not controlled hardware performance claims.

## Changes

- **Tuner & chords** opens a full native screen: eight six-string guitar tunings,
  chromatic mode, individual string targets, A4 calibration 430–450 Hz, adjustable
  green display tolerance, room/display gate, pitch meter, signal level and
  confidence. The current detector remains in use. Effects are bypassed while
  this view is open and restored when returning to the analyzer.
- Optional harmonic assistance is off by default and applies only to an explicitly
  selected string. An adjusted reading shows both raw and adjusted frequency.
  No target information reaches detection or blind benchmark scoring.
- All **42 authored chord voicings** in the source snapshot are available with
  finger numbers, open/muted strings, barre spans, fret position, and sounding
  notes. These diagrams always use standard guitar tuning and A4=440, independent
  of the tuner controls. Unsupported voicings are explicitly unavailable.
- The Reference demo has a collapsible **Compare chord shapes / notes** section
  showing example voicings for the expected and detected triads. Extensions may
  be omitted by the existing major/minor scoring reduction; no claim is made
  that the displayed fingering was physically played.
- Tuner history resets on source changes and seeks to avoid carrying a previous
  recording's pitch into the next one.
- The engine snapshot is comparison-only, behind `MMATHEN_COMPARE_TUNERS=ON`.
  Neither the desktop app nor the header-only core links it by default. All source
  copies and adaptations retain the applicable MIT licenses and provenance.

## Reproduce

Use the prepared recordings/environment from `41-real-audio-reference-demo.md`.
The additional 11 WAV files are under
`build_real_audio/reference_sources/f3-emily-guitar`, with CC0 license/manifest.

```powershell
cmake -S . -B build_real_audio/cpp -DBUILD_TESTING=OFF -DMMATHEN_BUILD_EXAMPLES=OFF -DMMATHEN_BUILD_VALIDATION=ON -DMMATHEN_COMPARE_TUNERS=ON
cmake --build build_real_audio/cpp --config Release --target tuner_probe --parallel 4
build_real_audio/venv/Scripts/python tools/real_audio/compare_tuners.py
build_real_audio/venv/Scripts/python -m unittest discover -s tools/real_audio -p 'test_*.py' -v
cmake --build build_app_msvc --config Release --target mmathen_app --parallel 4
build_app_msvc/Release/mmathen_app.exe --tuner build_real_audio/reference_sources/f3-emily-guitar/a2.wav
```

`build_real_audio/tuner-comparison/report.json` contains all per-track scored
points, response successes/failures, source/binary/audio hashes, and aggregate
results. Neighboring JSON files contain raw engine outputs and timing for every
update; `summary.json` contains the table totals. Nothing under `D:/dev/f3` needs
to be executed, built, or changed to rerun the comparison.

## Verification

- MSVC Release and UCRT64 GCC desktop builds pass, including warnings as errors.
- **434 test cases / 660,599 assertions pass**, including calibration, invalid
  readings, target-only assistance, chord notes/enharmonics, and exact comparison
  of the shared streaming helper with the old D35 logic across a 24-frame rollover,
  note transition, silence, and reset.
- **17 Python scorer tests pass**, including causal output selection, shared
  eligibility, missing voices, octave mistakes, and tighter tuning thresholds.
- Native capture of the real A2 recording showed 109.989 Hz, voiced, no clipping.
  Interactive checks covered wrong/right selected strings, calibration target
  changes, Drop D, and an F barre diagram with the expected sounding notes.
- The original source is read-only throughout. Copy hashes and final verification
  are recorded alongside the comparison artifacts.

Live microphone/device latency and a broad multi-instrument accuracy assessment
remain unmeasured. Metronome, lessons, games, and 3D instruments were assessed as
possible later work; they are not part of this tuner-and-diagram change.
