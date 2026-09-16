# Math + Physics Audit (complete — findings applied in follow-up batch)

Status: 7 passes executed · Verdict: **no wrong math found** · F1+F2+D1–D3
plus the FM suite test applied afterward; full matrix re-verified at
104/104 on MSVC Release+Debug, GCC-13 Release, Clang-18 Release, and GCC
ASan+UBSan (zero reports); probes 10/10.

Method: mpmath 1.4.1 @ 50 digits from an isolated dir (zero system impact);
re-derivation of all 8 closed-form families; full suites re-run on MSVC
Release+Debug, GCC-13 Release, Clang-18 Release, GCC ASan+UBSan (102/102
everywhere, zero sanitizer reports); FM `rate=0` edge executed via a scratch
program outside the repo; docs grepped for convention/mystique claims.

## Sources (all live, none superseded)

NASA GRC (a² = γRT derivation + calorically-perfect bounds) · Stanford
CCRMA/J. Smith (γ = 1.4 dry air) · OpenStax Univ. Physics (`v = √(γRT/M)`,
`v = 331√(T/273)`) · ISO 16:1975 (A440, confirmed current 2022) · NIST
(WWV A440; 1 cent = 1200th root of 2 = 1.00057779) · McGill / UNSW Wolfe /
Puckette UCSD (`f = 440·2^((m−69)/12)`, M60 = middle C) · UCSD / Georgia
Tech / Montana State / Couch (phase = ∫frequency; naive product is wrong) ·
CMUSE (Yamaha C3=60 vs scientific C4=60 vs Ableton octave-label variants).

## Tier 0 — Wrong math: NONE

Every derivation re-performed and matched: sweep phase, FM integral
(`d/rm` factor verified by differentiation), tempo quadratic + inverse root
selection incl. decelerando and `a == 0`, envelope joint continuity incl.
short-note `v0` path, octave-wrap floor division, retrograde span proof,
MIDI table incl. B#/Cb crossings, `kMaxRenderFrames = 2147483628`
re-derived from `36 + 2N ≤ 2³²−1` (max admissible 2147483629; ours is
conservative by one), WAV header offsets, RMS identities, PCM mapping.

## Tier 1 — Robustness findings (behavioral; proposed diffs unapplied)

**F1. int-overflow UB on extreme degree arithmetic (low severity).**
`EqualTemperament::degreeToFrequency` computes `degree - refDegree_` in
`int` (`units/tuning.h`); `Scale::degreeFrequency` adds two `int`s;
`transposeSemitones` adds `midi + semitones` before range-checking
(`music/pitch.h`). Adversarial inputs (INT_MIN/MAX) are UB, not a defined
error — contradicting the error-handling contract. Realistic inputs never
trigger it. Proposed: promote to `long long` before arithmetic in all
three sites (same pattern already correct in `JustIntonation`).

**F2. Absurd `blockSize` escapes as `std::bad_alloc`, not `InvalidConfig`
(low severity).** `renderOffline` (`render/offline.h`) allocates
`vector<double>(blockSize)` unchecked; `blockSize = UINT64_MAX` throws
instead of reporting. Related: `kMaxRenderFrames` is a *format* limit —
a full-size `MemorySink` render needs ~17 GB RAM, undocumented. Proposed:
reject `blockSize > kMaxRenderFrames + 1` as `InvalidConfig` (blocks are an
internal batching detail; no legitimate need exceeds it) and document the
~8 bytes/frame RAM bound next to `kMaxRenderFrames`.

## Tier 2 — Doc gaps (prose additions proposed, unapplied)

**D1. Octave-label convention unstated.** Labels are scientific pitch
notation (C4 = MIDI 60 — Puckette/UNSW), but Yamaha (C3 = 60) and Ableton
variants exist in the wild (CMUSE). Math correct; convention undisclosed.
Proposed: one sentence in `pitch.h` + `docs/10-m2-structures.md`.

**D2. Ideal-gas accuracy range unstated.** Only validity (`T > 0 K`) is
recorded. NASA bounds the calorically-perfect model; for music purposes a
stated accuracy range (e.g. −20…+50 °C) plus the humidity direction (moist
air is *faster*, unquantified here) is missing. Formula and code correct.

**D3. R provenance unstated.** Engine uses conventional R = 287.058;
SI-derived R = 8.31446261815324/0.0289647 = 287.05502277 (mpmath).
Difference is 10 ppm → 0.0018 m/s in c — far below test tolerances and
humidity effects. Formula correct; constant choice should carry one
provenance sentence. Test anchors pin implementation fidelity against the
stated constant (correct scope), not physical truth — that scope should be
written down.

**Checked and cleared (no action):** `makePitch` hard-codes A440 — disclosed
in code ("12-TET spellings at the A440 reference in this milestone") and
`docs/10` ("12-TET/A440 labels"); `TwelveTET`/demos take reference as a
parameter, so no universality claim exists. 440 elsewhere is fixture/example
values. Mystique grep (432/scientific-pitch/universal-pitch): no
endorsements anywhere.

## Tier 3 — Fragile-anchor review: all justified, none tightened

All ~80 exact `==` assertions classified: binary-exact identities
(`pow(2,±1)`, `log2(1,2,1/2)`, `cents(2)`, octave doublings, MIDI 0/127,
ADSR segment joints — each re-verified bitwise-exact), same-expression
identity (just-chord products, enharmonic `==`, retrograde note moves),
or IEEE-order-fixed under the recorded no-contraction policy and confirmed
on 3 compilers + sanitizer (ramp `beatsAt` values, `0.44*1000 == 440`,
`E4/G4` products). FM discriminator margin quantified (0.227 vs 1e-6
tolerance); quantize margins ≫ 1 ulp; zero-crossing ±1 and RMS tolerances
unchanged. No tighten diffs warranted — tightening exact-pinned identities
would weaken the suite.

## Untested-claim disposition

FM `rate = 0` downstream flagging: **executed** (scratch, outside repo) —
`tryAt` rejects, render completes 480/480 NaN flagged, `clean = 0`, exactly
as documented. Proposed: mirror it as a suite test (shares the scratch's
assertions). Everything else claimed is suite-covered.
