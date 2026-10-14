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
