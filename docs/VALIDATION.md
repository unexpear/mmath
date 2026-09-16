# VALIDATION — Verification vs Validation, Evidence Rules

Status: canonical · Labels: **[ACCEPTED] / [PROPOSED] / [DEFERRED]**

## Distinction

- **[ACCEPTED]** **Verification**: does the code correctly solve the chosen model?
  (MVP sine + sweep + WAV pipeline — fully verifiable against closed forms.)
- **[ACCEPTED]** **Validation**: does the model agree with physical observation for
  its intended use? (Guitars, rooms, circuits — requires measurements; MVP validates nothing physical.)
- **[ACCEPTED]** First release claims verification only. Any later physical model
  ships with: assumptions, supported conditions, input-data sources, solver method,
  and validation evidence — or it ships marked unvalidated.

## Evidence rules for later models

- **[ACCEPTED]** Unknown properties stay unknown: no silent zero, convenient default,
  or passing test in place of data. Missing inputs are errors or explicitly-marked estimates.
- **[ACCEPTED]** Method names (`FEM`, `FDTD`, `modal`, `analytical`, `waveguide`,
  `measured`, `hybrid`) are methods/representations, **not** accuracy ranks.
  Credibility = model + numerical error + input uncertainty + comparison with reality.
- **[ACCEPTED]** Solver↔object compatibility is explicit per model. No promise that every
  object runs on every solver unchanged. Coupled systems may solve internally rather
  than decompose into a signal chain — sketches showing chains are examples, not constraints.
- **[PROPOSED]** Each physical model gets a validation annex: calibration data refs,
  error metrics, applicable ranges, known-failure modes.
- **[DEFERRED]** Full measurement corpus (IRs, modal scans, HRTFs, material tables),
  room-solver validation suite, perceptual listening protocols.

## MVP verification checklist (maps to 06-testing)

- **[ACCEPTED]** Units/identities, constant + sweep analytical agreement, FM discriminator,
  composition identity, reset semantics, block-size invariance (64/127/512 + partial),
  exact frame counts + endpoint, float-stats pre-quantization, known-value PCM fixtures,
  independent decode, write-failure injection, max-length render, recorded toolchain/FP flags.
