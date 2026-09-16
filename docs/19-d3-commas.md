# D3 — Tuning Mathematics: Commas, Errors, MOS (complete)

Status: implemented · 143/143 on MSVC Release+Debug, GCC-13 Release,
Clang-18 Release, GCC ASan+UBSan (zero reports) · Probes: 10/10

Depth milestone (research-grade) for tuning theory: commas as exact
rationals (relations hold by integer arithmetic, not tolerance), EDO
errors as computed measurements, MOS as generate-and-verify. Full
regular-temperament mapping theory explicitly deferred — this module
measures temperaments, it does not map them.

## New code

| Header | Contents |
|---|---|
| `music/commas.h` | `Comma{num,den}` reduced at construction; checked mul/div (RangeError, never wraparound); `commaCents`; `primeLimit` (6k±1 trial division, both sides); `tenneyHeight` (log-sum, no product overflow); 7 named constants |
| `music/temperament.h` | `nearestDegreeError` (degree + signed cents); `errorReport` (worst/mean over a target set) |
| `music/mos.h` | `mosWord` (stack-sort-measure; 1e-9 step clustering; degenerate stacks rejected); `isMOS` = exactly two step sizes |

## Evidence

- All 7 comma cents values mpmath-anchored; PC/syntonic == schisma and
  syntonic*schisma == pythagorean hold EXACTLY (integer proof, strongest
  assertions in the file); construction normalizes (162/160 == 81/80).
- EDO table reproduces the published xen.wiki values (12: −1.955/+13.686/
  −15.641; 19: −7.218/−7.366/+0.148; 31 fifth −5.181); triad ranking
  computed: 12 (15.64) > 19 (7.37) > 31 (5.96) — measured, not asserted.
- MOS diatonic 5+2 and pentatonic 3+2 (order-independent multiset checks);
  zero/tritone generators rejected; single note well-defined non-MOS.
- Audible: `comma_demo` renders Pythagorean vs just vs 12-TET thirds (the
  ~22¢ comma clearly audible) plus printed error tables (`thirds.wav`,
  230444 bytes, clean).
- Caught during implementation: `primeLimit` factored only `max(num,den)`
  (denominator primes missed) — fixed to factor both sides with regression
  coverage; plus a leftover tautological CHECK removed before building.
