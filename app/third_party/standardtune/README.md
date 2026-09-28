# StandardTune adaptations

The chord voicings and tuner presentation logic were adapted from the user's
`D:/dev/f3/guitar-tutor` copy, read-only, on 2026-09-19. Its MIT license is retained
here. `source-manifest.json` identifies the exact original files by SHA-256.

`app/chord_shapes.h` contains all authored chord shapes from that snapshot.
`app/tuner_controls.h` adapts selected-string targeting, calibration, verdicts,
and explicitly selected harmonic assistance. The desktop display is new Dear ImGui
code; no React Native runtime or mobile bridge is needed.

The detector receives no target information. Harmonic assistance is off by
default, available only with an explicitly selected string, and displays both raw
and adjusted frequency. It is never used by the blind comparison scorer.
