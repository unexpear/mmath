# Roadmap — Beyond MVP

Status: planning · Nothing below may shape Phase 0–2 APIs

## Post-MVP sequence (each its own milestone + doc)

1. **Music math (orig. Phases 1–2 of first sketch):** `Pitch/Tuning/Scale/Chord/Melody/Rhythm/Progression/Transforms/Analysis`
   - Tuning interface `n → f`; 12/19/31-TET, Just, cents, custom maps.
   - `pitch(n) = 220·2^(n/19)` demo; `velocity(t)` dynamics shaping; fibonacci rhythm.
   - Chord = `{root, ratios[]}`; transforms pure + provenance-preserving; JSON round-trip.
2. **Signal depth:** harmonics, envelopes (ADSR + custom), wavetable, FM/PM correctness suite.
3. **Waves → pressure:** `y(x,t)`, media (`c=√(γRT)`), `p(x,t)`, solver ladder
   `Analytical | ReducedOrder | Modal | Waveguide | FDTD | FEM | Measured | Hybrid`.
4. **Objects → instruments:** String/Bar/Plate/Membrane/Tube/Cavity/Reed/Hammer/Bow;
   geometry ≠ material; coupling `H(f)` chains; excitation position → modal weights.
   Landed: `instruments/` with violin, viola, cello, and double bass ideal strings, a
   pluck, a bridge hook, a steady Helmholtz bow, one open-loop body mode, and a
   hard narrow hammer, and a quasi-static single reed (`docs/48-instruments-violin-viola.md`). A reed bore, felt hammers,
   measured bridge admittance, and radiated pressure are not built. This slice does not change Phase 0–2 APIs.
5. **Spatial + hearing:** `t=d/c`, Doppler from first principles, reverb/HRTF;
   loudness/pitch perception separate from physics.
6. **Composition graph:** `Scale→Chords→VoiceLead→Rhythm→Humanize→Instrument→Room→Out`;
   every node math-in/math-out, block-serializable.
7. **Electronics + DSP:** `V(t), I(t), Z(ω)`; pickups/circuits/amps; FIR/IIR/FFT-conv/
   granular/wavetable/additive/subtractive with declared latency/state/realtime-safety.
8. **Renderer + backends:** sampler, DSP graph, buffers, `AudioDevice/Stream/Clock`;
   WASAPI → CoreAudio/ALSA-PipeWire → JACK/ASIO; realtime (no alloc/lock in callback)
   vs offline-scientific (high-fs, FDTD/FEM) from same model.
9. **Analysis direction:** `audio → STFT → partials → pitch → notes → rhythm → chords → key/tuning`.
10. **Instrument simulator + blocks UI:** assembly editing, measured-data fusion
    `H_result(f) = H_physics(f)·H_measurement(f)`, visual blocks, MIDI/MPE out.

## Solver fidelity ladder (applies from step 3 on)

```text
Analytical → ReducedOrder → Modal → Waveguide → FDTD → FEM → Measured → Hybrid
```

Object stays constant; only solver swaps. Realtime uses cheap end, science uses dear end.

## Guardrails

- No step may introduce dependency from `physics/` into `music/`.
- No DSP/backend headers included by core.
- New math structures (matrices, FFT, ODE solvers, RNG) added on demonstrated need.
