# Instruments — violin and viola (first slice)

Status: assembly layer landed for two bowed strings. Verified against the
ideal-string law and the published tables below. Unvalidated as radiated
sound: no body resonator, no bow, no measured bridge admittance.

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

`physics/` does not include `instruments/`. The only music dependency is
the pitch→frequency adapter (`makePitch`), so open-string hertz are the
engine's A440 12-TET pitches rather than a second copy of that formula.

Bow, hammer, and reed are named excitation kinds. They return
`DomainError`. They are not played as a pluck.

## Sources

String table (both instruments), one length per instrument and a g/m range
plus a tension range per string: Knut Guettler, "Some typical properties
of bowed strings"
(<https://knutsacoustics.com/files/Typical-string-properties.pdf>), with
values contributed by Fan Tao (D'Addario) and Michel Simane
(Corelli/Savarez).

- Violin vibrating length **32.8 cm**. Viola vibrating length **37.5 cm**.
- Printed frequencies (0.1 Hz): violin 196.0, 293.7, 440.0, 659.3; viola
  130.8, 196.0, 293.7, 440.0. Those are the adapter's pitches at that
  resolution (`kTabulatedHertzTolerance` = 0.05 Hz).
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

Compass (approximate orchestral range, not a fingerboard stop):
HyperPhysics, "Playing Ranges of Instruments", Georgia State University
(<http://hyperphysics.phy-astr.gsu.edu/hbase/Music/orchins.html>):
violin G3–E7, viola C3–C6. The low end is the lowest open string.

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

## Still planned

Bow (friction / Helmholtz motion), hammer, reed, soundboard and cavity,
a measured bridge admittance in place of a caller-supplied oscillator,
fingered stopping as a change of vibrating length, and stiffness of wound
strings. None of those are implied by this slice.
