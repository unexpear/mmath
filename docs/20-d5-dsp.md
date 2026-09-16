# D5 — DSP Depth: FM, Bandlimited Oscillators, Integrator, Spectra, Biquad (complete)

Status: implemented · 159/159 on MSVC Release+Debug, GCC-13 Release,
Clang-18 Release, GCC ASan+UBSan (zero reports) · Probes: 10/10

Depth milestone (research-grade) for the signal path, including the first
stateful processor under the reserved contract. Two deviations from the
locked plan, both documented with reasons below (PTR, sampled rendering).

## New code

| Header | Contents |
|---|---|
| `signal/fm.h` | `fmTone` (phase-exact adapter over `sineFM`, deviation = I·fm); `sidebandTable` (fc±k·fm, signed Bessel amplitudes, k = 0..ceil(I)+2, negative frequencies listed raw); `carsonBandwidth` (rule of thumb, labeled) |
| `signal/bandlimited.h` | 2-point PolyBLEP residual (Tale, verified verbatim) + saw/square as PURE functions of integer frame index (see position note); naive references; `framePhase` (Nyquist enforced); explicit `std::numbers::pi` (no magic literals) |
| `signal/phase.h` + | `integratePhaseFrequency` (adaptive Simpson, tol-guaranteed, max depth 20, closed forms stay the hot path) |
| `signal/spectral.h` | `timeVaryingAdditive` (per-partial envelopes; attack transients differ per partial) |
| `dsp/biquad.h` | First stateful processor: RBJ lowpass/highpass (a0-normalized), DF1 with owned state, init/reset, caller buffers, latency declared 0; `transferMagnitude`; pole-stability predicate |

## Position notes (deviations, all deliberate)

- **PTR deferred**: Faust's PTR is feedback-stateful; a pure-function port
  would be our own unverified derivation. PolyBLEP (exact residual in hand)
  covers the bandlimited need statelessly.
- **Sampled sources are frame-indexed**: PolyBLEP correction spans samples,
  so it cannot live in continuous-time `Signal`s honestly. The functions
  are pure over `(n, fs)` with the same no-hidden-state contract; the demo
  renders through an explicit per-n loop — the preview of sampled-source
  rendering, kept out of the continuous renderer by design.
- **Quantitative alias proof awaits FFT** (stated, not claimed): verified
  instead are naive-agreement away from wraps (exact), bounded documented
  deviation at wraps, zero-crossing pitch, and Nyquist rejection.

## Evidence

- Bessel anchors vs mpmath (incl. J1(4) < 0 sign, J0 zero at 2.40483);
  sideband structure incl. k=2 lower at DC and signed lower amplitudes;
  FM trajectory vs inline textbook closed form + deviation wiring check.
- RBJ anchors (a0-normalized); |H(DC)| = 1, |H(Nyquist)| = 0 (lowpass);
  step starts at b0 exactly, settles at unity; reset restores pristine
  state bit-identically; block invariance 64/127/512 exact.
- Integrator meets 1e-9 against all three closed forms; envelopes shape
  partials independently; mix identities exact.
- Audible: `dsp_demo` renders FM brass, polyBLEP saw, and a lowpassed sweep
  (`dsp.wav`, 288044 bytes, all clean, sideband table printed).
- Caught in the open: the demo's hand-rolled render loop missed `done += n`
  and wrote 4.2 GB before the timeout kill — found by file-size evidence,
  fixed in one line. Demos are not suite-covered; this is now written down
  as the reason to keep demo loops trivial or promote them to tested code.
