# Music Mathematical Engine — Vision (Layer 0–30 Summary)

Status: planning · MVP: Phases 0–2 + 2.5

## Core idea

Music is represented as **math first, then rendered into sound**.
Notes, chords, rhythms are *derived structures*, not the foundation.

Forward direction:

```text
Math → Physics → Acoustics → Sound → Music
```

Reverse direction (analysis):

```text
Audio → Signal Analysis → Pitch/Rhythm/Harmony → Musical Structure
```

One engine covers: synthesis, physical modeling, composition, theory,
tuning research, procedural music, instrument simulation.

## Foundational axiom

> **Musical labels describe phenomena; they don't define the underlying phenomena.**

| Concept | Example |
|---|---|
| Frequency (physics) | `440 Hz` |
| Pitch (musical interpretation) | `A4` |
| Signal | pressure / voltage over time |
| Physical system | vibrating string |

These are related but **not interchangeable**. The architecture enforces this
via module dependencies and strong types.

## Key examples

12-TET (not hard-coded, just one tuning):

```
f = f_ref * 2^(n/12),  f_ref = 440 Hz
```

Arbitrary tuning as equation input:

```text
pitch(n) = 220 * 2^(n/19)      # 19-TET instrument
velocity(t) = 0.6 + 0.25*sin(t*pi/4)
note[i].duration = fibonacci(i) / fibonacci(i+1)
```

Chord as frequency relationships:

```
C_major (12-TET) = {1, 2^(4/12), 2^(7/12)}
C_major (just)   = {1, 5/4, 3/2}
```

Rhythm as temporal math:

```
duration = (60 / BPM) * beats
1 beat @120BPM = 0.5 s
dotted quarter = 0.5 * 1.5 = 0.75 s
```

## What this becomes

A **music physics engine** a DAW could sit on top of — not another DAW:

- instruments built from physical components
- conventional + microtonal composition
- procedural / algorithmic composition
- recording analysis + transcription
- tuning-system design lab
- room / acoustics simulation
- audio circuit modeling
- MIDI/MPE output, visualization, theory teaching
- scientific acoustic experiments

## Non-goals for MVP

No MIDI, chords, instruments, rooms, FFT framework, plugins, GUI,
or real-time device output in Phases 0–2.5. See `02-mvp.md`.
