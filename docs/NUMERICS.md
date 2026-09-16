# NUMERICS — Precision, Determinism, Tolerances

Status: canonical · Labels: **[ACCEPTED] / [PROPOSED] / [DEFERRED]**

## Arithmetic policy

- **[ACCEPTED]** Foundation uses `double`. Reference execution policy, stated
  per compiler (not just "no fast-math", since fusion changes rounding points):
  MSVC `/fp:precise` (contraction off by default in VS2022, stated explicitly);
  GCC/Clang `-fno-fast-math -ffp-contract=off`. These are a reference policy,
  not a cross-platform identical-transcendentals guarantee. Compiler (+ stdlib
  version where available), target architecture, configuration, and effective
  FP options are recorded with results (build/numerics.json).
- **[ACCEPTED]** Header-defined code is also compiled by a separate consumer
  target under the documented policy: flags on the header-only library target
  alone prove nothing about consumers. (Phase 0 consumers: the test executable
  plus the isolated try_compile probes.)
- **[ACCEPTED]** Comparison criterion: `|a-b| <= max(eps_abs,
  eps_rel*max(|a|,|b|))`, `==` stays exact. Absolute tolerance carries the
  quantity's unit (a Duration for TimePoints); relative tolerance is
  dimensionless. No global epsilon; invalid tolerances throw; non-finite
  operands compare true only when exactly equal.
- **[ACCEPTED]** C++20 has no portable constexpr `sin/cos/log2/pow`: quantity
  arithmetic may be compile-time tested; transcendental functions are runtime-tested.
  No hand-rolled trig to force `static_assert`.
- **[PROPOSED]** Record exact toolchain + flags in a `numerics.json` beside test logs
  (compiler, version, `/fp` or `-ffp-*` mode, arch FMA availability).

## Determinism tiers

- **[ACCEPTED]** Tier 1 — same supported build/config + deterministic inputs →
  identical output (exact for frames/sizes; bitwise for fixed encoding fixtures).
- **[ACCEPTED]** Tier 2 — different supported compilers/platforms → agreement within
  documented per-quantity tolerances (not bitwise).
- **[ACCEPTED]** Tier 3 — "identical WAV bytes everywhere" is **not** promised for
  synthesized signals; required only for fixed encoding fixtures with locked inputs.
- **[ACCEPTED]** No RNG in MVP render path; payload contains no timestamps.

## Tolerances (never one global epsilon)

- **[ACCEPTED]** `FrameCount` / file sizes: exact integer equality.
- **[ACCEPTED]** Phase / signal values: absolute + relative tolerance pair, documented
  per test (e.g. sine closed-form, sweep phase, composition identity).
- **[ACCEPTED]** Estimated frequency (zero-crossing): tolerance in Hz + minimum-cycle
  requirement documented with the test.
- **[ACCEPTED]** Float-buffer statistics (mean, RMS) checked pre-quantization;
  PCM conversion checked separately against known-value fixtures with its own
  ±½-LSB-aware criteria. Quantized output never compared directly to unquantized identity.
- **[DEFERRED]** Physical-measurement uncertainty bounds (later models carry
  uncertainty as data, separate from numerical tolerance).

## Non-finite + overflow

- **[ACCEPTED]** NaN/Inf/overflow are detected and reported at construction,
  operation, and render boundaries; render fails on non-finite rather than writing it.
  No silent substitution, saturation, or renormalization except where an explicit,
  counted overload policy declares it (WAV export boundary).
