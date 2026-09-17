# D16 — Sourced Material Data and a Validation Annex (complete)

Status: implemented · D13–D16 matrix: see the status update at the end of
this record · Probes: 10/10

Closes the step-3 item "measured material data", as far as open,
retrievable sources allow, and states the gap where they don't. It is
also the first **validation annex** (VALIDATION.md): the first comparison
of a model in this engine against **measurement** rather than against
another model.

## Sources (every number is cited; nothing is a default)

| Data | Source |
|---|---|
| Steel piano wire E = 2.0·10¹¹ Pa, ρ = 7850 kg/m³ | Chabassier, Chaigne & Joly, "Modeling and simulation of a grand piano", *J. Acoust. Soc. Am.* 134(1), 648–665 (2013), Table III |
| Five concert-grand strings (D♯1, C2, F3, C♯5, G6): L, d, winding factor F, T₀, f₀ as printed | same Table III (the paper's **model inputs** for a real instrument, not per-string inharmonicity measurements) |
| Q/ρ = 25.5·10¹⁰ (cm/s)² for steel wire; cents formula constant 3.4·10¹³ (cgs) | R. W. Young, "Inharmonicity of Plain Wire Piano Strings", *J. Acoust. Soc. Am.* 24(3), 267–273 (1952), DOI 10.1121/1.1906888. Abstract retrieved verbatim from the Crossref record |
| **Measured** trend: plain wire strings in situ in six pianos, "about 1.2 cents for the second mode of vibration of the middle C string. Above this point every eight semitones it is doubled. Below middle C the inharmonicity is consistently less in large pianos than in small ones." | Young 1952, abstract |
| Fletcher's steel constant 3.95·10¹⁰ (cgs) | Fletcher, *J. Acoust. Soc. Am.* 36(1) (1964), as quoted by E. Roy, *Edinburgh Student Journal of Science* (2024), DOI 10.2218/esjs.9815 |

## New code

| Header | Contents |
|---|---|
| `physics/materials.h` | `WireMaterial` + `steelPianoWireChabassier2013()`; the Young 1952 constants; `PianoStringSpec` + `chabassier2013Strings()` (all five columns as printed); `pianoString(spec, material)` → D10 hinged stiff string (μ = ρFπd²/4, κ = (d/4)√(E/(ρF)), the table's convention; hinged per the paper's Table II); `young1952Mode2Cents(semitones)`; `inharmonicityCents(system, n)` = 1200·log₂(fₙ / n·c/2L) |

## Validation annex: three kinds of evidence, kept apart

**A. Source consistency** (independent publications agree):

| Quantity | Comparison | Agreement |
|---|---|---|
| E/ρ (Chabassier) vs Q/ρ (Young) | 2.548·10⁷ vs 2.55·10⁷ m²/s² | 0.09% |
| Young's cents constant, re-derived as (1200/2 ln 2)·π²(E/ρ)/64 | 3.4010·10¹³ vs 3.4·10¹³ | 0.03% |
| Fletcher's constant, re-derived as π²(E/ρ)/64 | 3.929·10¹⁰ vs 3.95·10¹⁰ | 0.53% |

**B. Data consistency**: the published scale data reproduce the published
f₀ through D2's ideal-string law, c/2L with c = √(T/μ).

| Note | Computed f₀ | Published f₀ | Difference |
|---|---|---|---|
| D♯1 | 39.000 | 38.9 | +0.26% |
| C2 | 65.381 | 65.4 | −0.03% |
| F3 | 175.152 | 174.6 | +0.32% |
| C♯5 | 554.678 | 555.6 | −0.17% |
| G6 | 1566.993 | 1571.4 | −0.28% |

All within 0.32%, consistent with f₀ printed to 0.1 Hz and T₀ to 1 N.
Asserted at 0.5%.

**C. Model vs measurement**: mode-2 inharmonicity predicted by the D10
stiff-string model from the Chabassier data, against Young's **measured**
six-piano trend. Plain wire only, as Young measured:

| Note | Model (cents) | Young trend (cents) | Ratio |
|---|---|---|---|
| F3 (−7 semitones) | 0.576 | 0.654 | **0.880** |
| C♯5 (+13) | 3.314 | 3.701 | **0.895** |
| G6 (+31) | 14.649 | 17.606 | **0.832** |

- The modeled concert grand sits **12–17% below** the six-piano average
  trend at every note. That's consistent with Young's summary (a trend
  across pianos, "about").
- **Young's falsifiable directional statement holds**: below middle C, a
  large piano is less inharmonic than the trend (F3 ratio < 1).
- Asserted at ±25%, a trend-level tolerance chosen from Young's own
  wording. The measured ratios above are the claim; the tolerance only
  keeps the test from being tighter than the source.
- **Wound strings are not compared.** D♯1 comes out 3.5× Young's trend,
  but Young measured plain wire only, and the Chabassier winding
  convention (core stiffness, density ×F) is a modeling choice with no
  measured counterpart here.

## Also verified

The same C♯5 string gives D10 solver modes equal to D1's Fletcher
partials (1e-12). Young's own cgs cents formula on the same data agrees
with `inharmonicityCents` to 0.5% (the small-B approximation).

## What is not available, stated plainly

**Measured string damping.** Two open candidates were tried in this
milestone. The Acta Acustica 2020 plucked-string article couldn't be
fetched (the host returned a 777-byte block page). The arXiv Dulcken
harpsichord study (2025) reports measured *soundboard* damping, not
string decay. No open table of measured per-partial string decay times
was obtained. **Loss parameters therefore remain caller-supplied data**
(`lossFromT60`) and unvalidated, and every demo's T60 values stay labeled
illustrative.

Two further sources were tried and not used: the Edinburgh 2024 paper
reports its measurements only as a figure (no numeric table), and an HTML
copy of Young 1952 was unreachable (its host no longer resolves).

## Model status after D16

The stiff-string **inharmonicity** model has its first validation
evidence: trend-level agreement (within 17%) with in-situ measurement
from six pianos. Loss, end corrections and radiation remain unvalidated.
