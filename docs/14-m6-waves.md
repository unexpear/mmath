# M6 — Wave Kinematics + Air Medium (complete)

Status: implemented · 102/102 on MSVC Release+Debug, GCC-13 Release,
Clang-18 Release, GCC ASan+UBSan (zero reports) · Probes: 10/10

First physics-domain slice (branched sibling, not downstream of music).
Analytical rung of the solver ladder only: FDTD/FEM, the pressure wave
equation as a solved system, and all other media stay deferred.

## New headers (`include/mmathen/physics/`)

| Header | Contents |
|---|---|
| `waves.h` | `wavenumber` (k = 2π/λ, explicit rad/m), `phaseVelocity` (v = fλ), `wavelengthFromFrequency` / `frequencyFromWavelength` (through a given speed), `travelTime` (t = d/c, sign follows displacement), `travelingWave` (y = A·sin(kx−ωt+φ), all-finite checked) |
| `medium.h` | `Air{temperature}` + `makeAir` (T > 0 K); `speedOfSound` via dry-air `c = √(γRT)` (γ = 1.4, R = 287.058); humidity/pressure corrections and other media deferred, formula string carried |

## New units (added on demonstrated need, per policy)

`DistanceMeters`, `DisplacementMeters` (signed y — not the digital
`SignedAudioValue`), `SpeedMetersPerSecond`, `TemperatureKelvin`,
`WavenumberRadPerMeter` (rad/m tag, distinct from bare 1/m).

## Evidence

- c(0/20/30 °C) = 331.3214 / 343.2368 / 349.0420 m/s (Python literals);
  cold < warm ordering pinned.
- λ(440 Hz, 20 °C) = 0.7800835466619759 m; f↔λ round trip; v = fλ recovers c.
- Traveling wave analytic nodes (0, ±A, 0) on a 1 m / 1 Hz wave; 10 m delay
  0.02913440851883633 s; full validation-error tables.
- `air_table` demo prints the same tone's λ/k/delay across temperatures.
- Caught during authoring: two `Result::value()` vs quantity-double
  confusions in the new tests — the unit system rejecting them at compile
  time, as designed.
