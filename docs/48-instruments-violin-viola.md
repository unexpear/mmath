# Instruments — bowed strings, harp, harpsichord, guitars, piano, drums, cymbals, clarinet, flute, trumpet, saxophone

Status: assembly layer for four bowed strings, a concert pedal harp, a
French double harpsichord, a six-string classical guitar, a six-string
electric guitar, a five-string concert-grand bank, a steady Helmholtz bow on one ideal string,
a sum of driving-point body modes driven by that bow's bridge force, a cavity Helmholtz frequency from caller area, volume, and neck, a hard narrow hammer,
the first fixed-end images of that hammer, a felt contact law, a
quasi-static single reed, a cylindrical bore, reed inertia, a clarinet
cylinder on that open bore, an open-open flute driven by an embouchure
jet, a lip-driven trumpet with a mouthpiece cup, and a single-reed
truncated cone. A
fingered stop shortens one open course. A Yamaha Tour Custom kit and a
SABIAN XSR cymbal set add fixed circular membranes and free circular
plates. Verified against the ideal-string
law, Guettler's tables, the harp, harpsichord, classical-guitar,
electric-guitar, and Table III piano measurements named below, the Helmholtz/Schelleng identities, the rigid-end stick-slip step, the wound-string inharmonicity, the driving-point modal sum, the cavity Helmholtz frequency, the mutual-stiffness split, the compact-monopole pressure,
the hard-hammer contact law, the first-image sum, the felt power law, the
reed flow law, the open-cylinder clarinet, the open-pipe flute and its embouchure jet, the lip-driven trumpet, the truncated-cone saxophone, the circular-membrane Bessel frequencies, and the free-plate
frequency law. Unvalidated as radiated sound: the body-mode sum is
open-loop, and there is no measured bridge admittance and no far-field
pressure. The bore is an ideal cylinder with the caller's length and
cross-section. The clarinet is that cylinder open at the bell, with the
reed and its inertia; tone holes, a bell, and a register key are absent.
The flute is an open-open pipe, fundamental \(c/(2L)\), driven by a
Bernoulli jet. It is not that clarinet. The trumpet is the quarter-wave
cylinder with a mouthpiece cup in parallel and an outward-striking lip.
The saxophone uses the clarinet's inward reed on a truncated cone, so
its impedance poles are neither the quarter-wave series nor \(nc/(2L)\)
except in those limits of the apex distance. The harp and the harpsichord are
not sounded through a soundboard model. The classical guitar is six
ideal strings on the published scale; it has no soundboard either. The
electric guitar is the 25.5 in chart scale, not that classical body.
The piano bank is five Table III strings and has no soundboard either.

An instrument is objects + materials + excitation (architecture L26–L27).
Geometry is a length. Material is a linear density. Tuning is a pitch the
string is asked to play. Those three stay separate; tension is the number
the ideal-string law needs to make the geometry and the material hit the
tuning, and it is kept only when it falls inside the published tension
interval.

## What landed

| Header | Contents |
|---|---|
| `instruments/instrument.h` | `StringCourse` (geometry, material, tuning, derived tension); `makeStringCourse` / `makeStringInstrument`; pluck → `InitialShape` and the modal sum; `coupleToBridge` (caller-supplied `BridgeOscillator`); `fingerStop` shortens the vibrating length |
| `instruments/violin.h` | G3 D4 A4 E5, vibrating length 0.328 m, body 0.356 m, compass G3–E7 |
| `instruments/viola.h` | C3 G3 D4 A4, vibrating length 0.375 m, body 0.410 m, compass C3–C6 |
| `instruments/cello.h` | C2 G2 D3 A3, vibrating length 0.690 m, body 0.755 m, compass C2–E5 |
| `instruments/double_bass.h` | Orchestral E1 A1 D2 G2, vibrating length 1.060 m, body 1.110 m, sounding compass E1–B3 (written E2–B4) |
| `instruments/bow.h` | Steady Helmholtz stick/slip, bridge-force sawtooth, Schelleng maximum and minimum normal force, and a time-stepped stick-slip capture on rigid ends |
| `instruments/body.h` | Sum of driving-point body modes, a cavity Helmholtz frequency, a mutual stiffness between two modes, a compact-monopole far field, and a string re-solved on a moving bridge. Resonances, Q, masses, f-hole area, volume, neck, mutual stiffness, radiating area or volume velocity, distance, and bridge resistance are arguments. No stored admittance, f-hole, A0/B1, or radiating-area table |
| `instruments/hammer.h` | Hard narrow hammer until the first reflection; first-image sum until the second image; felt law \(F=K\xi^{p}\). Mass, speed, strike, \(K\), and \(p\) are arguments |
| `instruments/reed.h` | Quasi-static single reed; cylindrical bore; reed inertia. Area, closure, air state, bore length and cross-section, mass, tip area, closing displacement, and damping are arguments |
| `instruments/clarinet.h` | Open-cylinder clarinet: the characteristic, the bore, and reed inertia on one air state. Length, area, and the reed parameters are arguments |
| `instruments/flute.h` | Open-open flute and an embouchure jet. Length, bore area, jet area, thickness, flue-to-edge distance, and the convection ratio are arguments |
| `instruments/trumpet.h` | Lip-driven cylinder and a mouthpiece cup. Length, bore area, cup volume, and the lip parameters are arguments |
| `instruments/saxophone.h` | Inward reed on a truncated cone. Length, apex distance, throat area, and the reed parameters are arguments |
| `instruments/harp.h` | Camac Atlantide Prestige concert pedal harp, 47 strings, height 1.86 m |
| `instruments/harpsichord.h` | Goermans/Taskin 1764/1783–4 French double: 2×8′ + 4′, compass F1–F6 |
| `instruments/guitar.h` | Classical guitar E2 A2 D3 G3 B3 E4, scale 650 mm, body 490 mm. Density and tension are arguments |
| `instruments/electric_guitar.h` | Electric guitar E2 A2 D3 G3 B3 E4, scale 25.5 in. EXL110 tensions are printed; linear density is an argument |
| `instruments/piano.h` | Chabassier Table III concert-grand bank: D#1, C2, F3, C#5, G6. Stiff string, hammer, and felt reuse the existing models |
| `instruments/drum.h` | Yamaha Tour Custom shells: bass, two rack toms, floor tom, two snares. Head tension and areal density are arguments |
| `instruments/cymbal.h` | SABIAN XSR5006B: splash, hi-hat pair, two crashes, china, ride. Thickness and alloy constants are arguments |
| `instruments/wound.h` | Bending stiffness of a wound string. Core diameter, Young's modulus, and core density are arguments |
| `physics/circular_plate.h` | Free circular plate eigenvalues and frequencies. No alloy table |

`physics/` does not include `instruments/`. The only music dependency is
the pitch→frequency adapter (`makePitch`), so open-string hertz are the
engine's A440 12-TET pitches rather than a second copy of that formula.

A pluck is an initial shape. A bow is not; `makeHelmholtzStroke` builds
the steady regime. A hammer is not either; `makeHardHammerBlow` is the
hard narrow hammer until the first reflection returns. A reed is not
either; `makeReedCharacteristic` is the quasi-static slit.
`realizeExcitation` still returns `DomainError` for a bow, a hammer, and
a reed, because that function only builds an initial displacement. The
harp is finger-plucked and the harpsichord is plucked by a jack. The
classical guitar is finger-plucked on the same ideal string. None of
those factories stores a bow, a hammer, or a reed. Peau de buffle is a jack
register on the lower 8', not a fourth choir and not a hammer.

## Sources

String table (all four instruments), one length per instrument and a g/m
range plus a tension range per string: Knut Guettler, "Some typical
properties of bowed strings"
(<https://knutsacoustics.com/files/Typical-string-properties.pdf>), with
values contributed by Fan Tao (D'Addario) and Michel Simane
(Corelli/Savarez).

- Violin vibrating length **32.8 cm**. Viola **37.5 cm**. Cello **69.0 cm**.
  Double bass **106.0 cm**, the orchestra-tuning block of the same table
  (not the solo-tuning block F#1 B1 E2 A2, which uses the same length).
- Printed frequencies (0.1 Hz): violin 196.0, 293.7, 440.0, 659.3; viola
  130.8, 196.0, 293.7, 440.0; cello 65.4, 98.0, 146.9, 220.0; double bass
  orchestra tuning 41.2, 55.0, 73.4, 98.0. Agreement
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
- Double bass **1110 mm**: MENC nominal size chart, 3/4 standard bass
  (the column marked STD; nominal string length 1060 mm, which matches
  Guettler's 106.0 cm). The 4/4 column is 1160 mm of body and 1100 mm of
  string and is not this instrument. Body length is still stored
  separately from the vibrating length.

Compass (approximate orchestral range, not a fingerboard stop):
HyperPhysics, "Playing Ranges of Instruments", Georgia State University
(<http://hyperphysics.phy-astr.gsu.edu/hbase/Music/orchins.html>; the page
cites Rossing, *Science of Sound*, p. 219, and White & White, p. 280):
violin G3–E7, viola C3–C6, cello C2–E5, double bass sounding E1–B3
(printed 41.2 Hz to 246.9 Hz). The low end is the lowest open string.
Orchestral double-bass parts are written an octave higher, so the
practical written compass for that sounding range is E2–B4. The
frequencies on the instrument are the sounding pitches.

## Double bass strings

Guettler's orchestra-tuning rows at 106.0 cm, low to high:

| Course | Printed Hz | g/m range | Tension range (N) |
|---|---|---|---|
| E1 | 41.2 | 37.32–44.12 | 284.7–336.6 |
| A1 | 55.0 | 20.45–23.88 | 278.0–324.7 |
| D2 | 73.4 | 10.96–13.12 | 265.3–317.6 |
| G2 | 98.0 | 5.98–7.25 | 258.0–312.8 |

The same block lists a B0 extension (30.9 Hz, 68.81–81.35 g/m,
295.3–349.1 N). This instrument has four courses, so that string is not
built. The solo-tuning block (F#1 B1 E2 A2) is a different tuning and is
not built either.

G2 is also the cello's G string. The adapter frequency matches. The bass
vibrating length is 106.0 cm against the cello's 69.0 cm, so the wave
speed is higher, and Guettler's density and tension ranges for the bass
G2 are not the cello's. A double bass is not a cello with the labels
rewritten, and the body length is not used as \(L\).

The Helmholtz bow is unchanged and can be applied to a bass course the
same way as to a violin, viola, or cello course. The body-mode sum and the
hard hammer are the same functions for any of the four courses. The reed
does not take a course. None of these four factories stores a resonance, a
hammer mass, a strike speed, or a reed.

## Concert pedal harp

Camac Atlantide Prestige, a 47-string concert pedal harp, not a folk harp.
String geometry is Jean-Loïc Le Carrou, *Vibro-acoustique de la harpe de
concert*, thèse, Université du Maine, 2006, Annexe A.1
(<https://theses.hal.science/tel-00133443>). The study instrument did not
have the pedal discs fitted, so the lengths are the full open lengths.
Natural and sharp speaking lengths are not in the table and are not
inferred from a fork spacing.

The table is the flat-pedal string set, numbered 00 (highest) to 45
(lowest). Stored labels are the A440 scientific spellings of the printed
hertz, low to high Cb1–Gb7. Chadefaux's "Do♭−1" to "Sol♭6" is another
octave count for the same 31 Hz–2960 Hz span and is not the stored
spelling. Printed hertz agree with the adapter within one printed unit:
0.1 Hz where the table prints a tenth, 1 Hz where it prints a whole
number. Cb7 is the wide case (printed 1975 Hz, adapter 1975.53 Hz).

Materials, as printed: nylon 00–6, sheep gut 7–33, wound steel 34–45.
Each row also stores the printed diameter, angle, and soundboard
position. None of those is the vibrating length. At the gut/steel break
the diameter drops (string 33 is 2.36 mm, string 34 is 1.6 mm), which is
the table and not a smoothed gauge.

Vibrating lengths run from 7.7 cm (string 00) to 154 cm (string 45).
Instrument height is a different object: Delphine Chadefaux, *Musician /
instrument interaction: the case of the concert harp*, thèse, Université
Pierre et Marie Curie, 2012
(<https://theses.hal.science/tel-00828314>), Camac Atlantide Prestige,
"une hauteur de 1 m 86". Le Carrou's Annexe A.2 prints other millimetre
figures without naming the edge; they are not used as the height or as
\(L\).

Annexe A.1 does not print linear density or a per-string tension
interval. `tuneHarpString` takes both as arguments. Nominal density is
the midpoint of the caller's g/m range, and tension is
\(T=\mu(2Lf)^2\) at the adapter frequency. Construction fails if that
tension leaves the caller's interval. Table 7.1's volumetric densities
are hypotheses taken from another harp (Bell and Firth) and are not
used, including for the wound strings, where an outer diameter and a
steel density would not be the linear density. Chadefaux reports that
the tensions on an Atlantide Prestige are spread from 45 N to 756 N.
That is the spread of the set, not an allowance for each string, and
`tuneHarpString` does not read it.

## French double harpsichord

Jean Goermans / Pascal Taskin, Paris 1764 / 1783–4, Russell Collection,
Musical Instrument Museums Edinburgh, accession 4329
(<https://collections.ed.ac.uk/mimed/record/14796>). Two manuals. Compass
61 notes, F1–F6. Choirs: two 8' and one 4'. Peau de buffle is a jack
register on the lower 8'; the buff is a batten on the upper 8'. The
catalogue says gauge numbers are stamped and does not print them.

Case length 2365 mm is Table 1 of "Taskin Harpsichord Scalings and
Stringings Revisited", Bray Baroque, 2011
(<https://www.braybaroque.ie/harps/Taskin_stringing.htm>), the 1784
Edinburgh row. The museum Measurements field prints 2365 and does not
name the unit; the millimetre case length is that table. The longest
stored speaking length is the long 8' F1 at 1835 mm, which is not the
case length.

The catalogue prints eleven long-8' speaking lengths and plucking
points (mm): F6 142 (80), C6 184 (87), F5 271 (98), C5 366 (105),
F4 550 (113), C4 713 (124), F3 993 (138), C3 1207 (148), F2 1532 (162),
C2 1727 (174), F1 1835 (193). Bray's long-8' column matches F, C, c,
c', c'' (F1, C2, C3, C4, C5). The plucking point is stored as printed.
It is not turned into a fraction, because the catalogue does not say
which end it is measured from.

The 4' lengths at c, c', and c'' are Bray's Table 1, not half the 8':
C3 637 mm against 1207 mm, C4 351 mm against 713 mm, C5 169 mm against
366 mm. The short 8' speaking lengths are in neither source and are
not invented. The other 4' notes are not invented either.

The catalogue does not print hertz, a linear density, or a tension
interval. `tuneHarpsichordString` takes the density and tension ranges
as arguments and tunes the catalogue's note name with the A440 adapter.
The course records no published frequency. Historical pitch is not
A440 and is not stored. The same density on the long 8' C3 and the 4'
C3 produces two tensions, because the speaking lengths differ; a
tension interval that holds one refuses the other.

## Classical guitar

Six open strings, low to high E2 A2 D3 G3 B3 E4. The vibrating length
is 650 mm. Savarez's standard-prime block prints 65 cm for those note
names (positions 6 to 1). Yamaha's C40II prints the same scale length.
The body length on that Yamaha row is 490 mm and is stored separately.
Total length 995 mm, nut width 52 mm, and string spacing 11.8 mm are
not the scale and are not stored. The neck makes the scale longer than
the body; the body is not substituted for \(L\).

The Savarez block prints 82.4, 110, 146.79, 196, 246.88, and 329.6 Hz.
The E2 and E4 figures, printed to a tenth, agree with the A440 adapter.
146.79 and 246.88 do not agree with D3 and B3 within one printed
hundredth, so they are kept on the row and are not written onto the
course. `tuneClassicalGuitarString` tunes the pitch label.
`frequencyPrinted` is false. The block does not print a diameter or a
tension: it says the technology depends on the Savarez set. Yamaha
names the strings only as Medium. Density and tension ranges are
arguments. A tension that fits \(T=\mu(2 L_{\mathrm{body}} f)^2\) is
refused, because the course length is the scale. D'Addario's classical
tension chart is computed at 25.5 in and is not applied here.

The open strings ascend by fourths except G3–B3, which is a major
third. That is the 12-TET spelling, not a fret spacing. No fret table
is stored. `fingerStop` shortens one course and keeps its tension.
A pluck is `ExcitationKind::Pluck` on that course. There is no
soundboard. This is not the desktop chord recognizer.

## Electric guitar

Six open strings, low to high E2 A2 D3 G3 B3 E4, on a different scale
from the classical guitar. The vibrating length is 25.5 in, the scale
D'Addario's tension charts use for guitar. Fender prints the same
25.5 in and a parenthetical 648 mm. That millimetre is a rounding.
\(L\) is \(25.5\times 0.0254\,\mathrm{m}\). No body length is printed
with the scale, so none is stored. The 650 mm classical scale and the
490 mm classical body are not this length.

The courses are D'Addario EXL110, 10-46 regular light. The product
chart prints, low to high, NW046 16.900 lb, NW036 19.000 lb, NW026
18.400 lb, PL017 16.600 lb, PL013 15.400 lb, and PL010 16.200 lb. The
pounds are not monotonic in pitch. The page does not print unit
weight, so `tuneElectricGuitarString` takes linear density and keeps
the course only when \(T=\mu(2Lf)^2\) agrees with the printed pounds
within 0.001 lb. The pound-force is \(0.45359237\times 9.80665\,\mathrm{N}\),
not the catalog's rounded factor 4.45. The chart does not print hertz,
so `frequencyPrinted` is false. Fender's .009–.042 gauge is a different
set and is not used. No pickup constant is printed, and no pickup
model is stored. A pluck is `ExcitationKind::Pluck`. `fingerStop`
keeps the open tension. The same density at 650 mm misses the EXL110
window.

## Piano string bank

`concertGrandStringBank` is Chabassier, Chaigne, and Joly 2013, Table
III, through `chabassier2013Strings()`. Five strings: D#1, C2, F3,
C#5, G6. Speaking lengths 1.945 m, 1.600 m, 0.961 m, 0.326 m, and
0.124 m are that table and are not one shared length. Tension is the
printed \(T_0\). Linear density is \(\rho F \pi d^2/4\) with the
Table III steel. The ideal fundamental stays within 0.5% of the
printed \(f_0\), the same check as the D16 annex. C#5's printed
555.6 Hz is not the A440 adapter, and the course is not rewritten to
the adapter. `tunedPianoString` is `pianoString` on that row, so the
first stiff partial sits above the flexible fundamental. A strike is
`makeHardHammerBlow`. Felt is \(F=K\xi^p\) with \(K\) and \(p\) as
arguments. No hammer mass is stored. The other piano keys are not in
the table and are not invented. This is not the desktop piano panel.

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
- Adjacent open strings on violin, viola, and cello are equal-tempered
  fifths (\(2^{7/12}\)). Orchestral double-bass tuning ascends by
  fourths (\(2^{5/12}\)): E1 A1 D2 G2. Both are what the adapter returns.
- A pluck's modal sum stays within `ModalSolution::truncationBound` of
  the d'Alembert field. Ends stay at rest within that bound.
- `coupleToBridge` with a rigid oscillator recovers the fixed-fixed
  fundamental; a massless free end recovers half of it (fixed-free).
- Bad tables (tension band that cannot tune the pitch, published hertz
  that is a different note, courses not low-to-high, compass that is not
  the lowest open string) fail construction.
- The Atlantide's 47 strings ascend Cb1–Gb7, each printed hertz within
  its printed unit of the adapter, and every vibrating length is shorter
  than the 1.86 m height. Nylon, gut, and wound steel change at the
  printed string numbers. A course whose ideal tension leaves the
  caller's interval is refused, including when that tension still sits
  inside the 45–756 N instrument-level spread.
- The French double's case is 2365 mm and its long 8' F1 is 1835 mm.
  The compass is 61 notes, F1–F6. The 4' C3, C4, and C5 are not half
  the long 8'. No short-8' length is stored. The same density on the
  two C3 speaking lengths yields two tensions, and the interval that
  holds the 4' refuses the 8'.
- The classical guitar's scale is 650 mm and its body is 490 mm. Open
  strings are E2 A2 D3 G3 B3 E4, with a major third from G3 to B3 and
  fourths otherwise. Savarez's 146.79 Hz and 246.88 Hz stay off the
  course. A caller density tunes the scale; the tension of that density
  on the 490 mm body is refused. A pluck's modal sum stays within the
  truncation bound, and a fingered stop keeps the open tension.
- The electric guitar's scale is 25.5 in, not 648 mm and not 650 mm.
  EXL110's printed pounds are kept only when the caller's linear density
  produces that tension within 0.001 lb. The same density at 650 mm
  misses the window. A pluck stays within the truncation bound, and a
  fingered stop keeps the tension.
- The piano bank is D#1, C2, F3, C#5, and G6, with the Table III
  lengths and tensions. Each ideal fundamental is within 0.5% of the
  printed \(f_0\). C#5 is not retuned to the A440 adapter. The stiff
  partial sits above the flexible fundamental. A hard hammer and a felt
  force use the existing laws, and A0, C4, and C8 are absent.
- Helmholtz stick and slip velocities, their durations, the bridge-force
  slope and flyback, and a zero-mean bow displacement match the identities
  above. A normal force above Schelleng's maximum is refused. The cello's
  A string, at the same β and bow speed, has a different impedance and a
  longer period than the violin's A string.
- Schelleng's minimum is \(Z^2|v_b|/(2R\beta^2(\mu_s-\mu_d))\), and
  \(f_{\max}/f_{\min}=4\beta R/Z\). \(R\) is an argument. Doubling \(R\)
  halves the minimum. A force below it is refused when \(R\) is supplied,
  and the same stroke without \(R\) is still the rigid-bridge motion. An
  up-bow has the same minimum. The cello's A string, at the same \(R\),
  does not.
- A wound string's \(\kappa\) matches the core-only Table III formula.
  Doubling Young's modulus doubles \(B\). Doubling the winding factor at
  fixed wave speed halves \(B\). Doubling the core diameter at fixed
  linear density multiplies \(B\) by 16. A core heavier than the string
  is refused. A violin course has no stored core: two caller diameters
  produce two values of \(B\).
- A resistive moving bridge keeps \(nc/(2L)\) when \(R>Z\), with decay
  \((c/L)\operatorname{artanh}(Z/R)\) and reflection \((Z-R)/(Z+R)\).
  \(R<Z\) keeps the fixed-free pitches. \(R=Z\) is refused. Doubling the
  length halves the frequency and the decay. One high-Q body mode, used as
  the termination, matches the lossless coupled-string root, and a second
  caller mode moves that root. \(\operatorname{Re}(1/Y)\) at the mode's
  own resonance equals its \(r\), and Schelleng's minimum accepts that
  value. No instrument resistance is stored.
- The stick-slip step has \(v=v_h+f/(2Z)\), and each rigid end returns
  the outgoing wave with a factor \(-1\) after the caller's delay. From
  rest, a bow whose static limit sits between \(2Z|v_b|\) and \(4Z|v_b|\)
  sticks until the bridge echo and then slips. A heavier bow is still
  stuck at that echo. In the band \(\mu_d N<|f|\le\mu_s N\), a stuck
  sample stays stuck and a slipping sample stays at \(\mu_d N\). A force
  above Schelleng's maximum is refused by the steady stroke and is still
  stepped. No bridge resistance is applied.
- One body mode has stiffness \(m\omega_0^2\), damping \(m\omega_0/Q\), and
  mobility \(1/r\) exactly at resonance, with a positive imaginary part
  below resonance and a negative imaginary part above it. Driven by the
  Helmholtz sawtooth, its velocity and displacement match the independent
  particular solution of \(m\ddot x + r\dot x + kx = F\) harmonic by
  harmonic. Mean damper power matches mean input power. The same mode on
  the lowest string of each of the four instruments sees a different
  flyback, so the first velocity coefficient changes with the instrument.
- Several caller-supplied modes add. The mobility of the table equals the
  sum of the one-mode mobilities, a one-mode table matches that mode, and
  doubling every modal mass halves the sum. At one mode's resonance the
  imaginary part is the other modes' imaginary part. Below every resonance
  the imaginary part is positive; above every resonance it is negative and
  the magnitude follows \(\sum 1/(m_k\omega)\). The Helmholtz velocity and
  displacement of the table equal the sum of the one-mode responses, and
  the sum of the modal damper powers matches the input power through
  \(\operatorname{Re} Y\). An empty table is refused. No A0 or B1 frequency
  is stored.
- A cavity Helmholtz frequency is \(f=(c/2\pi)\sqrt{A/(VL)}\), with
  \(M_a=\rho L/A\), \(C_a=V/(\rho c^2)\), and \(\omega=1/\sqrt{M_a C_a}\).
  \(c\) and \(\rho\) are the dry-air model. Doubling the opening area
  multiplies \(f\) by \(\sqrt{2}\) and halves \(M_a\). Doubling the volume
  or the neck divides \(f\) by \(\sqrt{2}\). Doubling pressure leaves \(f\)
  and \(c\) unchanged and swaps a factor of two between \(M_a\) and \(C_a\).
  Quadrupling temperature doubles \(c\) and \(f\), quarters \(M_a\) because
  \(\rho\propto 1/T\), and leaves \(C_a\) unchanged because
  \(\rho c^2=\gamma p\). A non-positive area, volume, or
  neck is refused. The violin's body length is not the neck. The frequency
  is not written into a body mode.
- Two modes with a mutual stiffness \(K\) have coupled frequencies from
  \((k_1-\omega^2 m_1)(k_2-\omega^2 m_2)-K^2=0\). \(K=0\) returns the two
  resonances. The sign of \(K\) does not move them. Their squared angular
  frequencies still add to \(\omega_1^2+\omega_2^2\), and a nonzero \(K\)
  pushes the lower root down and the upper root up. Identical modes split
  as \(\sqrt{\omega_0^2\pm|K|/m}\). \(K^2\ge k_1 k_2\) is refused. No A0
  or B1 frequency is stored, and the driving-point sum stays diagonal.
- A compact monopole has pressure
  \(P=j\omega\rho U e^{-jkr}/(4\pi r)\). \(|P|\) is \(\omega\rho|U|/(4\pi r)\).
  Doubling the distance halves \(|P|\). Doubling the frequency doubles
  \(|P|\). A radiating area times a normal velocity is the same \(U\).
  Doubling the air pressure doubles \(P\). Quadrupling temperature
  quarters \(|P|\) and halves \(k\), because \(\rho\propto 1/T\) and
  \(c\propto\sqrt{T}\). A steady volume velocity radiates nothing. A
  non-positive distance is refused. The violin's body length is not the
  distance, and no radiating area is stored.
- A hard hammer's contact velocity is \(v_0 e^{-t/\tau}\) with
  \(\tau = m/(2Z)\) and \(Z=\mu c\), the force is \(2Zv\), and the
  contact displacement is the integral of that velocity. The impulse
  equals the hammer's lost momentum. The same mass and speed on the
  bass's lowest string is a different impedance, so the initial force
  and the time constant are not the violin's. At and after the first
  reflection the formula is refused. A non-positive speed, a non-positive
  mass, and a strike on the nut or the bridge are refused. The direct
  wave is zero ahead of the outgoing front and matches the retarded
  contact displacement behind it, until a reflection arrives.
- A reed's opening is \(S_0(1-\Delta P/P_M)\) up to closure and zero after
  it. The volume flow is \(S\sqrt{2\Delta P/\rho}\), with \(\rho\) the
  ideal-gas density at the caller's pressure and temperature. The flow
  is largest at \(\Delta P=P_M/3\). A negative pressure difference is
  refused. A warmer air state at the same slit has a lower density and a
  different peak flow.
- A clarinet is that characteristic on an open cylinder. The reflection is
  \(-1\) and the fundamental is \(c/(4L)\). The input impedance is a pole
  at that fundamental and at three times it, and it is finite at twice it.
  The reed density equals the bore density. A warmer temperature raises
  the fundamental and the peak flow. Doubling the length halves the
  fundamental. Doubling the area halves \(Z_c\). Stiffness is
  \(P_M A_{\mathrm{tip}}/y_{\mathrm{close}}\). At \(\Delta P=P_M/3\) the
  reed flow is the characteristic maximum and the bore flow is
  \((p^+-p^-)/Z_c\). A negative difference is refused. Zero length, a
  negative area, and a negative damping are refused. The only length
  stored is the caller's.
- A flute of the same length and air speaks an octave above that
  clarinet cylinder. Its modes are \(nc/(2L)\), including the octave.
  The bore impedance \(j Z_c\tan(kL)\) is zero on those modes and a pole
  on the quarter-waves. Jet speed is \(\sqrt{2\Delta P/\rho}\). Regime
  \(m\) is the odd half-period travel time \((2m-1)\sigma U/(2W)\), and
  it is not the pipe fundamental. Doubling the length halves the pipe
  frequency and leaves the jet speed unchanged. Doubling the blowing
  pressure leaves the pipe frequency unchanged and slows the jet.
  Doubling the bore area halves \(Z_c\). The jet flow into the bore and
  the bore's own flow are reported separately. A negative overpressure,
  a convection ratio outside \((0,1]\), and a zero length are refused.
- A trumpet of the same length and air has the clarinet cylinder's
  \(c/(4L)\), and it rejects the octave the flute keeps. A positive cup
  volume leaves the lip impedance finite at that quarter-wave pole,
  equal to \(-j/(\omega C_a)\) with \(C_a=V/(\rho c^2)\). A zero cup
  restores the pole. Doubling the cup halves that reactance. Doubling
  the air pressure leaves the bore frequency unchanged, halves \(C_a\),
  and slows the lip jet. The lip area grows with \(\Delta P\) until
  \(S_{\max}\) and the flow does not return to zero there. Lip stiffness
  is the caller's embouchure stiffness. The lip flow and the bore flow
  are reported separately. A negative overpressure, a maximum opening
  below the rest opening, and a zero length are refused.
- A saxophone of the same length and air keeps that inward reed and
  replaces the cylinder with a truncated cone. The throat impedance is
  \(j Z_c (k x_0 \sin(kL))/(\sin(kL)+k x_0 \cos(kL))\). Its poles solve
  \(\tan(kL)=-k x_0\), so the fundamental lies strictly between
  \(c/(4L)\) and \(c/(2L)\). With the apex equal to the length the
  second partial is neither twice nor three times the first. A small
  apex approaches \(c/(2(L+x_0))\) and a harmonic ratio of 2. A large
  apex approaches the clarinet quarter-waves and a ratio of 3, and the
  reactance approaches \(j Z_c\tan(kL)\). The bell area is
  \(S((x_0+L)/x_0)^2\). At \(c/(4L)\) the cone impedance is finite.
  Doubling the length without the apex does not halve the fundamental;
  doubling both does. The reed flow peaks at \(P_M/3\) and is zero at
  closure, where an outward lip of the same air is still open. A zero
  apex, a zero length, and a negative pressure difference are refused.

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
Schelleng's minimum, same section eq. (9), is

\[
f_{\min}=\frac{Z^2|v_b|}{2R\beta^2(\mu_s-\mu_d)},\qquad
\frac{f_{\max}}{f_{\min}}=\frac{4\beta R}{Z}.
\]

\(R\) is the caller's bridge resistance. It is not read off a mode, and no
violin, viola, cello, or bass value is stored. Supplying \(R\) refuses a
bow lighter than \(f_{\min}\). Omitting \(R\) leaves the rigid-bridge
stroke, which does not know a minimum. The string is not re-solved with a
moving end, so this is not a wolf note. Friction coefficients are
arguments, not a hidden rosin table.

`captureStickSlip` is the separate time step (Woodhouse, Euphonics 9.2.1).
The bow force and the string velocity meet where \(v=v_h+f/(2Z)\). Each
end reflection is a delay and a factor \(-1\) (Euphonics 9.5.1, rigid
termination). Round-trip delays in samples are arguments, and their ratio
is \(\beta\). The string starts at rest, so the first sample uses static
friction. It keeps sticking while the required force stays inside
\(\mu_s N\). Once it is slipping, \(|f|=\mu_d N\) until that required
force falls to \(\mu_d N\). A normal force above Schelleng's maximum is
still stepped: the steady stroke is what refuses it. No falling rosin
curve is stored. Friedlander and Keller showed that periodic motion on a
lossless rigid string is unstable, so a long run is not a Helmholtz tone.
No dashpot is applied, because that would be a bridge impedance, and none
is stored.

Not in this bow: bow-hair width, torsion, thermal rosin, a rounded corner,
and any body filter on the bridge force. The body-mode sum below is
applied after the steady force is known. It does not change the bow.

## Body

`body.h` is the modal driving-point admittance (Woodhouse, "On the 'bridge
hill' of the violin", eq. (4)), one term per mode and the sum of a
caller-supplied table:

\[
Y(\omega) = \sum_k \frac{j\omega}{m_k\left(\omega_k^2 - \omega^2 + j\omega\omega_k/Q_k\right)},
\quad Q = \frac{1}{2\zeta}.
\]

The mode-shape factor at the driven point is absorbed into the effective
mass \(m_k\). Each resonance, \(Q\), and \(m\) is an argument. None is
derived from body length, and there is no stored A0 or B1, and no measured
admittance table, for the violin, viola, cello, or double bass. An empty
table is refused. A table of one mode is that single term: at its
resonance the mobility is the real value \(1/r\) with \(r = m\omega_0/Q\).
With more than one mode, the mobility at one resonance keeps the imaginary
part of the other terms. Doubling every modal mass halves \(Y\). Far below
every resonance the imaginary part is positive; far above every resonance
it is negative and \(|Y|\) follows \(\sum_k 1/(m_k\omega)\).

Each term is evaluated as \((\omega d_i + j\omega d_r)/(m|D|^2)\). The sum
adds those values. There is no complex division, including at a resonance
where \(d_r = 0\).

The steady bridge-foot velocity and displacement are the response to the
Helmholtz sawtooth, summed from its Fourier coefficients
\(c_n = -j\,\Delta/(2\pi n)\) (\(\Delta\) is the flyback) through the summed
mobility. That response equals the sum of the one-mode responses. That
drive is still open-loop: it does not rewrite the sawtooth.

`movingBridgeModes` does re-solve the string. The nut stays fixed. The
bridge end satisfies

\[
j\sin(\omega L/c)+YZ\cos(\omega L/c)=0,\qquad Z=\mu c,
\]

with \(Y\) the summed mobility. A caller resistance uses \(Y=1/R\). For
\(R>Z\) the pitches stay \(nc/(2L)\) and the decay is
\((c/L)\operatorname{artanh}(Z/R)\). For \(R<Z\) the pitches are the
fixed-free series \((2n-1)c/(4L)\) and the decay is
\((c/L)\operatorname{artanh}(R/Z)\). \(R=Z\) is matched and is refused.
The velocity reflection is \((Z-R)/(Z+R)\). A mode table uses the same
equation; one high-Q oscillator agrees with the lossless coupled string.
`bridgeResistance` is \(\operatorname{Re}(1/Y)\) at a caller frequency.
At one mode's own resonance it equals that mode's \(r\), and that value
may be passed to Schelleng's minimum. No instrument resistance is stored.
A double-slip wolf cycle is not stepped.

`coupleBodyModes` is the missing mutual term: two caller-supplied
modes and one stiffness \(K\) off the diagonal. The driving-point sum
itself stays diagonal. \(K=0\) returns the uncoupled resonances. A nonzero
\(K\) pushes one root down and the other up, and
\(\omega_-^2+\omega_+^2=\omega_1^2+\omega_2^2\). \(K^2\ge k_1 k_2\) is not
a pair of positive frequencies. No A0 or B1 number is stored, and the
cavity's acoustic mass is not used as a mechanical modal mass.

`makeCavityHelmholtz` is Rayleigh's resonator, not a measured A0:

\[
M_a=\frac{\rho L}{A},\qquad C_a=\frac{V}{\rho c^2},\qquad
\omega=\frac{1}{\sqrt{M_a C_a}}=c\sqrt{\frac{A}{VL}},\qquad
f=\frac{\omega}{2\pi}.
\]

\(A\) is the caller's total f-hole area, \(V\) the cavity volume, and \(L\)
the effective neck length. An end correction is already inside \(L\) if the
caller put one there; the function does not add \(0.6r\) or any other
correction. \(c=\sqrt{\gamma RT}\) and \(\rho=p/(RT)\) are the dry-air
model, so pressure and temperature are arguments the way they are on a
reed bore. Frequency does not depend on pressure, because \(\rho\) cancels.
\(C_a\) does not depend on temperature, because \(\rho c^2=\gamma p\). No
violin, viola, cello, or bass area, volume, or neck is stored, and the
body length is not substituted for any of the three. The frequency is not
inserted into the modal sum: that would need a modal mass and a \(Q\).

Schelleng's minimum is `schellengMinimumForce` once the caller supplies
\(R\). Each mode carries its
resonant resistance \(r = m\omega_0/Q\), the reciprocal of that mode's own
real mobility at \(\omega_0\). The summed mobility at that frequency is
not \(1/r\), and neither number is the bridge resistance the string sees.
Neither is substituted into the bow.

`compactMonopolePressure` is the spherical wave of a compact source,

\[
P=\frac{j\omega\rho}{4\pi r}U\,e^{-jkr},\qquad k=\frac{\omega}{c}.
\]

\(U\) is the caller's complex volume velocity, or the caller's radiating
area times a normal velocity. Distance is an argument. \(\rho\) and \(c\)
are the dry-air model. The factor \(j e^{-jkr}\) is sine and cosine, so
the pressure is not a complex division. No violin, viola, cello, or bass
radiating area is stored, and the body length is not the distance. A
steady volume velocity has \(\omega=0\) and radiates no pressure.

`physics/` does not include `instruments/`.

## Reed

`reed.h` is the elementary single-reed characteristic of Dalmont, Gilbert,
and Ollivier, JASA 114(4), 2253–2262 (2003), eqs. (1) and (5). Effective
jet area \(S_0\), closure pressure \(P_M\), and the static air state are
arguments. Density is \(\rho=p/(RT)\) from `airDensity`, not a stored
1.2 kg/m³. For \(0\le\Delta P\le P_M\),

\[
S=S_0\left(1-\frac{\Delta P}{P_M}\right),\qquad
U=S\sqrt{\frac{2\Delta P}{\rho}}.
\]

Above \(P_M\) the reed is on the lay and both area and flow are zero. The
flow is largest at \(\Delta P=P_M/3\),

\[
U_{\max}=\frac{2}{3}S_0\sqrt{\frac{2P_M}{3\rho}}.
\]

\(\Delta P<0\) is `DomainError`. Equation (5) is the blowing direction.

\(S_0\) is already the effective jet area. A second vena-contracta
coefficient is not applied. Dalmont's quasi-static use assumes the
playing frequency is small beside the reed resonance, and the paper does
not give a numerical ratio at which that assumption fails, so no ratio
is invented. A viscous cutoff would need a viscosity and the channel
width separately from \(S_0\); that cutoff is not applied.

### Bore

`makeReedBore` is a cylinder. Length, cross-section, static pressure,
temperature, and the far end are arguments. \(c=\sqrt{\gamma RT}\) and
\(\rho=p/(RT)\) come from the air already in `physics/`. Acoustic
impedance \(Z_c=\rho c/S\). The round trip is \(2L/c\).

- Open far end: pressure reflection \(-1\), fundamental \(c/(4L)\),
  \(Z=j Z_c\tan(kL)\). `RangeError` when \(\cos(kL)=0\).
- Closed far end: pressure reflection \(+1\), fundamental \(c/(2L)\),
  \(Z=-j Z_c\cot(kL)\). `RangeError` when \(\sin(kL)=0\), including
  steady pressure.

The impedance is built as a pure-imaginary complex. It is not a quotient
of two complexes. `boreWaveSplit` takes the outgoing pressure and the
outgoing pressure from one round trip earlier. \(p^-=R\,p^+_{\text{delayed}}\),
\(p=p^++p^-\), and the volume flow into the bore is \((p^+-p^-)/Z_c\).
There is no stored time step. The mouth-to-bore \(\Delta P\) that enters
the reed characteristic is the caller's mouth pressure minus this mouth
pressure. `clarinet.h` wires an open bore to this characteristic. This
header stores no bore length, and none is stored on the bowed-string,
harp, or harpsichord factories.

### Reed inertia

`makeReedInertia` takes a mass, a tip area, a closing displacement, and
a damping. Damping is an argument, including zero. Stiffness is derived
so the static equilibrium sits on Dalmont's line,

\[
k=\frac{P_M A_{\text{tip}}}{y_{\text{close}}},\qquad
f=\frac{1}{2\pi}\sqrt{\frac{k}{m}}.
\]

For \(0\le\Delta P\le P_M\) the equilibrium displacement is
\(y=y_{\text{close}}\Delta P/P_M\), and the spring acceleration there is
zero. \(\Delta P<0\) and \(\Delta P>P_M\) are `DomainError` for that
equilibrium. The acceleration \((A\Delta P-ky-rv)/m\) stays the linear
spring past the lay; there is no contact solver. Opening against
displacement is the same straight line, zero on and beyond the lay, and
larger than \(S_0\) if the reed is pulled back through \(y=0\). No
reed-mass number is stored on a bowed-string factory.

## Clarinet

`clarinet.h` wires the quasi-static characteristic, an open cylinder, and
reed inertia. A clarinet bore is open at the bell, so the far end is
fixed to `BoreFarEnd::Open`: reflection \(-1\), fundamental \(c/(4L)\),
input impedance \(j Z_c\tan(kL)\). A closed far end is not offered. The
next resonance of this cylinder is

\[
f_3=\frac{3c}{4L},
\]

an impedance pole. Even multiples of the fundamental are finite. That
twelfth is the cylinder's next pole. It is not a register key.
`clarinetCylinderMode` is `stoppedPipeMode` on the built length and sound
speed, so an even index is `DomainError`.

Length, cross-section, \(S_0\), \(P_M\), reed mass, tip area, closing
displacement, and damping are arguments. Damping may be zero. Dalmont,
Gilbert, and Ollivier, Table 1, summarise embouchure ranges (rest
opening, beating pressure, surfacic stiffness, jet width, maximum flow).
That table does not print a bore length or one effective jet area, so no
midpoint of it is stored, and the ranges are not acceptance windows.

The characteristic, the bore, and the inertia share one static pressure
and temperature. The reed density therefore equals the bore density.
`clarinetMouth` is one sample. The caller supplies mouth pressure, the
outgoing bore pressure, the outgoing pressure from one round trip
earlier, and the reed displacement and velocity. \(\Delta P\) is mouth
pressure minus the bore mouth pressure. Reed flow is the Bernoulli jet,
bore flow is \((p^+-p^-)/Z_c\), equilibrium displacement is the static
line, and tip acceleration is the linear spring. The two flows are
reported and are not solved into one note. No time step is stored.
\(\Delta P<0\) and \(\Delta P>P_M\) are `DomainError` because the
equilibrium is only the lay interval. A displacement past the lay is
still the linear spring while \(\Delta P\) is on the characteristic.

Not in this model: tone holes, a bell correction, a register key, and a
viscous channel. The viscous channel still needs a viscosity and a
channel width separate from \(S_0\).

## Flute

`flute.h` is an open-open cylinder and an embouchure jet. It does not
include `clarinet.h`. A clarinet of length \(L\) speaks at \(c/(4L)\) and
rejects the octave. A flute of the same length speaks at

\[
f_n=\frac{nc}{2L}
\]

for every positive integer \(n\), and the fundamental period equals the
round trip \(2L/c\). The far-end reflection is \(-1\). The bore
impedance at the embouchure is \(j Z_c\tan(kL)\), the same formula as an
open far end, but the flute uses its zeros. The odd quarter-waves, where
that impedance poles, are `RangeError` and are not flute modes. No end
correction is added.

The jet is not a reed. Centerline speed and volume flow are

\[
U=\sqrt{\frac{2\Delta P}{\rho}},\qquad Q=S_j U,
\]

with \(\rho=p/(RT)\). \(S_j\) is not the bore area. \(\Delta P<0\) is
`DomainError`. \(\Delta P=0\) is a jet at rest: speed and flow are zero,
and the travel time is `DomainError`.

A disturbance convects at \(u=\sigma U\) across the flue-to-edge distance
\(W\). Regime \(m\ge 1\) is an odd number of half periods,

\[
f_m=\frac{(2m-1)\sigma U}{2W}.
\]

\(\sigma\in(0,1]\) is an argument. Published convection ratios are not
one constant, so none is stored. These regime frequencies are not the
pipe modes. `fluteEmbouchure` reports the jet flow, the fraction of that
jet on the bore side of the edge, and the bore flow \((p^+-p^-)/Z_c\)
from the caller's outgoing pressures. The two flows are not solved into
one note. No time step is stored.

The jet is a top-hat of thickness \(h\). The bore fraction is the
overlap of \([y-h/2,\, y+h/2]\) with the bore side of the edge: one half
at \(y=0\), all of the jet for \(y\ge h/2\), and none for \(y\le -h/2\).
\(h\) is not derived from \(S_j\).

Not in this model: tone holes, an end correction, embouchure-hole
radiation, a viscous wall layer, and the spatial growth of the jet.

## Trumpet

`trumpet.h` is a lip-driven cylinder and a mouthpiece cup. It does not
include `clarinet.h` or `flute.h`. The bare bore is open at the bell.
Its modes are the odd quarter-waves

\[
f_n=\frac{(2n-1)c}{4L},
\]

and an even index is `DomainError`. The throat impedance is
\(j Z_c\tan(kL)\), so those modes are poles of the bare bore. A flute of
the same length speaks at \(c/(2L)\), where this bore is a short. The
round trip is \(2L/c\); the fundamental period is twice that.

The mouthpiece cup, volume \(V\), is a compliance \(C_a=V/(\rho c^2)\) in
parallel with the bore:

\[
Z=\frac{1}{1/Z_{\mathrm{bore}}+j\omega C_a}.
\]

\(C_a\) does not depend on temperature, because \(\rho c^2=\gamma p\).
Doubling the static pressure halves \(C_a\) and leaves the bore
frequencies unchanged. A zero cup returns the bare bore. At a
quarter-wave pole, \(Z_{\mathrm{bore}}\) is infinite and a positive cup
leaves \(Z=-j/(\omega C_a)\), which is finite. The imaginary part is
\(-1/D\) with \(D=\omega C_a-1/X\) and \(X=Z_c\tan(kL)\). It is not a
quotient of two complexes. No bell flare is added, so the peaks are not
pulled onto \(nc/(2L)\).

The lip is outward-striking. Blowing pressure opens it:

\[
y=\frac{A\Delta P}{k},\qquad
S=\min\left(S_{\max},\, S_0+w y\right),\qquad
U=S\sqrt{\frac{2\Delta P}{\rho}}.
\]

\(k\) is the caller's embouchure stiffness, and the resonance is
\((1/2\pi)\sqrt{k/m}\). It is not the clarinet lay stiffness
\(P_M A/y_{\mathrm{close}}\). Area grows with \(\Delta P\) until
\(S_{\max}\). Past that cap the jet speed still grows, so the flow does
not return to zero. \(\Delta P<0\) is `DomainError` for the opening, the
flow, and the equilibrium. The acceleration stays the linear spring past
\(S_{\max}\). `trumpetEmbouchure` reports the lip flow and the bore flow
\((p^+-p^-)/Z_c\) and does not solve them into a note.

Not in this model: a bell flare, valves, a crook, and a stored
mouthpiece or sounding length.

## Saxophone

`saxophone.h` is Dalmont's inward reed on a truncated cone. It includes
`reed.h` and does not include `clarinet.h`. The characteristic and the
inertia are the clarinet's: area \(S_0(1-\Delta P/P_M)\), flow
\(S\sqrt{2\Delta P/\rho}\), peak at \(P_M/3\), and stiffness
\(P_M A_{\mathrm{tip}}/y_{\mathrm{close}}\). The bore is not that
cylinder.

The throat is a distance \(x_0\) from the virtual apex and the open end
is a length \(L\) further on. A spherical standing wave that vanishes at
the open end has throat impedance

\[
Z=j Z_c\frac{k x_0\sin(kL)}{\sin(kL)+k x_0\cos(kL)},
\]

with \(Z_c=\rho c/S_{\mathrm{throat}}\). Poles are the positive roots of
\(\tan(kL)=-k x_0\), one in each interval \(((n-1/2)\pi,\, n\pi)\). An
even index is a resonance. A clarinet of the same length refuses it.
As \(x_0/L\) grows, those roots approach the odd quarter-waves
\((2n-1)c/(4L)\) and \(Z\) approaches \(j Z_c\tan(kL)\). As \(x_0/L\)
shrinks, they approach \(nc/(2L)\), the complete-cone series, which has
the same pitches as an open pipe of length \(L\). A moderate truncation
is inharmonic. The first root sits strictly between \(c/(4L)\) and
\(c/(2L)\). At \(c/(2L)\) this impedance is zero for any \(x_0>0\). At
\(c/(4L)\) it is finite, \(j Z_c k x_0\), where the clarinet bore is a
pole.

The bell area of the straight cone is
\(S_{\mathrm{throat}}((x_0+L)/x_0)^2\). It is fixed by \(L\) and \(x_0\).
The open end is an ideal pressure release. No radiation impedance and
no end correction are added. \(L/c\) is the travel time of the spherical
wave along the physical cone. It is not stored as a resonance period,
because a truncated cone is not a constant-delay cylinder. The caller
passes \(\Delta P\) into the reed. The reed flow and the cone impedance
are reported separately and are not solved into a note.

Length, apex distance, throat area, and the reed parameters are
arguments. No alto or tenor length is stored. A mouthpiece cavity that
has already been absorbed into an effective apex distance is that
argument; this layer does not add a second volume. \(x_0=0\) is the
apex, where the throat area would be zero, and it is `InvalidArgument`.

Not in this model: a tone-hole lattice, a separate mouthpiece volume, a
radiation load, and an end correction.

## Hammer

`hammer.h` is the hard narrow hammer of Smith, *Physical Audio Signal
Processing*, "The Mass-String Collision", cut off where Hall's finite
string stops being that one wave. One-way impedance \(Z=\mu c\), incoming
speed \(v_0\), hammer mass \(m\), contact starting at \(t=0\):

\[
v(t)=v_0 e^{-2Zt/m},\qquad F(t)=2Zv(t),\qquad
y(t)=v_0\tau(1-e^{-t/\tau}),\qquad \tau=\frac{m}{2Z}.
\]

Mass, speed, and the strike fraction are arguments. No felt table and no
hammer mass is stored on the violin, viola, cello, double bass, harp, or
harpsichord.

`hardHammerVelocity`, `hardHammerForce`, `hardHammerDisplacement`, and
the direct-wave copies stay refused at \(t \ge 2d/c\), where \(d\) is the
distance from the strike to the nearer end. A hard hammer on an infinite
string never rebounds, so those functions invent no contact-loss time.
Ahead of the outgoing front the string is still at rest. Behind it,
displacement and velocity are the contact history retarded by
\(|x-x_0|/c\). That copy is refused once a reflection from either end has
reached \(x\).

`reflectedHammerVelocity`, `reflectedHammerForce`, and
`reflectedHammerDisplacement` add the first fixed-end image from the nut
(\(t_{\mathrm{nut}}=2x/c\)) and from the bridge
(\(t_{\mathrm{bridge}}=2(L-x)/c\)). Velocity reflection at a fixed end is
\(-1\). For each delay \(t_d\) that has arrived,

\[
v(t)=v_0 e^{-t/\tau}+\sum v_0\frac{t_d-t}{\tau}e^{-(t-t_d)/\tau},\qquad
F=2Z\bigl(v+\sum v_0 e^{-(t-t_d)/\tau}\bigr).
\]

The sum is valid on \([0, 2 t_{\text{first}})\) and is `DomainError` at
and after that horizon, where the nearer end's second image arrives and
is not included. A centered strike adds both images. Velocity and
displacement are continuous at an arrival; the force jumps with the
returning step. This does not replace the hard-hammer cutoff.

Felt compliance is separate and instantaneous. \(K>0\) and \(p>0\) are
arguments:

\[
F=K\xi^{p}\quad(\xi>0),\qquad F=0\quad(\xi\le 0).
\]

\(\xi\le 0\) is loss of contact, which is zero force, not the hard-hammer
`DomainError`. `feltHammerDerivative` uses the blow's mass and impedance
and the caller's state: \(a_h=-F/m\) and
\(v_{\text{string}}=F/(2Z)+v_{\text{incoming}}\). It does not take a time
step. Hammer width, shank rotation, and gravity are not in the model.

## Fingered stop

`fingerStop` takes one open course and a fraction \(\alpha\) in \((0,1)\),
measured from the nut toward the bridge, the same coordinate as a pluck.
The nut and the bridge are not stops. Geometry becomes the bridge-side
length \(L'=(1-\alpha)L\). Material \(\mu\) and tension \(T\) are copied,
so the wave speed is unchanged and

\[
T=\mu(2L'f')^2,\qquad f'=\frac{1}{2L'}\sqrt{\frac{T}{\mu}}=\frac{L}{L'}f.
\]

\(f'\) is `stringMode` of the shortened course. The published tension
interval stays the open course's; a tension outside it is `RangeError`.
The stopped course has no spelled pitch (`frequencyPrinted` is false).
The same \(\alpha\) on two instruments is the same frequency ratio and a
different hertz value, because \(L\) and the open pitch differ. No
fingerboard length and no fret table is stored. Factories still hold
open courses only.

## Drum kit

`drum.h` is the Yamaha Tour Custom series printed at
<https://usa.yamaha.com/products/musical_instruments/drums/ac_drums/drum_sets/tour_custom2/specs.html>.

| Piece | Model | Diameter × depth |
|---|---|---|
| Bass drum | TMB-2216 | 22″ × 16″ |
| Rack tom | TMT-1007 | 10″ × 7″ |
| Rack tom | TMT-1208 | 12″ × 8″ |
| Floor tom | TMF-1615 | 16″ × 15″ |
| Snare | TMS-1455 | 14″ × 5.5″ |
| Snare | TMS-1465 | 14″ × 6.5″ |

TMP2F4 is the bass, the two rack toms, and the floor tom. The two snares are components on the same page, not a column of TMP2F4. Shells are 5.6 mm, 6-ply maple. The bearing edge is 45° for the bass and the toms; the page does not print one for the snares, so those shells do not store an angle. Radii are half the printed diameter times the international inch, 0.0254 m. Shell depth and shell thickness are stored and are not the head radius.

Each head is a fixed circular membrane (Rayleigh): \(c=\sqrt{T/\sigma}\) and

\[
f_{mn}=\frac{\alpha_{mn}\,c}{2\pi a},
\]

where \(\alpha_{mn}\) is the n-th positive zero of \(J_m\), computed by bisection. The (0,1) and (1,1) zeros are the usual 2.4048… and 3.8317…. Mode ratios are \(\alpha_{mn}/\alpha_{01}\) and do not depend on tension. `tuneMembraneHead` takes an areal-density range and a tension-per-length range, because the page names Remo heads and does not print either number. Nominal density is the midpoint. Tension is

\[
T=\sigma\left(\frac{2\pi a f}{\alpha_{01}}\right)^2
\]

and construction fails if \(T\) leaves the caller's interval. The same requested pitch on the 22″ and 10″ heads needs tensions in the ratio of the squared diameters. The two snares share a diameter, so the same density and pitch produce the same tension; the 5.5″ and 6.5″ depths do not enter \(f\).

A strike is an impulse of momentum \(P\) at a point inside the head. The rim is refused. The modal velocity coefficient is \(b=(P/\sigma)\varphi/N\) on \(\varphi=J_m(\alpha r/a)\cos m(\theta-\theta_s)\). A center strike leaves every \(m>0\) coefficient at zero. Displacement starts at zero and the rim stays at zero. Felt contact is `feltContactRate`: \(F=K\xi^p\) and \(a=-F/m\). The string driving-point law \(F/(2Z)\) is not applied to a head.

Not evaluated, because the page does not give the numbers: snare-wire mass and tension, the air in the shell, coupling of the batter head to the resonant head, and a bass-drum port.

## Cymbals

`cymbal.h` is the SABIAN XSR Complete Set XSR5006B
(<https://sabian.com/product/xsr5006b-xsr-complete-set/>): 10″ splash, 14″ hi-hats, 16″ and 18″ fast crashes, 18″ china, 20″ ride. Splash and china are in that sentence, so they are separate plates. The alloy is named B20. The page does not print thickness, density, Young's modulus, Poisson's ratio, or a frequency. A reviewer's "fairly thin" is not a thickness.

A cymbal is a free Kirchhoff plate, not a membrane. With bending stiffness \(D=Eh^3/(12(1-\nu^2))\),

\[
f=\frac{\lambda^2 h}{2\pi a^2}\sqrt{\frac{E}{12\rho(1-\nu^2)}}.
\]

\(\lambda\) is a positive root of the free-edge moment and shear determinant, computed for the caller's \(\nu\in(-1,1/2)\). The rigid-body root is not returned. The lowest elastic mode has two nodal diameters. Frequency scales as \(h/a^2\) and as \(\sqrt{E/\rho}\). The 18″ crash and the 18″ china have the same ideal-plate spectrum: the china's flange is not given a height, so none is invented.

The hi-hat is two 14″ plates. No clutch gap is printed, and the plates are not coupled by an invented contact stiffness. A strike is an impulse on one plate. The edge is part of a free plate, so it may be struck; it is not held at zero the way a drum rim is. Felt uses the same `feltContactRate` and not the string impedance.

## Wound string

`wound.h` attaches hinged bending stiffness to one course. The convention
is Chabassier, Chaigne, and Joly, JASA 134(1) (2013), Table III, the same
one `pianoString` uses: \(I=\pi d^4/64\) from the core, mass from the
course, \(\kappa=(d/4)\sqrt{E/(\rho F)}\), and \(F=\mu/(\rho A_{\mathrm{core}})\).
Partial \(n\) is \(n f_0\sqrt{1+Bn^2}\) with \(f_0=c/(2L)\). Doubling \(E\)
doubles \(B\). Doubling \(F\) at fixed wave speed halves \(B\). Doubling
the core diameter at fixed \(\mu\) multiplies \(B\) by 16 when the wider
core is still no heavier than the string.

\(F\) below 1 by more than rounding is `DomainError`. That is the result
of passing an outer diameter as a core. \(F=1\) is a plain core and is
accepted; a one-ulp miss of \(\rho A\) is still that plain core. Core diameter, Young's
modulus, and core density are arguments. Young's 1952 specific modulus is
plain wire and is not substituted. Chabassier's steel constants are not
substituted either; a test may pass them, and they are not a violin,
viola, cello, bass, or harp core. Le Carrou's printed diameters are outer
diameters and are not read. No instrument stores a core.

## Still planned

Images after the first from each hammer end are not in this layer. A
viscous reed channel, clarinet tone holes, a bell correction, and a
register key are not in this layer. A flute end correction, tone holes,
embouchure-hole radiation, and the spatial growth of the jet are not in
this layer. A trumpet bell flare and valves are not in this layer. A
saxophone tone-hole lattice, a separate mouthpiece volume, a radiation
load, and an end correction are not in this layer. The
stick-slip capture still uses rigid ends. Snare wires, shell air, a bass-drum port, and a hi-hat clutch
gap are not in the drum kit. A double-slip wolf cycle is not stepped. A
classical-guitar soundboard and a fret table are not in this layer. An
electric-guitar pickup and the other 83 piano keys are not in this layer.
