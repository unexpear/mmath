# D2 — Waves Deep: Modes, Impedance, Absorption (complete)

Status: implemented · 121/121 on MSVC Release+Debug, GCC-13 Release,
Clang-18 Release, GCC ASan+UBSan (zero reports) · Probes: 10/10

Second depth milestone (research-grade). Analytical only: no solvers, no
object assembly (those consume these relations later). End corrections
(~0.61r per open end, LibreTexts) cited and deferred, not fudged.

## New code

| Header | Contents |
|---|---|
| `physics/modes.h` | `stringWaveSpeed` (v = √(T/μ)); `stringMode` (f_n = n·v/2L); `openPipeMode` (all integers); `stoppedPipeMode` (odd n only — even n are `DomainError`, nonexistent resonances, not numbers) |
| `physics/impedance.h` | `airDensity` (ρ = p/RT); `characteristicImpedance` (z = ρc); `airImpedanceAt` (101325 Pa); `waterImpedance` (1.48 MRayl, UNSW); `reflectionCoefficient` / `transmissionCoefficient` with energy identity |
| `physics/absorption.h` | `HumidAir` + factory (ISO scope enforced); `saturationVapourPressure`; `molarConcentration` (**mole percent** — the x100 is the standard's unit); O₂/N₂ relaxation frequencies; `attenuationCoefficient` (dB/m); `attenuationOverDistance` (dB) |

## Provenance (ISO 9613-1 primary is paywalled, CHF 135)

Implemented from the open python-acoustics transcription (BSD-3) of the
standard's equations, verified term-by-term, and checked against ISO
Table 1 spot values fetched from the standard-derived CSV. Humidity-
corrected `c` (Cramer-type) deliberately NOT implemented — unverified
from available sources; humidity enters absorption only, and the boundary
is stated, not silent.

## Findings during implementation (both fixed before green)

1. **Mole-percent trap**: first transcription passed humidity as a fraction
   into equations expecting mole percent — compiled, ran, shifted every
   relaxation frequency ~100x. Caught by Table 1 comparison (6.35 vs 7.96
   dB/km), not by the type system. This is now the documented reason the
   x100 lives loudly in `molarConcentration()`.
2. **Boundary-inclusion fragility**: exact comparison rejected T computed
   as `-20.0 + 273.15` (1-ulp short of the literal). ISO scope now carries
   a documented ±1e-9 K tolerance band — nine orders below physics.

## Evidence

- String A110 harmonics and pipe series exact; stopped-pipe missing evens
  as `DomainError`; matched R = 0 / T = 1 exact; air→water R/T anchors;
  energy identity to 1e-12; air density + impedance anchors.
- Relaxation frequencies exact at zero humidity (9.0/24.0 Hz); p_sat anchor;
  Table 1 spots within 2% (table precision + era computation, documented);
  humidity ordinal (4 kHz absorption *falls* as RH rises) pinned.
- Audible: `modes_demo` renders open vs stopped C4 pipes (missing evens
  clearly audible) plus the impedance/absorption print table.
