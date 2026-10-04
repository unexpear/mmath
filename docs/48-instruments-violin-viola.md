# Instruments — bowed strings, harp, harpsichord, bow, body, hammer, reed

Status: assembly layer for four bowed strings, a concert pedal harp, a
French double harpsichord, a steady Helmholtz bow on one ideal string,
one body mode driven by that bow's bridge force, a hard narrow hammer
valid until the first reflection returns, and a quasi-static single-reed
characteristic. Verified against the ideal-string law, Guettler's tables,
the harp and harpsichord measurements named below, the Helmholtz/Schelleng
identities, the single-mode admittance, the hard-hammer contact law, and
the reed flow law. Unvalidated as radiated sound: the body mode is
open-loop, and there is no measured bridge admittance and no far-field
pressure. The reed is not a sounding bore. The harp and the harpsichord
are not sounded through a soundboard model.

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
| `instruments/double_bass.h` | Orchestral E1 A1 D2 G2, vibrating length 1.060 m, body 1.110 m, sounding compass E1–B3 (written E2–B4) |
| `instruments/bow.h` | Steady Helmholtz stick/slip, bridge-force sawtooth, Schelleng maximum normal force |
| `instruments/body.h` | One driving-point body mode. Resonance, Q, and modal mass are arguments |
| `instruments/hammer.h` | Hard narrow hammer. Mass, speed, and strike position are arguments |
| `instruments/reed.h` | Quasi-static single reed. Effective area, closure pressure, and air state are arguments |
| `instruments/harp.h` | Camac Atlantide Prestige concert pedal harp, 47 strings, height 1.86 m |
| `instruments/harpsichord.h` | Goermans/Taskin 1764/1783–4 French double: 2×8′ + 4′, compass F1–F6 |

`physics/` does not include `instruments/`. The only music dependency is
the pitch→frequency adapter (`makePitch`), so open-string hertz are the
engine's A440 12-TET pitches rather than a second copy of that formula.

A pluck is an initial shape. A bow is not; `makeHelmholtzStroke` builds
the steady regime. A hammer is not either; `makeHardHammerBlow` is the
hard narrow hammer until the first reflection returns. A reed is not
either; `makeReedCharacteristic` is the quasi-static slit.
`realizeExcitation` still returns `DomainError` for a bow, a hammer, and
a reed, because that function only builds an initial displacement. The
harp is finger-plucked and the harpsichord is plucked by a jack. Neither
factory stores a bow, a hammer, or a reed. Peau de buffle is a jack
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
same way as to a violin, viola, or cello course. The body mode and the
hard hammer are the same functions for any of the four courses. The reed
does not take a course. None of the factories stores a resonance, a
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
- Helmholtz stick and slip velocities, their durations, the bridge-force
  slope and flyback, and a zero-mean bow displacement match the identities
  above. A normal force above Schelleng's maximum is refused. The cello's
  A string, at the same β and bow speed, has a different impedance and a
  longer period than the violin's A string.
- One body mode has stiffness \(m\omega_0^2\), damping \(m\omega_0/Q\), and
  mobility \(1/r\) exactly at resonance, with a positive imaginary part
  below resonance and a negative imaginary part above it. Driven by the
  Helmholtz sawtooth, its velocity and displacement match the independent
  particular solution of \(m\ddot x + r\dot x + kx = F\) harmonic by
  harmonic. Mean damper power matches mean input power. The same mode on
  the lowest string of each of the four instruments sees a different
  flyback, so the first velocity coefficient changes with the instrument.
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
rosin, a rounded corner, and any body filter on the bridge force. The
body mode below is applied after the force is known. It does not change
the bow.

## Body

`body.h` is one term of the modal driving-point admittance (Woodhouse,
"On the 'bridge hill' of the violin", eq. (4)):

\[
Y(\omega) = \frac{j\omega}{m\left(\omega_0^2 - \omega^2 + j\omega\omega_0/Q\right)},
\quad Q = \frac{1}{2\zeta}.
\]

The mode-shape factor at the driven point is absorbed into the effective
mass \(m\). Resonance frequency, \(Q\), and \(m\) are arguments. They are
not derived from body length, and there is no stored A0 or B1 for the
violin, viola, cello, or double bass: those are measurements this layer
does not have.

The steady bridge-foot velocity and displacement are the response to the
Helmholtz sawtooth, summed from its Fourier coefficients
\(c_n = -j\,\Delta/(2\pi n)\) (\(\Delta\) is the flyback). The string still
sees a rigid bridge. This is an open-loop drive: the body does not move
the string end, so wolf notes and other back-reaction are absent.

Schelleng's minimum bow force stays unevaluated. The mode does carry its
resonant resistance \(r = m\omega_0/Q\), which is \(1/|Y(\omega_0)|\), but
that number is not the bridge resistance the string sees, and it is not
substituted into the bow. Far-field pressure is not evaluated: it needs a
radiating area or a volume velocity, and a distance. The cavity Helmholtz
frequency is not evaluated: it needs f-hole area, cavity volume, and an
effective neck length. A0–B1 coupling needs a mutual term. None of those
constants is invented.

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
coefficient is not applied. The characteristic does not oscillate: a
played note needs a bore reflection function, or a length and a
cross-section, and neither is invented or taken from the bowed-string
factories. Reed mass is not an argument, so inertia and damping are
absent. Dalmont's quasi-static use assumes the playing frequency is small
beside the reed resonance, and the paper does not give a numerical ratio
at which that assumption fails, so no ratio is invented. A viscous
cutoff would need a viscosity and the channel width separately from
\(S_0\); that cutoff is not applied.

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
hammer mass is stored on the violin, viola, cello, or double bass.

The contact law is refused at \(t \ge 2d/c\), where \(d\) is the distance
from the strike to the nearer end: a reflection has returned, and this
slice does not sum Hall's later waves or decide when they throw the
hammer off. A hard hammer on an infinite string never rebounds, so no
contact-loss time is invented. Ahead of the outgoing front the string is
still at rest. Behind it, displacement and velocity are the contact
history retarded by \(|x-x_0|/c\). That copy is refused once a reflection
from either end has reached \(x\).

Felt compliance \(F=K\xi^{p}\) is not evaluated. \(K\) and \(p\) are not
universal constants, and this hard limit does not take them as arguments.
Hammer width, shank rotation, and gravity are not in the model.

## Still planned

A reed bore and reed inertia, felt compliance and the reflected-wave sum
for the hammer, a sum of measured body modes, cavity Helmholtz frequency,
A0–B1 coupling, far-field radiation, back-reaction of the body on the
string (and with it Schelleng's minimum bow force), fingered stopping as
a change of vibrating length, stiffness of wound strings, and a
time-stepped stick-slip capture. The Helmholtz stroke is the steady
regime only.
