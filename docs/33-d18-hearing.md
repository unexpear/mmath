# D18 — Hearing: Loudness, Perceptual Scales, Masking (complete)

Status: implemented · Suite 262/262 (10 new) on MSVC Release+Debug,
GCC-13 Release+Debug, Clang-18 Release+Debug, ASan+UBSan · Probes: 10/10

The first perception module: what the ear does to the engine's physical
quantities. A phon value is meaningless without frequency (60 dB SPL at
1 kHz is 60 phon; at 100 Hz roughly 48); this module converts both
directions, plus sones, Bark/ERB/A-weighting, and a simultaneous-masking
threshold. Pure tones only — complex-tone loudness (Zwicker/Stevens
integration over critical bands, ITU-R BS.1387) is deferred, stated below.

## Sources (every number is cited; nothing is a default)

| Data | Source |
|---|---|
| ISO 226:2023 formulation (Eq. 1/2 structure, αr = 0.3, Tr = 2.4 dB) and the 29-row αf/LU/Tf table | ISO 226:2023 itself is paywalled; transcribed from the open `hearingthreshold` implementation (MIT), which states its table is the 2023 revision and its formulas the standard's. Table verified row-by-row: **29/29 exact** against the transcription (audit script in `C:\Users\micha\AppData\Local\Temp\opencode\audit_iso226.sh`), plus structural checks: 1 kHz identity both directions to 1e-14, threshold minimum near 4 kHz, round-trips |
| Independent cross-check of the whole ISO computation | PyDSM/`jacobbaylesssmc` ISO 226:2003 lineage — different equations (A_f/B_f form), different project. Spot contours agree: 100 Hz/40 phon **64.24 vs 64.37 dB**, 4 kHz/40 phon 36.72 vs 36.65, 10 kHz/40 phon 54.38 vs 54.28, exact at 1 kHz both. Comparing the two tables, the 2023 revision's changes are confined mostly to αf |
| Bark scale, Schroeder form with Traunmüller low/high corrections | Traunmüller 1990, *JASA* 88(1) (abstract: "an excellent approximation … the expression 26.81f/(1960+f)−0.53 … slight adjustment"); identical mathematics to the frozen Essentia fit in `analysis/consonance.h`, which is untouched — this module owns the canonical definition going forward |
| ERB, Glasberg & Moore 1990 (24.7·(4.37f/1000+1)) | Moore & Glasberg 1983/1990 via the PMC notched-noise review (ERB_N equation, 1990 reference); explicitly NOT the classical critical bandwidth (~11% high, ~25 Hz floor per CCRMA) — the distinction is in the header |
| A-weighting analytical form (IEC 61672) | Stated as the standard's rational function (not a fit) in the `python-acoustics` docs; pinned against textbook anchors: −19.1 dB @ 100 Hz, exactly 0 @ 1 kHz, −2.5 dB @ 10 kHz |
| MPEG-1 spreading function (ISO/IEC 11172-3 psychoacoustic model) | Transcribed from the open `psychoacoustic-model` implementation (MIT); asymmetry confirmed in Schnupp lab notes (upward spread shallower, ~10 vs ~25 dB/Bark) and the masking-region description at CCRMA |

## New code

| Header | Contents |
|---|---|
| `hearing/loudness.h` | 29-row ISO 226:2023 table as `constexpr`; `splFromPhon` (forward formula), `phonFromSpl` (analytic inverse, DomainError below the 20-phon contour — no bisection, no iteration), `sonesFromPhon`/`phonFromSones` (Stevens doubling, total on finite inputs), `splDbFromPressure`/`pressurePaFromSplDb` (20 μPa). Validated scope enforced: f in [20, 12500] Hz, phon in [20, 90] (f ≤ 4 kHz) / [20, 80] (above); below-20-phon is informative-only per the standard, so refused |
| `hearing/scales.h` | `barkRate` (with both Traunmüller corrections), `erbBandwidth`, `erbNumber`, `aWeightingDb` (exact 0 at 1 kHz by construction). Plain-double returns: scale readings, not quantities crossing module boundaries |
| `hearing/masking.h` | `mpegSpreadingDb` (S(0) ≈ 0 by construction), `quietThresholdSpl` (direct T_f read), `maskingThresholdDb` (masker + spread, floored at quiet — a masker never makes silence less audible) |
| `units/physical_units.h` | `Phons`, `Sones` tags (D18 additions, same convention as the M6/D2 additions) |

## Deliberate non-goals and known divergences

- **A-weighting is NOT inverse-40-phon.** It descends from the old Fletcher–Munson 40-phon curve, not from ISO 226:2023; at 100 Hz they differ by ~5 dB. Observed, expected, not a bug — the two are pinned independently.
- **Interpolation is ours.** The module interpolates table parameters in log-frequency; the reference implementation fits a cubic spline through computed contour points. Both pass through all 29 nodes; the ≤0.2 dB spot differences above combine revision delta and interpolation choice.
- **Masking is simultaneous, single-masker, pure-tone.** No temporal masking, no informational masking, no power-sum additivity. The floor keeps every answer physical.
- **No complex-tone loudness.** Zwicker integration / ITU-R BS.1387 perceptual evaluation remain future work; the headers say so.

## Verification

- `tests/test_hearing.cpp` (10 cases): Bark anchors in all three regimes + monotonicity sweep, ERB/ERB-number, A-weighting textbook triple, spreading shape + asymmetry direction, 1 kHz identities, off-node contours with the 2003-lineage agreement pinned, all domain errors (out-of-band f, below-threshold SPL, NaN), sones doubling (40→1, 60→4, 70→8, 30→0.5), quiet-floor equality, masking self/floor/genuine cases.
- Header TUs `th_hearing_{loudness,scales,masking}.cpp`; `examples/hearing_demo` (60-phon demands 88.2 dB at 55 Hz, dip at 3.5 kHz; a 1 kHz/70 dB masker buries a 2 kHz probe far above quiet).
- Full matrix green with no tolerance relaxation: MSVC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, ASan+UBSan — 543016 assertions in 262 test cases everywhere.
