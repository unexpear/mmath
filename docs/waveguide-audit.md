# Waveguide and transfer-function audit (2026-10-14)

## 1. Fractional-delay tuning: where delay lengths are rounded

| Delay line | File | Fraction handled? | Pitch-relevant? |
|---|---|---|---|
| Cone/brass bore loop | cone_valve.h | Yes: first-order Thiran (`boreThiran_`) | Yes |
| Cylinder/reed bore loop | bore_valve.h | Yes: first-order Thiran | Yes |
| Plucked string | plucked_string.h | Yes: Thiran fine tune | Yes |
| Bowed string | bowed_string.h | Yes: Thiran (same as plucked) | Yes |
| Measured trumpet bore | brass_bore.h | Reflection function, no delay rounding | Yes |
| Tone-hole lattice (BoreLatticeVoice) | bore_network.h:469 | **No**: each segment `llround`ed on its own; error up to N/2 samples one way | **Yes** |
| Flute jet convection | bore_valve.h:623, bore_network.h:544 | No (`llround`) | Second order (jet phase) |
| Listener radiation delays | bowed_string.h:268, struck_string.h:434, cavity_drum.h:96, soundboard_voice.h:96 | No | No (latency only) |
| Vocal tract sections | player_airway.h:283-285 | Section counts rounded | Formants, not note pitch |

Work in progress: carried-remainder rounding plus a far-end first-order Thiran
(Thiran 1971; Laakso, Valimaki, Karjalainen, Laine, IEEE SPM 13(1), 1996) on the
lattice. It broke the render in "an open flute hole and a sax hole raise the bore"
(the render returns an error); the cause has not been found yet. Not committed.

## 2. Measured bell radiation and room transfer functions

- Bell: the measured trumpet bore already radiates through Levine-Schwinger
  (Silva et al. 2009 fit). A measured on-axis bell directivity/transfer table
  (e.g. Elliott, Bowsher & Watkinson, JASA 72, 1982) still needs to be located
  and digitised from the paper; none is in the repo.
- Room: no published measured room impulse response is in the repo. Openly
  licensed measured RIR sets exist (e.g. OpenAIR, MIT IR survey); licence and
  provenance must be checked before any are used. Not started.

## 3. Commuted-synthesis bodies (violin, guitar)

Missing: a published, measured body impulse or bridge-admittance response with
usable numeric data. Candidates to obtain: Jansson violin bridge mobility
measurements; Woodhouse "The acoustics of the violin: a review" (Rep. Prog.
Phys. 77, 2014) modal tables; Elejabarrieta/Ezcurra guitar modal data. Until
numeric data is in hand, no body filter is built.

## Known gap: modal violin body (2026-10-14)

- **Missing:** per-mode effective (modal) mass at the bridge for A0, CBR,
  B1- and B1+, and CBR damping. Frequencies (Bissinger 2008, Saitis 2012)
  and damping for A0, A1, B1- and B1+ (Bissinger 2012: 2.5, 1.0, 1.22 and
  1.15 %crit) are sourced. See docs/published-body-bell-data.md.
- **Why the digitized fit failed (17fa541):** the only open calibrated
  plot (Saitis 2012 Fig. 2) overlays 10 violins.
  - The 10.5 dB spread between curves leaves the masses uncertain to a
    factor of about 2-3.
  - The median curve merges B1- and B1+ (the fitted B1+ hit its 505 Hz
    bound, against 541 Hz published).
  - CBR's Q fell to 5, which is unphysical.
  - The dB reference is not printed.
- **What closes it:** one calibrated single-violin bridge admittance (a
  published single-instrument figure, or raw data from McGill CAML,
  Saitis/Scavone or Maestre/Scavone), or Gough JASA 137 (2015) if it
  tabulates bridge masses. Until then the violin keeps its current body,
  and the guitar is the only instrument with a measured modal body.
