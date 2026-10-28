# Sources and credits (music and recordings)

No audio recordings are stored in this repository. Reference recordings were
downloaded locally for measurement only. Measured numbers are facts with the
source cited next to them in the code.

## Scores / note data
| File(s) | Work | Source | Licence of this data |
|---|---|---|---|
| `tools/demo/christmas/jingle_bells_melody.txt`, `tools/demo/band/jb_*.csv` | J. Pierpont, Jingle Bells / "The One Horse Open Sleigh" (1857) | Project Gutenberg #10535 (verse); Wikimedia Commons `File:Jingle_Bells_refrain_vector.svg` (refrain) | Public domain |
| `tools/demo/christmas/silent_night_*.txt`, `tools/demo/band/sn_*` | F. X. Gruber, Stille Nacht (1818) | Mutopia GruberFX/stille_nacht (Mutopia-2008/02/19-1295) | Public domain |
| `tools/demo/band/pat_*` | La Monnoye, Patapan (1720), harm. Charles Wood | hymnsandcarolsofchristmas.com MIDI (not committed) | Public domain |
| `tools/demo/caprice24/caprice24v.csv` | N. Paganini, Caprice Op.1 No.24 | Mutopia Project, Caprice_24 (Ricordi 1836 source), 2015 | **CC BY-SA 4.0** — https://www.mutopiaproject.org/ftp/PaganiniN/O1/Caprice_24/ |
| `tools/demo/guitar_pro1/guitar72.csv`, `tools/demo/guitar_pro2/guitar72dyn.csv` | Anon., Romance (Spanish Romance) | Mutopia Project 2006 | **CC BY-SA 2.5** — https://www.mutopiaproject.org/ftp/Anonymous/spanish-romance/ |
| `tests/wind_excerpts.h` (Mozart K.622, Mussorgsky/Ravel Pictures, Debussy Faune, Ravel Boléro, Rimsky-Korsakov Bumblebee, traditional tunes) | — | written from public-domain works; no edition copied | Public domain (Boléro & Ravel's Pictures orchestration: US PD since 2025, France since 2016) |
| `tests/wind_excerpts.h` `kYankeeDoodle` | Yankee Doodle (traditional, 18th c.) | traditional tune, written out by hand | Public domain |
| `tests/wind_excerpts.h` `kGreensleeves` | Greensleeves (traditional English; registered 1580, printed 1584) | traditional tune, written out by hand | Public domain |
| `tests/wind_excerpts.h` `kProClarinet`, `kProFlute` | Serolillo, "Jazz Clarinet.ogg", "Jazz Flute Cm7.ogg" (Wikimedia Commons: https://commons.wikimedia.org/wiki/File:Jazz_Clarinet.ogg , https://commons.wikimedia.org/wiki/File:Jazz_Flute_Cm7.ogg) | transcribed with pYIN | CC BY 2.5 (attribution) |
| `tests/wind_excerpts.h` `kProSaxophone`, `kProTrumpet` | ChickSR, "Lick Alto Sax.wav", "Lick Trumpet.wav" (Wikimedia Commons: https://commons.wikimedia.org/wiki/File:Lick_Alto_Sax.wav , https://commons.wikimedia.org/wiki/File:Lick_Trumpet.wav) | transcribed with pYIN | **CC BY-SA 4.0** |
| `tests/wind_excerpts.h` `kProTrombone` | Mathieu Prevot, "Sheherazade opening, trombone bass.wav" (Wikimedia Commons: https://commons.wikimedia.org/wiki/File:Sheherazade_opening,_trombone_bass.wav) | transcribed with pYIN | **CC BY-SA 4.0** |
| `tools/demo/christmas/jingle_sleigh_timeline.txt`, `tools/demo/sleigh_bells/ref_timeline.txt` | sleigh-bell pattern | The Midnite Wolf, "Sleigh bells.wav" (Wikimedia Commons) | CC0 |

## Reference recordings used for measurement (not committed)
| Instrument | Recording | Licence |
|---|---|---|
| Sleigh bells | The Midnite Wolf, Sleigh_bells.wav, Wikimedia Commons | CC0 |
| Crash cymbal | Virtuosity Drums crash (vl1–vl3) | CC0 |
| Flute, snare, glockenspiel, tubular bells, strings | VSCO 2 CE, Sam Gossner (github.com/sgossner/VSCO-2-CE) | CC0 |
| Handbells | Freesound 557142 (F3); radwoc handbells | CC0 |
| Celesta | Freesound Macsat-Rd celesta pack | CC0 |
| Classical guitar | Jim Greeninger, "Romance Anónimo (Jeux interdits).ogg", Wikimedia Commons | CC0 |
| Cello | Bach Sarabande BWV 1007/IV, Wikimedia Commons | Public domain |
| Winds | Serolillo (CC BY 2.5); ChickSR, Mathieu Prevot (CC BY-SA 4.0), Wikimedia Commons | as listed |
| Woodwinds, bass, glock (2nd ref) | University of Iowa Musical Instrument Samples | free to use without restriction |
| Caprice tempo | Steinaco (PD), Yonemoto (CC BY 2.0), Wikimedia Commons | as listed |
| Violin (history charts, `tools/demo/history/violin-history41..45.png`) | Nikolay Madoyan, live Caprice 24 (opening), "Paganini 24 caprices played live in one concert without interval Nikolay Madoyan.webm", Wikimedia Commons, own work — https://commons.wikimedia.org/wiki/File:Paganini_24_caprices_played_live_in_one_concert_without_interval_Nikolay_Madoyan.webm (local excerpt `vt6/real.wav`) | CC BY-SA 4.0 (the five charts are CC BY-SA 4.0) |
| Rooms | OpenAIR impulse responses (Genesis 6 studio) | see openair.hosted.york.ac.uk per IR |

Notices also sit next to the files: `tools/demo/caprice24/README.md`, `tools/demo/guitar_pro1/README.md`, `tools/demo/guitar_pro2/README.md`, `tools/demo/winds_pro1/README.md`, `tools/demo/history/README.md`, and the header comment above the kPro* arrays in `tests/wind_excerpts.h`.

Two copyrighted pop melodies that used to be in `tests/wind_excerpts.h` were removed in favour of the public-domain Yankee Doodle and Greensleeves.

Images under `tools/demo/**` that show spectrograms or waveforms of the
CC BY-SA recordings above are licensed CC BY-SA 4.0.
