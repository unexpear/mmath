# Published body and bell data (research notes; nothing built from this yet)

Collected 2026-10-14. Numbers below are copied from the cited sources;
anything not stated by a source is not given.

## Violin signature modes (frequencies only; no damping or mass tables found)

Saitis et al., "Bridge admittance measurements of 10 preference-rated violins",
Acoustics 2012 Nantes (https://caml.music.mcgill.ca/lib/exe/fetch.php?media=publications%3Anantes2012_saitis.pdf).
Mean / SD / range over 10 violins:

| Mode | Mean Hz | SD Hz | Range Hz |
|------|--------:|------:|---------:|
| A0   | 271 | 6  | 262-281 |
| CBR  | 396 | 16 | 368-412 |
| A1   | 460 | 17 | 439-496 |
| B1-  | 477 | 15 | 452-502 |
| B1+  | 522 | 16 | 501-557 |

The paper also reports a mean A1/A0 ratio of 1.64 (SD 0.06).

Single instrument (Stoppani measurement, shown in Woodhouse, Euphonics section 5.3,
https://euphonics.org/5-3-signature-modes-and-formants/): A0 272, CBR 407,
B1- 462, B1+ 551 Hz. That page also shows a bridge hill peaking near 2.3 kHz.

Usual ranges (Sci. Rep. 2024, s41598-024-79497-7, Table 1, after Hutchins &
Voskuil 1993 and Gough SMAC 2013): A0 260-290, CBR 390-430, B1- 430-480,
A1 430-490, B1+ 510-580 Hz.

## Guitar low-frequency modes

Christensen & Vistisen, "Simple model for low-frequency guitar function",
JASA 68(3), 758-766 (1980): the first resonance is typically at 90-120 Hz and the
second at 170-250 Hz (both near 100 and 200 Hz). The second top-plate mode is
usually at 240-300 Hz. Model parameters are derived from f1, f2 and the cavity
Helmholtz anti-resonance. The full per-instrument parameter table is behind the
JASA paywall and was not read.

Not found: Jansson's tables, Elejabarrieta's tables, or Woodhouse's 2014
modal table with damping and effective mass. Commuted body synthesis still
needs damping (Q) and amplitude per mode.

## Trumpet bell radiation (Elliott, Bowsher & Watkinson)

Elliott, Bowsher & Watkinson, "Input and transfer response of brass wind
instruments", JASA 72(6), 1747-1760 (1982), doi:10.1121/1.388648. The paper
measures input impedance and the pressure transfer function T = p_out/p_in.
According to a Surrey PhD thesis (author not checked) (ibew.org.uk DV02966), it models the bell
with the radiation impedance of a pulsating hemisphere (Elliott 1979). The
abstract says bell radiation impedance data were insufficient. No numerical
transfer-function table is available without the paywalled paper, so the
bell and room transfer functions remain an open data gap.

## Addendum 2026-10-14: damping, effective mass, bell numbers

FOUND means the values were read in a source excerpt during this search.
VERIFY means they should be checked against the full PDF before use.

### Guitar body: modal table with damping and effective mass (FOUND)

Debut & Antunes, "Physical synthesis of six-string guitar plucks using the
Udwadia-Kalaba modal formulation", J. Acoust. Soc. Am. (2020). Open preprint:
https://hal.science/hal-03604430v1/file/UK_JASA_Preprint.pdf, Table I. The
modes were identified from input admittance measured at the bridge. The
table uses bridge-point modal masses m_n and the admittance formula
Y(w) = sum_n i w / (m_n (w_n^2 - w^2 + 2 i w w_n zeta_n)).

| n | f_n Hz | zeta_n % | m_n kg |
|---|------:|------:|------:|
| 1 | 89.4 | 1.5 | 0.40 |
| 2 | 182.6 | 1.82 | 0.08 |
| 3 | 229.9 | 0.64 | 0.20 |
| 4 | 310.2 | 0.89 | 0.47 |
| 5 | 403.5 | 0.55 | 1.06 |
| 6 | 511.4 | 0.75 | 0.58 |
| 7 | 544.2 | 0.53 | 1.82 |
| 8 | 613.9 | 0.61 | 0.69 |
| 9 | 669.8 | 0.69 | 0.38 |
| 10 | 848.4 | 0.59 | 0.81 |

A second guitar soundboard table appears in "Active control and sound
synthesis - two different ways to investigate the influence of the modal
parameters of a guitar on its sound", J. Acoust. Soc. Am. 139(3), 1411 (2016),
Table V (authors VERIFY, likely Benacchio et al.). It gives f_k Hz and damping
factor xi_k: 87/0.007, 113/0.029, 213/0.025, 232/0.009, 271/0.007,
378/0.008, 409/0.019, 418/0.014, 504/0.011. The table also has modal-mass
columns that were only partly legible in the excerpt (VERIFY). Source:
https://pubs.aip.org/asa/jasa/article-pdf/139/3/1411/19746380/1411_1_online.pdf

There is also an open dataset of bridge mobility for 65 Spanish guitars
(Mores, Zenodo). Its record was not opened (VERIFY URL).

### Violin damping (partial)

- A0 total damping is 2.56 +/- 0.2 % of critical (Q about 20), and does not
  depend on quality. Source: Bissinger, "The role of radiation damping in
  violin sound", ARLO 5(3), 82 (2004),
  https://pubs.aip.org/asa/arlo/article-pdf/5/3/82/8983982/82_1_online.pdf (FOUND).
- A1 damping is 1 % of critical, and A0 is modelled at 2.5 % (Q 20).
  Source: Bissinger, JASA 2012 dynamic filter model,
  https://strad3d.org/files/bissinger/JASA%202012%20dynamic%20filter%20model.pdf (FOUND).
- Signature-mode averages over 17 violins: A1 469 +/- 19, CBR 407 +/- 31,
  B1- 475 +/- 16, B1+ 541 +/- 22 Hz, with A1/A0 = 1.71 +/- 0.05.
  Source: Bissinger, "Structural acoustics of good and bad violins", JASA 124
  (2008), https://strad3d.org/files/bissinger/JASA%202008%20Struct%20Acoust%20good%20bad%20violins.pdf (FOUND).
- Per-mode damping and effective mass for CBR, B1- and B1+: NOT FOUND.
  The 2008 paper's tables probably contain them (VERIFY).

### Trumpet bell

- Kausel, Mayer & Beauchamp, "Brass instruments as a cascade of two-port
  networks: transfer functions, chain parameters, and power efficiency in
  theory and practice", JASA 149(4), 2698 (2021), doi:10.1121/10.0004303.
  The sample B-flat trumpet has a 12.3 cm bell diameter, a cutoff of
  1630 Hz, and a rim with tan(theta) = 3.23 (FOUND). The cutoff formula is
  f_cutoff > 1.84 c / (2 pi b). The paper compares four radiation models;
  Helie-Rodet follows the expected fall above cutoff, and Elliott's model
  reflects too much above cutoff. Its figures show measured transfer
  functions, but no numeric table was seen (VERIFY whether data is supplied).
- Kemp, Edinburgh PhD thesis 2002 (http://kempacoustics.com/thesis/node65.html
  and https://era.ed.ac.uk/...65f76e15...): measured input impedance of a
  trumpet bell section, both baffled and multimodal. Values appear only as
  plots (the cutoff numbers did not render in HTML; VERIFY).

### Is this enough?

- Guitar body: yes for a first commuted or modal body. The data covers 10
  modes with f, zeta and bridge mass (Debut & Antunes), which is enough for
  bridge admittance. Radiation from each mode to the far field is still not
  sourced.
- Violin body: no. There are frequencies for all signature modes, but
  damping only for A0 and A1, and no effective masses.
- Trumpet bell: no numeric measured transfer function. The cutoff (1630 Hz)
  and model choice (Helie-Rodet) are sourced, so a model-based bell filter
  could be built, but not a measured one.

## Addendum 2026-10-14 (2): violin per-mode damping and effective mass

### Damping (FOUND)

Bissinger, "Parametric plate-bridge dynamic filter model of violin
radiativity", J. Acoust. Soc. Am. 132(1), 465-476 (2012) (title VERIFY),
open PDF: https://strad3d.org/files/bissinger/JASA%202012%20dynamic%20filter%20model.pdf.

14-violin averages used in all of its simulations:

| Mode | f Hz | zeta %crit | Q = 1/(2 zeta) | Radiativity <R> Pa/N |
|------|-----:|-----:|-----:|-----:|
| A0  | 275 | 2.5  | 20 | - |
| A1  | 469 | 1.0  | 50 | - |
| B1- | 470 | 1.22 | 41 | 1.01 |
| B1+ | 541 | 1.15 | 43 | 0.95 |

The same paper's Strad-copy violin has B1- at 489 Hz with 1.2 %crit and
B1+ at 515 Hz with 1.1 %crit. Its measured radiativity is R_B1- = 0.72 and
R_B1+ = 0.84 Pa/N, its A0 radiativity is 0.29 Pa/N, and the violin mass is
M = 0.41 kg (FOUND).

Other damping values:
- A0: 2.56 +/- 0.2 %crit (Bissinger, ARLO 2004; FOUND, listed above).
- CBR: no per-mode value found (NOT FOUND).
- Total damping falls as zeta_tot = C f^x with x = -0.40 (Plowden-Titian)
  and -0.45 +/- 0.05 (bad violins). Source: Bissinger, JASA 124 (2008),
  https://strad3d.org/files/bissinger/JASA%202008%20Struct%20Acoust%20good%20bad%20violins.pdf (FOUND).
  The 2012 paper fits zeta_tot proportional to f^-0.34 (FOUND). Neither
  paper gives a per-mode table.
- Gough, "Violin acoustics", Acoustics Today 12(2), 2016,
  https://acousticstoday.org/wp-content/uploads/2016/06/Gough.pdf, gives
  typical body-mode Q of 25-50 (FOUND, generic, not per mode).

### Effective or modal mass at the bridge (NOT FOUND)

- No open source found gives bridge effective masses for A0, CBR, B1- or B1+.
- Bissinger reports radiativity (Pa/N) and total violin mass, not modal
  mass at the bridge.
- Gough says effective masses come from measured mobility. Gough, "A violin
  shell model", JASA 137, 1210-1225 (2015) computes effective mass at the
  bridge per mode from finite-element results. That paper is paywalled and
  was not read (VERIFY: it may tabulate them).
- Woodhouse's Euphonics 7.5 gives only a bridge-rocking effective mass
  (about 0.5-0.6 g), which is not a body mode.
- Malvermi et al., "A statistical approach to violin evaluation", Appl. Sci.
  12, 7313 (2022), fitted f, amplitude and Q for A0, CBR, B1- and B1+ on 25
  violins. The open PDF prints only correlations and PCA loadings, not the
  values (VERIFY: supplementary data).
- Saitis 2012 (Acoustics 2012 Nantes) gives frequencies only.

### Is this enough for a modal violin body like the guitar's?

No. Frequencies are FOUND for all five signature modes, and damping is
FOUND for A0, A1, B1- and B1+. CBR damping is missing, and so is every
bridge effective mass, which the modal admittance needs. The radiativities
(Pa/N) could set relative radiation weights, but they cannot replace
masses in the bridge admittance. The next step is to read Gough JASA 2015
(shell model) and Zhang & Woodhouse 2014 for tables, or to get the
Strad3D/VIOCADEAS mobility data and fit masses ourselves (fitting is not
"published numbers").

## Addendum 2026-10-14 (3): search for violin bridge effective masses

### (a) An open Gough copy: NOT FOUND

- Gough, "A violin shell model: Vibrational modes and acoustics", JASA 137,
  1210-1225 (2015), doi:10.1121/1.4913458. Only the paywalled AIP copy
  exists. There is no author preprint on Semantic Scholar or AIP. The
  abstract does not report per-mode bridge masses.
- Related conference version: Gough, "A generic model for instruments of
  the violin family", Proc. Inst. Acoust. 35(1) (2013). No open copy was
  found.
- Gough, "The violin bridge-island input filter", JASA 144(1) (2018). Its
  abstract describes a quasi-statistical filter, not a per-mode mass
  table. Paywalled.
- Gough, "Violin acoustics", Acoustics Today (2016), is open and says
  effective masses come from measured mobility, but it lists none.

### (b) Open measured bridge admittance data: NOT FOUND

- Maestre, Scavone & Smith, SMAC 2013, http://mtg.upf.edu/system/files/publications/smac_adm_2013_v3.pdf.
  Its supplementary site https://ccrma.stanford.edu/~esteban/adm/smac13/
  hosts only three synthesized WAV examples (violin_a, viola_a,
  cello_a.wav). It has no admittance data or modal parameter files, and
  the /adm/ directory returns 403.
- Strad3D, "Get Files" (https://www.strad3d.org/spectra.html): stereo WAVs
  of a microphone (left) and hammer (right) for the Plowden, Titian and
  Willemotte violins, described as "uncalibrated". These are radiativity
  (pressure/force), not bridge velocity/force, so they cannot give modal
  masses at the bridge.
- Zenodo: the open hammer datasets are for cello bridges
  (zenodo.org/records/20797149) and cello bridge force
  (zenodo.org/records/10619541). There is no violin admittance.
- Saitis 2012, Elie et al. SMAC 2013 (members.loria.fr/BElie/papers/elieetal_SMAC2013.pdf)
  and Malvermi et al. 2022 publish plots or statistics only.

Result: no FITTED masses. Fitting would need calibrated bridge
velocity/force data. Digitizing published admittance plots is possible
but was not done; it would be a lower-grade source and needs Mike's
approval.
