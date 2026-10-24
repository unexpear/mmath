# Ensemble play for the winds

Research note. Citations are from the literature as recalled; items marked
VERIFY still need the page or table checked before a number goes into code.
Anything without a source is UNSOURCED.

## What players do together that they don't do solo

| Behaviour | Source | Applies to winds | Status |
|---|---|---|---|
| Onsets are not simultaneous: rms asynchrony ~30-50 ms in small ensembles, the melody voice tends to lead | Rasch, Acustica 43, 121-131 (1979) | yes | model: shared leader grid done; sourced jitter not yet |
| Leader/follower: followers track the leader's timing, the leader adapts less | Goebl & Palmer, Music Perception 26, 427-438 (2009); Keller & Appel, Music Perception 28, 27-46 (2010) VERIFY | yes (trumpet lead) | done: parts aligned to the trumpet's measured onset |
| Phase correction between players, gain per pair | Wing, Endo, Bradbury & Vorberg, J. R. Soc. Interface 11, 20131125 (2014) | yes | proposed below |
| Intonation moves toward just intervals in held chords (thirds lowered ~14 c) | Kopiez, Music Perception 20, 383-410 (2003) (trumpet duets) VERIFY | yes | not done |
| Balance and blend: accompaniment below the melody | UNSOURCED level (used -5 dB clarinet, -3 dB trombone) | yes | mix only |
| Shared breath and phrasing: same tonguing, breath points, phrase ends | practice; no number | yes | done: harmony takes the melody's rhythm and tongue, same 0.56 s lead-in |

## How orchestras stay together

- **Conductor beat-following.** Players synchronise with the peak of the
  baton's acceleration rather than the lowest point of the beat (Luck &
  Toiviainen, Music Perception 24, 189-200, 2006). Wöllner and colleagues
  studied how conductor gestures signal tempo (Wöllner, Deconinck,
  Parkinson, Hove & Keller, 2012, VERIFY title and venue) and players
  anticipate the beat from the preparatory gesture.
- **Sections versus soloists.** A section averages its members, so the
  section onset is spread wider but its mean is steadier; a soloist leads
  and the others follow (Rasch 1979 measured the spread; section numbers
  for orchestras are VERIFY).
- **Seating and sound travel.** c = 343 m/s at 20 C, so 10 m across a stage
  is about 29 ms. Players far from each other cannot follow by ear alone and
  rely on the conductor's visual beat (the delay is physics; the reliance
  is practice, UNSOURCED).
- **Click track versus conductor.** Tapping to a metronome gives a negative
  mean asynchrony, taps ahead of the click by tens of ms (Repp, Psychonomic
  Bull. & Rev. 12, 969-992, 2005; Repp & Su, 20, 403-452, 2013). Error is
  corrected two ways: phase correction (shift the next onset by a fraction
  alpha of the last asynchrony) and period correction (change the tempo)
  (Vorberg & Wing 1996; Mates 1994; reviewed by Repp 2005).
- **Tempo drift.** Groups without a click drift; a linear phase-correction
  model with alpha below 1 plus timekeeper noise reproduces drift as a
  random walk of the shared period (Wing et al. 2014; the drift rate
  for winds is UNSOURCED).

## Proposed model (not implemented this run)

1. One conductor clock: beat times T_k on a period P (here 0.28 s).
2. Each player i keeps its own timekeeper with motor noise and corrects
   phase: A_{i,k} = t_{i,k} - T_k, t_{i,k+1} = t_{i,k} + P_i - alpha_i A_{i,k}
   (+ noise). alpha from Wing et al. 2014 (VERIFY values, roughly 0.1-0.3
   per partner), noise sd so that the rms asynchrony lands in Rasch's
   30-50 ms.
3. Follower players add the hearing delay d_ij / c to the leader.
4. Each instrument's own speech latency (measured below) is compensated so
   the *sounding* onset, not the command, is on the beat.
5. Period correction (beta) only at tempo changes, from the conductor.

## What was done this run (When the Saints)

Measured on the old arrangement: shared 0.28 s grid and the same 0.56 s
lead-in on every part; all parts written at concert pitch (no Bb
transposition); sounding onsets re grid, tongued notes: trumpet 13.9 ms,
clarinet 10.9 ms, trombone 53.0 ms (legato half notes). The trombone sat
on F4/C5, on top of the melody, and the clarinet ran a constant arpeggio,
so the three lines did not read as one band. Trumpet Bb4 sounds -13.7 c and
C5 -8.7 c against trombone C5 -0.4 c.

New arrangement: plain melody (trumpet), clarinet a 3rd or 6th under it in
the same rhythm and tongue, trombone chord bass on beats 1 and 3 (F4 for F,
E4 for C7, F4 for Bb: its tuned range is E4..D5). Onsets after alignment to
the trumpet leader: medians trumpet 13.9, clarinet 7.7 (shift -6.1),
trombone 14.6 ms (shift +0.8), all within 10 ms. Passing tones Bb4 over F
(beats 2, 10, 18, 49) are the melody's own and are left.

Left: sourced asynchrony jitter, phase-correction clock, just-intonation
in held chords, a lower trombone range, balance from a source.
