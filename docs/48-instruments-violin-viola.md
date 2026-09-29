# Instruments — violin, viola, cello, and a Helmholtz bow

Status: assembly layer for three bowed strings, plus a steady Helmholtz
bow on one ideal string. Verified against the ideal-string law, Guettler's
tables, and the Helmholtz/Schelleng identities below. Unvalidated as
radiated sound: no body resonator and no measured bridge admittance.

An instrument is objects + materials + excitation (architecture L26–L27).
Geometry is a length. Material is a linear density. Tuning is a pitch the
string is asked to play. Those three stay separate; tension is the number
the ideal-string law needs to make the geometry and the material hit the
tuning, and it is kept only when it falls inside the published tension
interval.

## What landed

| Header | Contents |
|---|---|
| `instruments/instrument.h` | `StringCourse` (geometry, material, tuning, derived tension); `makeStringCourse` / `makeStringInstrument`; pluck → `InitialShape` and the modal sum; `coupleToBridge` (caller-supplied `BridgeOscillator`) |
| `instruments/violin.h` | G3 D4 A4 E5, vibrating length 0.328 m, body 0.356 m, compass G3–E7 |
| `instruments/viola.h` | C3 G3 D4 A4, vibrating length 0.375 m, body 0.410 m, compass C3–C6 |
| `instruments/cello.h` | C2 G2 D3 A3, vibrating length 0.690 m, body 0.755 m, compass C2–E5 |
| `instruments/bow.h` | Steady Helmholtz stick/slip, bridge-force sawtooth, Schelleng maximum normal force |

`physics/` does not include `instruments/`. The only music dependency is
the pitch→frequency adapter (`makePitch`), so open-string hertz are the
engine's A440 12-TET pitches rather than a second copy of that formula.

A pluck is an initial shape. Hammer and reed still return `DomainError`
from that path. A bow is not an initial shape; `makeHelmholtzStroke`
builds the steady regime.

## Sources

String table (all three instruments), one length per instrument and a g/m
range plus a tension range per string: Knut Guettler, "Some typical
properties of bowed strings"
(<https://knutsacoustics.com/files/Typical-string-properties.pdf>), with
values contributed by Fan Tao (D'Addario) and Michel Simane
(Corelli/Savarez).

- Violin vibrating length **32.8 cm**. Viola **37.5 cm**. Cello **69.0 cm**.
- Printed frequencies (0.1 Hz): violin 196.0, 293.7, 440.0, 659.3; viola
  130.8, 196.0, 293.7, 440.0; cello 65.4, 98.0, 146.9, 220.0. Agreement
  with the pitch adapter is one printed unit
  (`kTabulatedHertzTolerance` = 0.1 Hz). The cello D3 is the wide case:
  Guettler prints 146.9 Hz and the adapter's A440 D3 is 146.83 Hz. His
  propagation speed 202.7 m/s is 2·0.69·146.9, so 146.9 is the table's
  figure, not a mis-read.
- Nominal linear density is the **arithmetic midpoint** of Guettler's g/m
  range. That is an explicit estimate inside sourced bounds, not a
  measurement of one commercial string.
- Tension is \(T = \mu (2 L f)^2\) at the adapter frequency. Construction
  fails if \(T\) leaves the published tension interval.

Body length is a different geometric object and is not folded into \(L\):

- Violin **356 mm**: Erik Jansson, *Acoustics for Violin and Guitar
  Makers*, KTH, chapter 7 (bridge at 195 mm of a 356 mm body). The MENC
  4/4 nominal body length is the same 356 mm. Guettler's 32.8 cm is not
  Jansson's nut-to-bridge (327 mm) and not the MENC nominal string length
  (330 mm); those conventions are not mixed into \(T\) or \(\mu\).
- Viola **410 mm**: MENC nominal size chart, 16-inch standard viola
  (nominal string length 375 mm, which matches Guettler's vibrating
  length). The body length is still stored separately.
- Cello **755 mm**: MENC nominal size chart, 4/4 cello (nominal string
  length 695 mm). Guettler's vibrating length is 69.0 cm, not 695 mm.

Compass (approximate orchestral range, not a fingerboard stop):
HyperPhysics, "Playing Ranges of Instruments", Georgia State University
(<http://hyperphysics.phy-astr.gsu.edu/hbase/Music/orchins.html>; the page
cites Rossing, *Science of Sound*, p. 219, and White & White, p. 280):
violin G3–E7, viola C3–C6, cello C2–E5. The low end is the lowest open
string.

## What the same pitch does on the longer instrument

G3, D4, and A4 exist on both. The adapter frequency matches. The viola's
vibrating length is longer, so the wave speed \(c = 2 L f\) is higher, and
Guettler's density range for that string is not the violin's. A viola is
not a violin with the labels rewritten.

## Verification

- Open-string fundamental from `stringMode` matches the pitch adapter
  (relative \(10^{-12}\)).
- Derived tension lies strictly inside Guettler's published interval;
  nominal density is the range midpoint.
- Adjacent open strings are equal-tempered fifths (\(2^{7/12}\)), because
  that is what the adapter returns.
- A pluck's modal sum stays within `ModalSolution::truncationBound` of
  the d'Alembert field. Ends stay at rest within that bound.
- `coupleToBridge` with a rigid oscillator recovers the fixed-fixed
  fundamental; a massless free end recovers half of it (fixed-free).
- Bad tables (tension band that cannot tune the pitch, published hertz
  that is a different note, courses not low-to-high, compass that is not
  the lowest open string) fail construction.
- Helmholtz stick and slip velocities, their durations, the bridge-force
  slope and flyback, and a zero-mean bow displacement match the identities
  above. A normal force above Schelleng's maximum is refused. The cello's
  A string, at the same β and bow speed, has a different impedance and a
  longer period than the violin's A string.

## Bow

`bow.h` is steady ideal Helmholtz motion on the lossless fixed-fixed
string. β is the fraction of the vibrating length measured from the
bridge. With bow speed \(v_b\) and period \(P = 2L/c\):

- stick lasts \((1-\beta)P\) at velocity \(v_b\)
- slip lasts \(\beta P\) at velocity \(-v_b(1-\beta)/\beta\)
- relative slip speed \(-v_b/\beta\)
- during slip, friction on the string has magnitude \(\mu_d N\) and
  opposes that relative velocity
- the force on a rigid bridge is a zero-mean sawtooth of slope
  \(T v_b/(\beta L)\) and flyback \(-2Zv_b/\beta\), \(Z=\mu c\)

Schelleng's maximum normal force \(2Z|v_b|/(\beta(\mu_s-\mu_d))\) is
enforced (Woodhouse, Euphonics §9.3.1). A heavier bow is `DomainError`.
Schelleng's minimum force is not enforced: it needs a bridge resistance,
and this slice does not invent one. Friction coefficients are arguments,
not a hidden rosin table.

Not in this bow: capture transients, bow-hair width, torsion, thermal
rosin, a rounded corner, and any body filter on the bridge force.

## Still planned

Hammer, reed, soundboard and cavity, a measured bridge admittance (and
with it Schelleng's minimum bow force), fingered stopping as a change of
vibrating length, stiffness of wound strings, and a time-stepped
stick-slip capture. The Helmholtz stroke is the steady regime only.
