# D19–D21 — Spectrum Analyzer, SVG Visualizer, Equalizers (complete)

Status: 368/368 test cases (640,468 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports) · Probes: 10/10

Three features the library didn't have: a spectrum analyzer (D19), a
dependency-free visualizer (D20) and equalizers (D21). They sit on top of
the D11 FFT and the D5 biquad, and `examples/spectrum_demo.cpp` shows all
three on one plucked string.

## New code

| Header | Contents |
|---|---|
| `analysis/spectrum.h` | `Spectrum{binHz, amplitude, power}`; `computeSpectrum` (power-of-two N ≥ 16); `amplitudeDb`; `FrequencyBand`, `fractionalOctaveBands(b = 1 or 3, fMin, fMax)`; `BandLevel`, `bandLevels`; `Spectrogram`, `computeSpectrogram` (STFT) |
| `plot/svg.h` | `PlotStyle`; `svgBandBars`, `svgSpectrum`, `svgSpectrogram`; `heatColorRgb` (the shared colour ramp); `writeTextFile` |
| `dsp/biquad.h` (extended) | RBJ `makePeaking`, `makeLowShelf`, `makeHighShelf`; `Biquad::setCoeffs` (retune, state kept) |
| `dsp/equalizer.h` | `EqShape`, `EqBand`, `designEqSection`; `ParametricEq` (1–64 bands, `processBlock`, `responseDb`, allocation-free `setBand`); `bandEdgeQ`; `makeGraphicEq`; `graphicEqCentreErrorDb` |
| `examples/spectrum_demo.cpp` | plucked stiff string → octave "smile" EQ → band bars before/after, spectrum, spectrogram and EQ-response SVGs, plus `eq.wav` |

## D19 — spectrum analysis

- **Scaling.** Amplitude A_k = 2|X_k| / Σw, so a bin-centred sine reads its
  exact amplitude. Power P_k = 2|X_k|² / (N·Σw²) is one-sided mean square,
  so Parseval holds to roundoff for every window. Silence reports a stated
  floor of −300 dB, not −∞.
- **Bands.** IEC 61260-1:2014 base-ten system: G = 10^0.3, exact mid-band
  f_m = 1000·G^(x/b), edges f_m·G^(±1/2b). A band's level sums the power of
  the bins whose centres lie in [lower, upper). A band too narrow to hold a
  bin at the chosen FFT size reports `bins = 0`. It is shown as "no data",
  never as silence.
- **Spectrogram.** Frames of N every hop samples, amplitude in dBFS per bin.

Evidence (`[d19spec]`, 6 cases): exact bin-centred amplitude; Parseval for
every window; band geometry (1 kHz reference, geometric centres, exact
tiling); a tone's power lands in its own band; empty bands at coarse
resolution are reported; a known sweep is tracked frame by frame.

## D20 — SVG visualization

- Deterministic text output (`snprintf` in the "C" locale): the same data
  gives byte-identical files on every platform.
- Plot frame x0 = 56, y0 = 34, w = width − 72, h = height − 78. A bar's
  height is h·clamp((L − dbMin)/(dbMax − dbMin)).
- The spectrum line and the spectrogram both **max-pool** into pixels, so
  a narrow peak is never averaged away.
- Five-stop colour ramp, from #0d083c to #fcf0aa. It is exposed as
  `heatColorRgb`, so the desktop app draws identical colours.

Evidence (`[d20plot]`, 7 cases): exact bar heights at the top, middle and
below the range; hatched no-data bands; determinism and XML escaping; the
log axis maps fMin and fMax to the frame edges; max pooling and the ramp's
end stops; refusals; `writeTextFile` round-trips exact bytes.

## D21 — equalizers

- **Sections** (RBJ Audio EQ Cookbook): peaking |H(f0)| = gain exactly,
  unity at DC and Nyquist; shelves reach their gain at their own end. A
  0 dB section is an exact identity; ±g sections cancel exactly.
- **Parametric EQ**: a series cascade. `responseDb` is the exact sum of
  the section magnitudes. `setBand` designs the new section,
  stability-checks it, then swaps the coefficients **without allocating
  and without resetting the filter state**, so it is safe on an audio
  thread. D23 extends the shapes to low/high-pass, band-pass, notch and
  all-pass.
- **Graphic EQ**: one peaking section per IEC band, with the band-edge
  Q = √r/(r − 1), r = upper/lower. The neighbours' skirts add up at each
  centre: with octave bands all at +6 dB, each interior centre gets
  1.6–2.8 dB extra. D21 **reports** this through `graphicEqCentreErrorDb`;
  D25 later **solves** it.

Evidence (`[d21eq]`, 6 cases): exact peaking and shelf anchors; rendered
sines come out at the level `responseDb` predicts (measured with the D11
estimator); flat gains pass through to roundoff (measured 2.4e-13); a
single band hits its gain exactly; the interaction is stated and bounded;
`setBand` gives the same response as a freshly built EQ, splitting a run
around a no-op retune is bit-identical, and a failed `setBand` leaves the
EQ unchanged.

## Limits

- Band levels come from one FFT resolution; low third-octave bands need a
  long frame (8192 samples at 48 kHz still leaves 20–25 Hz bands with 0–1
  bins, reported as such).
- The graphic EQ's centre gains aren't exact until D25's solver is used.
