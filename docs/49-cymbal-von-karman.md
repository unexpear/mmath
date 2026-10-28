# Step 19: nonlinear crash cymbal (von Kármán plate)

Header: `include/mmathen/instruments/cymbal_vk.h`. Tests: `tests/test_cymbal_vk.cpp`.
Kit use: `tools/demo/band/band3.cpp` (GM 49), stroke speed from
`drumKitPlayerProfile().crashStickSpeedAt127` via `kitCrashStickSpeed` (player_profile.h).

## Model
- M. Ducceschi & C. Touzé, J. Sound Vib. 344, 313-331 (2015): modal von Kármán plate,
  free-edge transverse modes, clamped in-plane (Airy) modes, coupling H^l_ij (their eq. 9).
- Potential form. With E^s_il = H^l_si (D&T eq. 18) the cubic term of D&T eq. (14),
  sum_l (1/zeta_l^4) (sum_ij H^l_ij q_i q_j) (sum_r H^l_sr q_r), is the gradient of
  V = sum_l C_l (sum_ij H^l_ij q_i q_j)^2, so the modal system is Hamiltonian and the
  SAV scheme of Bilbao, Ducceschi & Zama (J. Comput. Phys. 472, 111697, 2023) applies.
- Validation (unit tests): with nu 0.38 the free-edge eigenvalues 5.093 / 9.175 / 11.897
  (D&T Table 1: 5.093 / 9.175 / 11.90), Gamma ratios 1 : 4.517 : 8.971 (1 : 4.518 : 8.973).

## Constants (see the header for each citation)
| constant | value | status |
|---|---|---|
| B20 rho / E / nu | 8607 / 112.6e9 / 0.342 | cited (Marogiannis et al. 2023), VERIFY |
| thickness | 0.5 mm | TUNED inside N&T 2019 measured 0.5-0.9 mm edge |
| loss law | 0.007 w^0.6 + 2 | cited (Nguyen & Touzé 2019), VERIFY for bronze |
| loss scale | 0.5 | TUNED (decay times, below) |
| observe point | 0.896 a, 0.519 rad | cited (D&T sec. 3.2.3) |
| in-plane modes / max mode | 40 / 10 kHz | TUNED for cost |
| oversample | 4 (192 kHz step) | TUNED (step-size test, below) |
| crash stroke | 11 m/s at vel 127, square law | endpoints cited (Dahl et al. 2011), shape TUNED |
| band3 crash mix gain | 1.52e-3 | TUNED (kit balance, not physics) |

## Evidence (box runs, 48 kHz output)
- Thickness sweep, 1.5 s at 10 m/s: 0.7 mm centroid 1.3-2.4 kHz, flatness -41 dB;
  0.5 mm centroid 2.2-4.5 kHz, -21 dB (real crash 2.4-4.1 kHz, -9.5 dB).
- Loss law: unscaled N&T law T60 5.3 / 4.0 / 3.4 / 3.1 s at vel 30 / 70 / 110 / 127 vs
  real 9.4 / 7.4 / 6.2 / 6.2 s (Virtuosity Drums CC0 crash, layers vl1-vl3).
  Scale 0.5: 10.4 / 7.7 / 6.6 / 6.1 s; scale 0.4: 13.1 / 9.5 / 7.8 / 7.7 s.
- Step size, vel 127, OS 2/3/4 vs OS 8: level at 0-0.3 / 0.3-1 / 1-2 s
  OS2 -0.41 / +1.36 / +2.05 dB, OS3 -0.17 / +0.64 / +1.15, OS4 -0.43 / +0.39 / +0.31;
  centroid OS4 within ~7 % of OS8 (the cascade is chaotic: statistics only).
  HF peak time OS2 543 / OS3 293 / OS4 383 / OS8 268 ms (real 299 ms): the peak time is the
  least converged statistic, about 115 ms between OS4 and OS8. No blow-up at any rate.
- Soft limit vs the same plate with the coupling off: vel 5 waveform within 4 %,
  vel 10 spectra within 0.34 dB (centroid 110 vs 109 Hz); vel 20 differs by 3.2 dB.
- Linear hand-off (switch the coupling off late in the decay) was tried and dropped:
  V/E does not fall monotonically, and the earliest trigger cost -1.4 dB at 4-6 s.

## Scoreboard (error vs the real crash; lower is better; before = linear plate at 1287090)
| row | vel 30 | vel 70 | vel 110 | vel 127 |
|---|---|---|---|---|
| HF (>5 kHz) peak time err, ms | 2464 -> 24 | 2455 -> 470 | 2224 -> 9 | 2224 -> 84 |
| >5 kHz energy curve err, dB | 38.9 -> 8.7 | 49.0 -> 7.3 | 46.1 -> 7.1 | 46.1 -> 5.4 |
| centroid err, octaves | 5.02 -> 3.14 | 5.16 -> 0.86 | 5.39 -> 0.11 | 5.35 -> 0.42 |
| decay abs ln T60 ratio | 0.368 -> 0.099 | 0.621 -> 0.052 | 0.896 -> 0.013 | 0.911 -> 0.049 |
| spectral flatness (density) err, dB | 78.1 -> 62.0 | 92.7 -> 49.4 | 99.8 -> 15.4 | 100.8 -> 10.1 |

Vel 110: HF peak 308 ms vs 299 ms real (the linear plate had none, 2.52 s = end of its window).

## Limits
Uniform plate (no taper or dome), single-point pressure proxy (acceleration, no radiation
model), cos family only. About 100x slower than real time single core at OS 4.
