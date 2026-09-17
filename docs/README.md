# Docs Index — Music Mathematical Engine Planning

MVP: Phases 0–2 + 2.5 · C++20 · zero-dep core · strict units

| # | Doc | Contents |
|---|---|---|
| 00 | `00-vision.md` | Math-first vision, forward/reverse chains, axiom, key examples |
| 01 | `01-architecture.md` | Layered dependencies, module map, MVP slice, invariants |
| 02 | `02-mvp.md` | Phase 0/1/2/2.5 scope, directories, exit criteria, out-of-scope |
| 03 | `03-units.md` | Strict dimensions + semantic strong types, conversions, sketch |
| 04 | `04-signal-time.md` | Continuous time, Function1D, Signal graph, phase accumulation |
| 05 | `05-sinks-wav.md` | Null → WAV → native order, IAudioSink, golden WAV format |
| 06 | `06-testing-certification.md` | Identity tests, FM discriminator, golden a4.wav checks |
| 07 | `07-roadmap.md` | Post-MVP sequence, solver ladder, guardrails |
| 08 | `08-adr.md` | ADR-001…018 locked decisions (incl. tightening amendments) |
| 09 | `09-m1-tuning.md` | M1 milestone record: tuning catalogue, anchors, audible proof |
| 10 | `10-m2-structures.md` | M2 milestone record: pitch, structures, transforms, audible proof |
| 11 | `11-m3-timbre.md` | M3 milestone record: additive harmonics, ADSR, audible proof |
| 12 | `12-m4-time.md` | M4 milestone record: tempo curves, meter, audible proof |
| 13 | `13-m5-ci.md` | M5 record: CI matrix, sanitizers, move-order bug GCC caught |
| 14 | `14-m6-waves.md` | M6 record: wave kinematics, air medium, analytic proof |
| 15 | `15-math-physics-audit.md` | Full math/physics audit: verdict, findings, dispositions |
| 16 | `16-d1-inharmonicity.md` | D1 record: stiff spectra, consonance model, stretch mechanism |
| 17 | `17-d2-waves-deep.md` | D2 record: modes, impedance, ISO absorption, two caught traps |
| 18 | `18-d4-set-theory.md` | D4 record: Forte catalogue, voice-leading, Euclidean rhythm |
| 19 | `19-d3-commas.md` | D3 record: exact commas, EDO error tables, MOS |
| 20 | `20-d5-dsp.md` | D5 record: FM, PolyBLEP, integrator, spectra, biquad |
| 21 | `21-d6-wavetable.md` | D6 record: wavetable tables, interpolation bounds, oscillator |
| 22 | `22-d7-pm-envelopes.md` | D7 record: phase modulation, FM≡PM identity, segment envelopes |
| 23 | `23-d8-wave-solvers.md` | D8 record: 1-D wave object on analytical, modal, FDTD, waveguide solvers |
| 24 | `24-d9-loss.md` | D9 record: frequency-dependent loss, T60 fit, implicit lossy FDTD |
| 25 | `25-d10-stiffness.md` | D10 record: hinged stiffness, D1 Fletcher agreement, stiff FDTD stability |
| 26 | `26-d11-fft.md` | D11 record: FFT, windows, derivative-root sinusoid estimator |
| 27 | `27-d12-pitch-error.md` | D12 record: exact FDTD dispersion, FFT-measured pitch/decay error, D9 sign fix |
| 28 | `28-d13-stiff-ends.md` | D13 record: clamped/free stiff ends, boundary determinant, beam roots, ghost-row FDTD |
| 29 | `29-d14-fine-tuning.md` | D14 record: FDTD pre-warping, Thiran fractional delay, exact loop poles |
| 30 | `30-d15-waveguide-loss-dispersion.md` | D15 record: tuned waveguide loss and dispersion filters, per-partial error report |
| 31 | `31-d16-measured-data.md` | D16 record: sourced piano-wire data, validation annex vs Young 1952 measurement |
| — | `FOUNDATION.md` | **Canonical** base decisions (accepted/proposed/deferred) |
| — | `NUMERICS.md` | **Canonical** precision, determinism tiers, tolerances |
| — | `VALIDATION.md` | **Canonical** verification vs validation, evidence rules |

Start here: `00-vision.md` → `02-mvp.md` → `FOUNDATION.md` → `08-adr.md`.
Build order: Phase 0 → 1 → 2 → 2.5. Nothing beyond MVP shapes MVP APIs
except the explicitly reserved contracts in FOUNDATION (processing boundary,
frame rules, sink interface).
