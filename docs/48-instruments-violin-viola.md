# Instruments — bowed strings, harp, harpsichord, guitars, piano, drums, cymbals, clarinet, flute, trumpet, trombone, saxophone

Status: assembly layer for four bowed strings, a concert pedal harp, a
French double harpsichord, a six-string classical guitar, a six-string
electric guitar, a five-string concert-grand bank, a steady Helmholtz bow on one ideal string,
a sum of driving-point body modes driven by that bow's bridge force, a cavity Helmholtz frequency from caller area, volume, and neck, a hard narrow hammer,
the first fixed-end images of that hammer, a felt contact law, a
quasi-static single reed, a cylindrical bore, reed inertia, a clarinet
cylinder on that open bore, an open-open flute driven by an embouchure
jet, a lip-driven trumpet with a mouthpiece cup, that lip and cup on a
U-slide, and a single-reed truncated cone. A
fingered stop shortens one open course. A Yamaha Tour Custom kit and a
SABIAN XSR cymbal set add fixed circular membranes and free circular
plates. Verified against the ideal-string
law, Guettler's tables, the harp, harpsichord, classical-guitar,
electric-guitar, and Table III piano measurements named below, the Helmholtz/Schelleng identities, the rigid-end stick-slip step, the wound-string inharmonicity, the driving-point modal sum, the cavity Helmholtz frequency, the mutual-stiffness split, the compact-monopole pressure,
the hard-hammer contact law, the first-image sum, the felt power law, the
reed flow law, the open-cylinder clarinet, the open-pipe flute and its embouchure jet, the lip-driven trumpet, the trombone U-slide, the truncated-cone saxophone, the circular-membrane Bessel frequencies, and the free-plate
frequency law. Unvalidated as radiated sound: the body-mode sum is
open-loop, and there is no measured bridge admittance and no far-field
pressure. The bore is an ideal cylinder with the caller's length and
cross-section. The clarinet is that cylinder open at the bell, with the
reed and its inertia; tone holes, a bell, and a register key are absent.
The flute is an open-open pipe, fundamental \(c/(2L)\), driven by a
Bernoulli jet. It is not that clarinet. The trumpet is the quarter-wave
cylinder with a mouthpiece cup in parallel and an outward-striking lip.
The trombone is that lip and cup. Its air column is the closed length
plus twice the handslide travel. The saxophone uses the clarinet's inward reed on a truncated cone, so
its impedance poles are neither the quarter-wave series nor \(nc/(2L)\)
except in those limits of the apex distance. The harp and the harpsichord are
not sounded through a soundboard model. The classical guitar is six
ideal strings on the published scale; it has no soundboard either. The
electric guitar is the 25.5 in chart scale, not that classical body.
The piano bank is five Table III strings and has no soundboard either.

## Audible render

An instrument is done only when the physics that affects its sound is
stepped in time and that stepper is what a WAV file contains. A
single-instant mouth, a modal formula, or an envelope is not that
sound. `render/voice.h` pulls blocks from an `IVoice` into any
`IAudioSink`, including `WavSink`. Block size does not change the
samples. `WaveguideVoice` is one such voice: a tuned waveguide, read
at one rail node and then stepped. Its decay is the loop's loss
filter. It stores no instrument length.

`PluckedStringVoice` is an `IVoice` built from one course. The pluck
is the caller's position and height on that course. The resonator is
`designTunedLoop`: frequency-dependent loss and hinged stiffness are
arguments, and neither a T60 nor an inharmonicity coefficient is
stored. The listen fraction is one interior rail node, so a partial
with a node there drops out of the sample. `CoilVelocity` is the
transverse velocity at that node, \((u_n-u_{n-1})f_s\), which is the
open-circuit law of a magnetic pickup (Julius O. Smith, *Physical
Audio Signal Processing*, CCRMA,
https://ccrma.stanford.edu/jos/pasp/). Turns, magnet, and inductance
are not a gain. The same pluck is an `Fdtd1D` field; that grid's time
step is the CFL step, and the WAV voice is the tuned loop.

The courses that step are violin, viola, cello, and double-bass
pizzicato, one harp string, one harpsichord speaking length, one
classical-guitar string, and one electric-guitar string. The same four
bowed courses also step: `BowedStringVoice` is the Woodhouse stick-slip
junction on that tuned loop. While the bow is down the sample is the
bow-point velocity unless a body is attached. With a `BowBodyDrive` the
bridge force \(Z(2v_+-v_{\mathrm{bridge}})\) drives the caller's modes,
their velocity replaces the rigid reflection, and the sample is the
compact monopole of the bridge acceleration. After the caller lifts the
bow the junction is transparent, so the ring is `loopPole` loaded by
that bridge. A felt hammer steps the hinged
modes of one stiff string (`FeltHammerVoice`, Hunt–Crossley contact).
`HammeredStringVoice` is the same force on the stiff tuned loop, closed
on a modal soundboard: the bridge force \(Z(2v_+-v_{\mathrm{bridge}})\)
drives the caller's modes, their velocity replaces the rigid reflection,
and the sample is the compact monopole of the bridge acceleration.
A mallet steps one membrane mode or one free-plate mode. One valve
(`BoreValveVoice`) steps an inward reed, an outward lip, or a flute jet
against a cylindrical bore delay. A positive bore radius replaces the
constant far-end magnitude with the Levine–Schwinger unflanged load:
radius and end correction are arguments, and \(0.6133a\) is not stored.
`ConeValveVoice` is the spherical-wave cone, bilinear at the throat, so
a sealed mouth rings at \(\tan(kL)=-k x_0\). The same open end sits on
that cone. The listening sample of both valves is the radiated pressure
of the bell flow, \(p=\rho (dU_{\mathrm{bell}}/dt)/(4\pi r)\), smoothed
with a one-pole at \(\omega=c/a\) so the differentiator stops rising
past \(ka=1\). A steady flow does not radiate: that high-pass is what
removes DC. On the cone the bell flow is the throat flow times
\((x_0+L)/x_0\), because the spherical potential is conserved and the
bell area grows with the square of that ratio. The round trip is
\(f_s\cdot 2L/c\). The integer part is the delay line and the fraction
is a first-order Thiran section, folded into the end correction when
the far end is Levine–Schwinger so the fraction is not applied twice.
One sample per step is the Courant condition at speed \(c\).
`attackFrames` ramps the blowing pressure through a quick rise to the
threshold region, a linear creep across it, and the rest of the
published duration. `releaseFrames` stays a raised cosine. Zero is a
step. `tongueReleaseFrames` holds a reed or a lip shut until that
frame. A reed is then placed at the initial displacement. A lip stays
shut and the blowing pressure accelerates it. Zero leaves the valve
free from the first sample. The listener distance
omitted from `ValveBlow` is one metre, and the omitted jet aim is centered.

`SoundboardVoice` closes one plucked course on a bank of
body modes: the bridge force \(Z(v_+-v_-)\) drives each mode, the
bridge velocity is their sum, and that displacement replaces the rigid
reflection. The sample is the compact monopole
\(\rho A a(t-r/c)/(4\pi r)\). Clarinet, flute, saxophone, trumpet,
trombone, harp, harpsichord, and classical guitar render audio.
`ConeValveVoice` steps a positive cup compliance in parallel with the
throat, \(C\,dp/dt = U_{\mathrm{lip}}-U_{\mathrm{bore}}\), with
\(C=V/(\rho c^{2})\) from the trumpet. A zero cup leaves that junction
unchanged. `CavityDrumVoice`
diagonalizes the closed-shell compliance \(p=\rho c^{2}\Delta V/V\) on
the membrane modes and radiates the batter's outward volume
acceleration. The three toms render audio. Violin, viola, cello, and
double bass render audio through the bowed soundboard. The piano
renders audio through the hammered soundboard. The bass-drum port and
snare wires are not in those steppers.

| Instrument | Excitation | Resonator | Body | Radiation | Renders audio | Full instrument | Remaining gap |
|---|---|---|---|---|---|---|---|
| Violin | Bow friction and pizzicato both step | Lossy tuned waveguide; stiffness is a caller argument | Bow closes on a modal soundboard; bridge force is the drive | Compact monopole of the bowed bridge acceleration | yes | no | Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored |
| Viola | Bow friction and pizzicato both step | Lossy tuned waveguide; stiffness is a caller argument | Bow closes on a modal soundboard; bridge force is the drive | Compact monopole of the bowed bridge acceleration | yes | no | Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored |
| Cello | Bow friction and pizzicato both step | Lossy tuned waveguide; stiffness is a caller argument | Bow closes on a modal soundboard; bridge force is the drive | Compact monopole of the bowed bridge acceleration | yes | no | Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored |
| Double bass | Bow friction and pizzicato both step | Lossy tuned waveguide; stiffness is a caller argument | Bow closes on a modal soundboard; bridge force is the drive | Compact monopole of the bowed bridge acceleration | yes | no | Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored |
| Harp | Pluck position steps a waveguide | Lossy tuned waveguide; stiffness is a caller argument | Modal soundboard driven by bridge force, with feedback | Compact monopole of the bridge acceleration; area and distance are arguments | yes | no | Mode frequency, Q, and mass are arguments. Other courses are not coupled |
| Harpsichord | Pluck position steps a waveguide | Lossy tuned waveguide; stiffness is a caller argument | Modal soundboard driven by bridge force, with feedback | Compact monopole of the bridge acceleration; area and distance are arguments | yes | no | Mode frequency, Q, and mass are arguments. Other courses are not coupled |
| Classical guitar | Pluck position steps a waveguide | Lossy tuned waveguide on the 650 mm course | Modal soundboard driven by bridge force, with feedback | Compact monopole of the bridge acceleration; area and distance are arguments | yes | no | Mode frequency, Q, and mass are arguments. Other courses are not coupled |
| Electric guitar | Pluck position steps a waveguide | Lossy tuned waveguide on the 25.5 in course | No body filter | Pickup position comb and coil velocity; no inductance | no | no | String and pickup step. The body filter is absent |
| Piano | Felt hammer steps a stiff waveguide | Lossy stiff tuned waveguide; strike and stiffness are arguments | Hammer closes on a modal soundboard; bridge force is the drive | Compact monopole of the hammered bridge acceleration | yes | no | Mode frequency, Q, and mass are arguments. The other four courses are not coupled |
| Clarinet | Inward reed steps a bore delay | Cylindrical delay; an open end replaces the constant magnitude | None | Radiated bell pressure \(\rho\dot U/(4\pi r)\). Levine–Schwinger reflection when a radius is passed | yes | no | Caller holes and a register key sit on the lattice bore. Kirchhoff loss is on only when a reflection corner is set |
| Flute | Jet steps an open embouchure | Open-open cylindrical delay | None | Radiated bell pressure \(\rho\dot U/(4\pi r)\). Levine–Schwinger reflection at the far end; the embouchure stays an ideal release | yes | no | Caller tone holes sit on the lattice. Kirchhoff loss is on only when a reflection corner is set |
| Trumpet | Outward lip steps a cone with the cup | Truncated cone, or a caller Bessel area exponent | None | Radiated bell pressure \(\rho\dot U/(4\pi r)\) on the cone or the horn. Levine–Schwinger reflection when a radius is passed | yes | no | Engaged valve lengths are an argument. A caller Bessel area exponent is the flare. No measured bell curve is stored |
| Trombone | Outward lip steps the slide length on a cone with the cup | Truncated cone at \(L = L_0 + 2x\), or a caller Bessel area exponent | None | Radiated bell pressure \(\rho\dot U/(4\pi r)\) on the cone or the horn. Levine–Schwinger reflection when a radius is passed | yes | no | The F-attachment length is an argument. A caller Bessel area exponent is the flare. No measured slide chart is stored |
| Saxophone | Inward reed steps the spherical-wave cone | Truncated-cone delay; the cylinder is not this stepper | None | Radiated bell pressure \(\rho\dot U/(4\pi r)\). Levine–Schwinger reflection; bell radius is an argument | yes | no | Caller tone holes sit on the stepped cone. Kirchhoff loss is on only when a reflection corner is set |
| Bass drum | Felt mallet steps the batter head | Damped membrane modes; a port is not opened | Closed cavity, not a ported shell | Monopole of the batter volume acceleration | no | no | Two heads and a closed cavity radiate. The port is absent |
| Rack tom 10 in | Felt mallet steps the batter head | Damped membrane modes of both heads | Closed cavity couples the heads; shell depth is the volume | Monopole of the batter volume acceleration | yes | no | Head tension and Q are arguments. No port is in this shell |
| Rack tom 12 in | Felt mallet steps the batter head | Damped membrane modes of both heads | Closed cavity couples the heads; shell depth is the volume | Monopole of the batter volume acceleration | yes | no | Head tension and Q are arguments. No port is in this shell |
| Floor tom | Felt mallet steps the batter head | Damped membrane modes of both heads | Closed cavity couples the heads; shell depth is the volume | Monopole of the batter volume acceleration | yes | no | Head tension and Q are arguments. No port is in this shell |
| Snare 5.5 in | Felt mallet steps the batter head | Damped membrane modes of both heads; snare wires absent | Closed cavity couples the heads | Monopole of the batter volume acceleration | no | no | Two heads, the cavity, and radiation step. Snare wires are absent |
| Snare 6.5 in | Felt mallet steps the batter head | Damped membrane modes of both heads; snare wires absent | Closed cavity couples the heads | Monopole of the batter volume acceleration | no | no | Two heads, the cavity, and radiation step. Snare wires are absent |
| Splash | Felt mallet steps a free plate | Damped Kirchhoff mode | None | None | no | no | Mallet and plate step. Nonlinearity and radiation are absent |
| Hi-hat | Felt mallet steps one free plate | Damped Kirchhoff mode; plates uncoupled | None | None | no | no | Mallet and one plate step. The pair, nonlinearity, and radiation are absent |
| Crash 16 in | Felt mallet steps a free plate | Damped Kirchhoff mode | None | None | no | no | Mallet and plate step. Nonlinearity and radiation are absent |
| Crash 18 in | Felt mallet steps a free plate | Damped Kirchhoff mode | None | None | no | no | Mallet and plate step. Nonlinearity and radiation are absent |
| China | Felt mallet steps a free plate | Damped Kirchhoff mode; flange not modeled | None | None | no | no | Mallet and plate step. The flange, nonlinearity, and radiation are absent |
| Ride | Felt mallet steps a free plate | Damped Kirchhoff mode | None | None | no | no | Mallet and plate step. Nonlinearity and radiation are absent |

Required WAV files go to `wavOutputDirectory()`: `MMATH_WAV_DIR` when
that variable is set and non-empty, otherwise the process temporary
directory. A listening copy is written under `/opt/cursor/artifacts`
only when that directory already exists. A missing artifacts directory
is not a failure. The files are generated and are not committed.

The example `instrument_voice_demo` writes `mmath-voice-waveguide.wav`
from the waveguide voice. The test writes the same name and checks the
file is not silent. `instrument_pluck_demo` writes one second of each
wired course: `mmath-pluck-violin.wav`, `mmath-pluck-viola.wav`,
`mmath-pluck-cello.wav`, `mmath-pluck-double-bass.wav`,
`mmath-pluck-harp.wav`, `mmath-pluck-harpsichord.wav`,
`mmath-pluck-classical-guitar.wav`, and
`mmath-pluck-electric-guitar.wav`. The electric-guitar file is the coil
velocity. The test writes the same names, shorter, and checks pitch,
decay, finiteness, and a non-silent PCM payload. The contact test
writes the bow, hammer, mallet, and valve renders as
`mmath-<instrument>.wav`, peak-scaled to 0.5 so a displacement in
metres or a pressure in pascals is audible. The five bore-delay
renders are `mmath-clarinet-delay.wav`, `mmath-saxophone-delay.wav`,
`mmath-flute-delay.wav`, `mmath-trumpet-delay.wav`, and
`mmath-trombone-delay.wav`, so a later case cannot replace the
two-second wind files. That scale is playback
only. The assertions read the unscaled samples. The wind-tone test
writes `mmath-clarinet.wav`, `mmath-saxophone.wav`,
`mmath-flute.wav`, `mmath-trumpet.wav`, and `mmath-trombone.wav`
as the two-second limit cycle, loudness-normalized so those five
files sit within 1 LU of each other. That gain is playback only. The soundboard test
writes `mmath-violin-soundboard.wav`,
`mmath-violin-soundboard-light.wav`, `mmath-harp-soundboard.wav`,
`mmath-harpsichord-soundboard.wav`, and
`mmath-classical-guitar-soundboard.wav` the same way. The cavity-drum
test writes `mmath-bass-drum-cavity.wav`,
`mmath-rack-tom-10-cavity.wav`, `mmath-rack-tom-12-cavity.wav`,
`mmath-floor-tom-cavity.wav`, `mmath-snare-55-cavity.wav`, and
`mmath-snare-65-cavity.wav`. The cup test writes
`mmath-trumpet-cup.wav` and `mmath-trombone-cup.wav`. The bowed
soundboard writes `mmath-violin-bow-body.wav`,
`mmath-violin-bow-body-light.wav`, `mmath-viola-bow-body.wav`,
`mmath-cello-bow-body.wav`, and `mmath-double-bass-bow-body.wav`. The
hammered soundboard writes `mmath-piano-soundboard.wav` and
`mmath-piano-soundboard-light.wav`.

## Defaults

A physical number is a default only when a comment and this file name the source. The new voices do not store a T60, a Q, a modal mass, a felt \(K\) or \(p\), a hammer mass, a radiating area, a listening distance, or a bell radius. `ValveBlow`'s radius and end correction default to 0, and that 0 keeps the constant far-end magnitude; it is not \(0.6133a\). Passing \(0.6133a\) is the caller's choice. Attack and release default to 0, which is a step in blowing pressure. The omitted listener distance is 1 m. The omitted jet aim is 0, on the edge center. `OpenEndReflection::create` refuses a non-positive radius. The international inch \(0.0254\,\mathrm{m}\) and the avoirdupois pound \(0.45359237\,\mathrm{kg}\) are the SI definitions used by the catalog parsers. Dry air uses \(\gamma=1.4\) and \(R=287.058\,\mathrm{J/(kg\cdot K)}\), the engineering gas constant named in `medium.h`. `VoiceRenderConfig`'s block size of 512 only batches samples.

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
| `instruments/trombone.h` | That lip and cup on a U-slide. Closed length and handslide travel are arguments. The air column is \(L_0+2x\) |
| `instruments/render_status.h` | Two columns: renders audio, and full instrument complete. Sixteen render audio. None are fully complete |
| `instruments/cavity_drum.h` | Mallet, one or two heads, closed-cavity compliance, and a monopole. Shell depth is the volume. A port and snare wires are not stored |
| `instruments/soundboard_voice.h` | Plucked course on a yielding modal soundboard. Bridge force drives the modes; their velocity is the end displacement. The sample is \(\rho A a(t-r/c)/(4\pi r)\). Frequency, Q, mass, area, and distance are arguments |
| `physics/radiation.h` | Unflanged open-end load \((1/4)(ka)^2+jk\Delta\) and its delay-plus-curvature stepper. Radius and end correction are arguments |
| `instruments/cone_valve.h` | Reed or lip on the spherical-wave cone, optional open end, cup compliance, and a caller extra length for valves or the F attachment. The sample is radiated bell pressure. Lip displacement stops at the opening rails. Zero extra length leaves the cone delay unchanged |
| `instruments/bessel_horn.h` | Lip on a Webster power-law horn. Area exponent 0 is the cylinder and 2 is the cone. Any other non-negative exponent is the flare, solved with Bessel functions rather than a chain of cones. No measured bell curve is stored |
| `instruments/plucked_string.h` | One course as a lossy tuned-waveguide voice, or the same pluck on `Fdtd1D`. Listen fraction and `CoilVelocity` are arguments. No stored T60, stiffness, or pickup location |
| `instruments/bowed_string.h` | Stick-slip junction on the tuned loop, optionally closed on a modal soundboard. Without a body the sample is bow-point velocity and the bridge is rigid. With a body the sample is the compact monopole of the bridge acceleration. Frequency, Q, mass, area, and distance are arguments |
| `instruments/struck_string.h` | Felt hammer against hinged stiff-string modes, and the same Hunt–Crossley force on a stiff tuned loop closed on a modal soundboard. The radiating sample is the compact monopole of the bridge acceleration. \(K\), \(p\), \(\lambda\), frequency, Q, mass, area, and distance are arguments |
| `instruments/mallet_voice.h` | Felt mallet on one membrane mode or one free-plate mode. Quality factor is an argument. No air cavity, snare wires, or plate nonlinearity |
| `instruments/bore_network.h` | Keefe, Lefebvre, and Dalmont tone-hole lengths, a lattice shared by clarinet, flute, and saxophone, and the sum of engaged brass slides. Hole radii and valve lengths are arguments. Viscous wall loss is not stepped |
| `instruments/bore_valve.h` | Inward reed, outward lip, or flute jet on a cylindrical delay with a fractional Thiran section. The sample is radiated bell pressure. Far-end magnitude is an argument, not a stored bell. The saxophone cone and the brass cup are not stepped |
| `physics/oscillator.h` | Exact step of \(q''+2\sigma q'+\omega^{2}q=f\) with \(f\) held over one sample. Underdamped only |
| `render/voice.h` | Block-based `IVoice` into any sink. No envelope |
| `render/waveguide_voice.h` | Tuned waveguide stepped at the sample rate. Not an instrument |
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
set and is not used. No pickup constant is printed, and no
inductance is stored. `PluckedStringVoice` reads a caller listen
fraction on a tuned course. `CoilVelocity` is the transverse velocity
at that node. A pluck is `ExcitationKind::Pluck`. `fingerStop`
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
- A trombone with the slide closed matches that trumpet at the closed
  length. Extending the handslide by \(x\) makes the air column
  \(L_0+2x\), and the fundamental, the round trip, and the bore
  reactance match a trumpet of that longer cylinder. Doubling \(x\) does
  not halve the fundamental when \(L_0\) is still there. \(Z_c\), the
  cup compliance, and the lip flow do not change with \(x\). An even
  index is still refused. A negative travel and a zero closed length
  are refused.
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
- A two-second blow of each wind is a periodic radiated note. The
  normalized autocorrelation at one period is above 0.9 in steady
  state, the RMS of the last two quarter-seconds stays within 10
  percent, each 100 ms window from 1.0 s to 2.0 s stays within 1 dB
  of their mean, and the mean is small beside that RMS. Every voice
  radiates with the same \(\rho\dot U/(4\pi r)\) at \(r=1\,\mathrm{m}\).
  The clarinet
  fundamental is the quarter-wave. Its odd partials fall after the
  third, and the evens are present once the reflection corner is set.
  The saxophone fundamental lies between \(c/(4L)\) and \(c/(2L)\) and
  keeps a run of even and odd partials. The flute is near \(c/(2L)\),
  the fundamental is the strongest partial, and the second harmonic is
  present once the jet is aimed off the edge center. Trumpet and
  trombone play the cone's second register, with the lip below that
  root, and the magnitude centroid sits above \(2.5 f_0\). A clarinet
  below the pressure threshold does not sustain. Lengthening the
  clarinet lowers the pitch. The same clarinet at 44100 Hz and 48000 Hz
  agrees.

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

## Trombone

`trombone.h` is the trumpet lip and cupped cylinder on a handslide. It
includes `trumpet.h`. The lip, the cup compliance
\(C_a=V/(\rho c^2)\), and the open-cylinder impedance
\(j Z_c\tan(kL)\) are that header's. The length is not a valve
combination.

A handslide is two legs joined by a bow. Travel \(x\) of the outer
slide lengthens each leg by \(x\), so the air column is

\[
L=L_0+2x.
\]

\(L_0\) is the caller's length with the slide closed, including whatever
closed path was measured. \(x=0\) is that closed slide and is legal.
\(x<0\) is `InvalidArgument`. The two is the pair of legs. It is not an
end correction and not a semitone spacing. No seven-position chart is
stored, and no tenor or bass length is stored.

At fixed \(x\) the modes are the odd quarter-waves \((2n-1)c/(4L)\). An
even index is `DomainError`. Moving the slide leaves \(Z_c\), \(C_a\),
and the lip unchanged, and it changes \(L\), the round trip \(2L/c\),
and the frequencies. Doubling \(x\) halves the fundamental only in the
limit \(L_0=0\), which this slide refuses. The lip flow at a given
\(\Delta P\) is the trumpet's outward-striking jet. The caller passes
the outgoing pressure and the outgoing pressure from one current round
trip earlier. Those flows are not solved into a note.

Not in this model: a bell flare, an F attachment, a stored position
chart, and a crook.

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

## Bessel horn

`besselHornResonanceHz` is the Webster power-law horn
\(S(x)=S_a(x/x_a)^\varepsilon\). \(\varepsilon=0\) is the cylinder
\((2n-1)c/(4L)\). \(\varepsilon=2\) is the spherical cone, and its first
root matches `ConeValveVoice`. A sealed throat is \(\partial p/\partial x=0\)
and an ideal open bell is \(p=0\), so the root is a Bessel cross product
of order \(|\varepsilon-1|/2\). The exponent is an argument. It is not
replaced by conical slices, and no measured bell radius list is stored.
`BesselHornVoice` drives those roots from the lip. The throat impedance
is the modal sum whose residue is
\(2\omega\rho p/(S\partial^2 p/\partial x\partial\omega)\). Each mode
steps with the open-end loss \(-\ln(|R|)\) per round trip \(2L/c\). A
positive radius uses the Levine-Schwinger reflection at that mode. An
end correction lengthens the stepped horn and leaves `fundamentalHz()`
on the uncorrected length. Extra length is valve tubing or the F
attachment. The cup, when the trumpet has one, stays
\(C\,dp/dt=U_{\mathrm{lip}}-U_{\mathrm{bore}}\). Exponent 2 is not a
different delay line from the cone, so the cone samples are unchanged.
Full-instrument complete stays false: there is still no measured bell
curve. The Bessel file is this modal horn. It is not the cone trumpet
or trombone note.

## Wind tone

The five self-sustained winds are a periodic radiated note, not the
mouth pressure and not the ring after the player stops. The junction
is the one in Julius O. Smith, *Physical Audio Signal Processing*
(CCRMA), and in the STK clarinet, flute, saxofony, and brass models:
a reed or lip solves \(Z U+\Delta P=p_m-2p_{\mathrm{in}}\) with an odd
Bernoulli jet, a flute adds a delayed jet on an open-open bore, and a
cone carries the spherical potential \(\psi=xp\) (Gary P. Scavone,
PhD thesis, Stanford, 1997; Guillemain, Kergomard, and Voinier, JASA
2005, "Real-time synthesis of clarinet-like instruments using
digital impedance models"). Those
references check the structure. No recording and no STK coefficient
table is stored.

The reed is the inertial mass on Dalmont's spring, stopped at the lay.
A memoryless negative resistance chatters at the sample rate. If the
embouchure damping is light beside the bore, the reed can lock to its
own resonance instead of the first bore root. That damping is the
caller's argument. The flute edge is \(\tanh(y/h)\) of the delayed
deflection, with the small-signal slope \(2 U_j y/h\). Aim zero is an
odd function of deflection, so the second harmonic is absent; a
nonzero aim is the even-harmonic operating point. A saturated jet
plus a flat reflection radiates a square, because bell pressure is
proportional to \(\mathrm{d}U/\mathrm{d}t\). The flute render therefore
keeps a reflection corner, an aim offset, and a small turbulent
centerline term. An outward-striking
lip oscillates when its resonance sits below a bore peak (Fletcher,
1979). On the cylinder the spring continues past \(S_{\max}\). On the
cone the displacement stops at the rails, because otherwise a high
blowing pressure winds the lip up and pins the opening at \(S_{\max}\).

A positive `reflectionCutoffHz` is the low-pass on the wave that
returns to the valve. The bell flow is the incident wave minus that
return, so energy the cutoff does not reflect still reaches the
radiator. On a cylinder, and on a lip cone with no Benade envelope,
that same corner is also the high-pass radiation smoother: the bore
is not the radiating mouth, and `radiationHz` replaces it when set.
On a saxophone with `spectrumBreakHz` set, the bell keeps \(c/a\) and
Benade's envelope is the external transfer; the reflection corner is
not applied a second time. The same flag turns on Kirchhoff's wide-tube
wall loss, \(\alpha=\sqrt{\omega\nu/2}\,(1+(\gamma-1)/\sqrt{\mathrm{Pr}})/(a c)\),
with Sutherland's viscosity (\(\mu_0=1.716\times 10^{-5}\,\mathrm{Pa\cdot s}\)
at 273.15 K, \(S=110.4\,\mathrm{K}\), \(\mathrm{Pr}=0.71\)). The one-pole
is matched at a high frequency so it does not invent a cutoff inside the
musical band; the caller's corner is what rolls the spectrum. A zero
corner leaves \(|R|\) constant and skips the wall loss, so the ideal
bore tests keep their pitch and their decay. A non-radiating bore adds
the caller's end correction as length. Levine and Schwinger's unflanged
pipe is about \(0.61a\); the flute render uses that. A positive radius
already carries the correction inside the open-end filter.

`tests/test_wind_tone.cpp` holds a two-second blow. The mouth-pressure
ramp is a performance argument. The demo durations are published player
times, not one shared 30 ms edge:

- clarinet and saxophone, 0.40 s, inside the 0.35–0.60 s mouth-pressure
  build-up measured by Fuks and Sundberg (KTH TMH-QPSR 3/1996, 41–56;
  Acta Acustica 85, 1999) on clarinet, saxophone, oboe, and bassoon;
- flute, 20 ms, inside the 10–30 ms flute-like attacks of Terrien, Blanc,
  and Fabre (ISMA 2014), who also give an acoustic rise near
  \(6\,\mathrm{ms}+9.5/f_0\) (Fletcher, Acustica 34, 1976);
- trumpet, 40 ms, the fast end of the 40–128 ms tongue-tip movement in
  Bertsch and Hoole (ISMA 2014). Trombone, 128 ms, the long end of that
  same range, used as the pressure rise. On these opening notes the lip
  is free (`tongueReleaseFrames` is 0): a tongue that lets go at full
  pressure, or a lip snapped to its rest opening, put the finished
  partial series on in one cycle. Later tongued notes still close for
  40 ms. The sound is compared with Luce and Clark (JASA 42, 1967):
  significant amplitude change about 50 ms, average deviation about
  20 ms; the trumpet fundamental transient about 10 ms; the trombone
  fundamental about seven cycles and higher partials toward 70 ms. The
  1 dB hold below is the tail after that rise.

Attack time below is the first moment a 10 ms RMS stays within 1 dB of
the 1.5–2.0 s level for 40 ms. That hold includes the exponential tail,
so it is longer than a 10–90% rise. Steady state is 1.0–2.0 s. Ten
100 ms windows in that second each stay within 1 dB of their mean RMS.
Partials in the bullets below are goertzel magnitudes on the unscaled
render, dB re the fundamental. The playback WAVs use a separate count,
dB re the strongest partial, written with the files. Levels are the unscaled steady
RMS at the shared listener, \(r=1\,\mathrm{m}\), in dB re 1 Pa, and the
same level inside 20 Hz–16 kHz. A cone multiplies bell volume flow by
\(x_\mathrm{bell}/x_0\) before that same radiator; a cylinder does not.
The spread below is source strength, not a second distance. Energy above
\(0.4 f_s\) is under 1% on each of these notes, and more than 90% of the
energy is below 5 kHz.

- clarinet, played 183.2 Hz on a bore of \(c/(4L)=190.7\,\mathrm{Hz}\), corner 1500 Hz (Benade's open-hole cutoff, the same corner as the excerpt), reed damping 0.11 N·s/m, ramp 0.40 s, radiated turbulence 0.40: 0, −16.2, −1.5, −12.2, −4.4, −12.6, −9.0, −17.7, −18.3, −28.7. The odd partials dominate and fall after the third, and the third, fifth, seventh, and ninth stay well above a sine's tail. Eighteen partials are above −40 dB. Autocorr 0.9715, HNR 15.3 dB. The inter-harmonic floor, 200 Hz–6 kHz, is −47.6 dB. Level −10.5 dB re 1 Pa (83 dB SPL); audible band −10.5 dB. Envelope ±0.11 dB. Attack 355 ms, inside the Fuks and Sundberg 0.35–0.60 s pressure pattern. From 80 ms to 180 ms the autocorrelation at the steady period is 0.20: that span is still mostly reed-gap noise, not a periodic tone. The fundamental then rises from 30 dB under its steady amplitude to 8 dB under it at 440 dB/s. Broadband residual leads the fundamental's arrival at 20 dB under its steady amplitude by 150 ms. Li, Almeida, Smith, and Wolfe (JASA 139, 2016) measured tens to hundreds of dB/s on a soft note and about 1000 dB/s on an accent. The acceptance window on this soft note is 50–500 dB/s. 25 ms of residual lead is an acceptance argument. A single raised cosine is steepest at half pressure, which is where this reed crosses threshold, and that shape grew at 880 dB/s. The creep is linear from 0.50 to 0.78 of full pressure over the middle of the 0.40 s. Those fractions are arguments: Bergeot et al. (arXiv:1510.04793) show that a fast rise delays the bifurcation and the amplitude then jumps, and they do not print this waveform. The reed step is the exact linear spring. Explicit Euler on this mass chatters at a fraction of the sample rate (the fast pole \(r/m\) is outside the Euler stability region) and that squeak was the smooth electronic attack.
- flute, 284.0 Hz, corner 1800 Hz, aim 0.4 jet thicknesses, initial centerline displacement 0, end correction \(0.61a\), ramp 20 ms, radiated turbulence 0.56: 0, −15.6, −17.5, −24.5, −34.6, −36.1, −55.6, −48.0, −46.9, −50.8. The fundamental still leads. Six partials are above −40 dB, even and odd. A 400 Hz corner had left the third under −40 dB. Fletcher (JASA 57, 233–244, 1975) measured the second and third within about 10 dB of the fundamental in the middle octave. This note does not reach that (the third is −18 dB), and the Faune C#5 third is about −28 dB. 1800 Hz is an argument, not a frequency from that paper. Autocorr 0.944, HNR 12.3 dB. The inter-harmonic floor, 200 Hz–6 kHz, is −48.4 dB on the unscaled render and −49.0 dB on the written file. Level +7.3 dB re 1 Pa (101 dB SPL); audible band +7.2 dB. Envelope ±0.10 dB. Attack 350 ms on the 1 dB hold. The 20 ms ramp is unchanged. The old attack started the jet already deflected by half a thickness, so the pressure ramp faded in a periodic tone. The centerline now starts at 0. Turbulent offset of that centerline seeds the bore (Fletcher, Acustica 34, 1976; Coltman), and the radiated monopole uses the mixing-layer transit \(h/U\) until the standing wave is louder than that jet. A pressure slice of the whole jet, at \(U/h\) or at the convection rate \(U/W\), pulled the fifth partial under −40 dB, so it is not added. The fundamental rises at 400 dB/s, and residual energy leads it by 35 ms. At 400 Hz the same hold landed near 45 ms, because that limit cycle is about 21 dB quieter; Terrien's 10–30 ms is the significant rise, not this last decibel.
- saxophone, autocorrelation 199.2 Hz, apex 0.20 m, 4200 Pa (about 43 cm H2O, inside the alto range of that Fuks and Sundberg paper) against a closure pressure of 7500 Pa, embouchure damping 0.09 N·s/m, reflection corner 1500 Hz, ramp 0.40 s, radiated turbulence 0.55. Goertzel re that autocorrelation pitch: 0, −1.9, −9.1, −9.3, −10.8, −17.9, −23.0, −27.2, −33.4, −44.7. Magnitude centroid \(3.18 f_0\). Autocorr 0.948, HNR 12.6 dB. Level +2.8 dB re 1 Pa (97 dB SPL); audible band +2.8 dB. Envelope ±0.06 dB. The inter-harmonic floor, 200 Hz–6 kHz, is −47.3 dB. Attack 940 ms. The ramp itself is inside 0.35–0.60 s; the 1 dB hold lands later because the linear creep holds the reed near threshold, and the radiated turbulence keeps a 10 ms window from sitting inside that 1 dB until then. The fundamental grows at 73 dB/s, inside the same 50–500 window as the clarinet, and residual energy leads it by 320 ms. The steady note is still the first mode (autocorrelation 199.2 Hz). YIN on the playback file is 199.3 Hz, and the Hann partials re the strongest peak are listed with the other four files below. The mouth is the clarinet's: the same pressure law, exact reed step, aperture-flow seed, and the same breath after the loop.

The 8 kHz reflection corner was a sawtooth. A beating reed already carries harmonics (Fletcher, JASA 93, 2172–2180, 1993). `reflectionCutoffHz` low-passes only the wave that returns to the reed. The bell flow is the incident wave minus that return, so energy above the corner still radiates on the first pass. Kirchhoff loss on the log-mean cone radius is mild at these frequencies and is not the brightness. The mouthpiece volume is already the apex distance in `saxophone.h`; Chen, Smith, and Wolfe (Acoustics Australia 37, 2009) give a soprano mouthpiece of 2.25 cm³ against a missing cone of 3.35 cm³, and a second cavity is not added.

The 1500 Hz corner is the high end of the alto tone-hole cutoffs measured by Petersen, Colinot, Kergomard, and Guillemain (Acta Acustica 4, 2020: about 500 Hz at the bell to about 1500 Hz in the highest cells) and sits inside Chen, Smith, and Wolfe's cylinder estimate for a soprano, \(1340\pm 240\,\mathrm{Hz}\). The radiated pressure then passes Benade and Lutgen's envelope, JASA 83, 1900–1907 (1988),

\[
E(x)=\left[\frac{x}{1+x^{7}}\right]^{1/2},\quad x=f/f_b,
\]

with their alto break \(f_b=837\,\mathrm{Hz}\) (tenor 618 Hz). Below the break the published slope is about +3 dB/octave; above it, about −18 dB/octave. The transfer in the voice is that shape, peak-normalized, so their scale factor is not a stored gain. On this reed a loop pole at 837 Hz jumps to the octave, so 837 Hz is the external transfer and the reflection pole stays at 1500 Hz. The mean of partials below 1 kHz is −6.2 dB and the mean from 2.5 to 5 kHz is −44.1 dB, still more than one octave of the −18 dB/octave slope. The second harmonic is a shelf. The reed already puts the third and fourth several decibels down, so the +3 dB/octave rise up to 837 Hz does not appear as a separate hump. Reed turbulence is the fraction 0.55. Once the tone is up, that fraction multiplies the radiated pressure's round-trip root-mean-square, applied after the loop and after that envelope, then smoothed at \(0.2 f_s\). Below threshold the same fraction multiplies \(\rho (U/\tau)/(4\pi r)\), with \(\tau=1/f_{\mathrm{res}}\) the reed period, and a sample of it also enters the bore as \(Z\,\delta U\). A product with the instantaneous sample had piled the same disturbance onto the harmonics, and the bins between them sat near −59 dB. The clarinet stays at 0.40. The flute is 0.56: 0.40 left its median under −50 dB, and 0.48 still read −50.4 dB on the written file once the peaks were interpolated. A disturbance of the blowing pressure is cancelled by the pressure search, so the bell stayed a line spectrum, and on the shorter Old Castle tubes that disturbance locked the second mode. Fletcher and Rossing (2nd ed., chap. 13) treat the reed channel and the flue mixing layer as turbulent and do not print a noise-to-harmonic ratio, so the fraction is an argument. Wolfe's UNSW saxophone notes record breathy noise from turbulence in the gap between reed and mouthpiece, in the regime that does not yet oscillate, and they print no level during a played note. The acceptance argument is a median inter-harmonic level, 200 Hz–6 kHz, no quieter than −50 dB re the strongest partial, as \(20\log_{10}(|X|)\). A floor near −120 dB is the converter and fails. On the low two-second note that median is −47.3 dB. A full-scale 16-bit sine in the same window sits near −122 dB. With the fraction at zero the same bins sit near −147 dB. The 100 ms RMS stays within ±0.09 dB. The long G#4 of the Old Castle excerpt is the held mid-range note: the same median on the written file, from 1.20 s to 2.80 s, is −48.4 dB, and YIN there is 416.5 Hz. Below threshold the same fraction of the aperture flow is audible as noise; the bore's fundamental grows out of it rather than arriving as a finished spectrum. Above \(f_b\) that note is still thinner than the low note. It is not a sine (the second sits near −6 dB), and it is not finished. Vibrato is not on this two-second note. It is on the three long notes of the Old Castle excerpt below, a length modulation at 5.5 Hz by 12 cents. Both numbers are arguments.

- trumpet, played 225.4 Hz, bore area \(1.09\times 10^{-4}\,\mathrm{m}^2\), lip damping 0.001 N·s/m, apex 0.26 m, lip below the second cone root, lip free during the rise, ramp 40 ms, brassiness 1, lip-jet turbulence 0.20, near concert Bb3 (233 Hz): 0, −8.7, −12.6, −15.0, −19.6, −24.8, −28.5, −31.0, −31.8, −33.8. Magnitude centroid \(3.38 f_0\), eighteen partials above −40 dB. Five of them sit within 20 dB of the strongest, which is five within 40 dB on a \(20\log_{10}(|X|^2)\) reading. At \(8.0\times 10^{-5}\,\mathrm{m}^2\) and 0.003 N·s/m that reading counted four. Both the area and the damping are arguments, not a measured trumpet. Autocorr 0.992, HNR 21.2 dB. Level −10.6 dB re 1 Pa (83 dB SPL); audible band −10.7 dB. Envelope ±0.06 dB. Attack 80 ms on the 1 dB hold. The fundamental grows at 550 dB/s and residual energy leads it by 35 ms. The old opening snapped the lip to its rest opening at 25 ms; that was a finished partial series in about 5 ms.
- trombone, played 118.8 Hz, bore area \(3.0\times 10^{-4}\,\mathrm{m}^2\), lip damping 0.0005 N·s/m, apex 0.60 m, lip free during the rise, ramp 128 ms, brassiness 1, lip-jet turbulence 0.20, on a 2.75 m cone, near concert Bb2 (117 Hz): 0, −10.5, −14.1, −16.0, −18.6, −21.7, −24.9, −27.7, −29.1, −30.8. Magnitude centroid \(4.11 f_0\), twenty-three partials above −40 dB. Five sit within 20 dB of the strongest. At \(1.0\times 10^{-4}\,\mathrm{m}^2\) and 0.004 N·s/m a \(20\log_{10}(|X|^2)\) count found three. The area and the damping are arguments, not a measured trombone. Autocorr 0.991, HNR 20.2 dB. Level −32.5 dB re 1 Pa (61 dB SPL); audible band −32.5 dB. Envelope ±0.08 dB. Attack 345 ms. The fundamental grows at 338 dB/s and residual energy leads it by 30 ms. A 45 ms rise with the lip snapped open crossed the −20 dB point inside the first audible hop. Luce and Clark's trombone fundamental is about seven cycles and higher partials tend toward 70 ms; the 1 dB hold is the tail after the 128 ms rise, later on this wider bore. The played note stays under 2.4 times the cone's first root.

`brassiness` is the fraction of the cylindrical Burgers advance \(\beta p L/(\rho c^3)\), \(\beta=(\gamma+1)/2\), applied to the traveling wave at the bell. Hirschberg, Gilbert, Msallam, and Wijnands (JASA 99, 1754–1758, 1996) measured the shocks that advance produces in trombones. A flare accumulates less than a cylinder, and that paper prints no waveguide gain, so the fraction is an argument. The steepened sample is what radiates. It is not written back into the delay: feeding the advance back grew a length-change transient without bound and darkened this trumpet's limit cycle. The lip still sees the linear return. On these notes the peaks ask for about 1.8 samples of advance. A one-sample Taylor step stopped there, so the radiated sample is a four-sample causal delay read at age \(4-\mathrm{adv}\), linearly interpolated. That is still short of the shock in that paper. Neither brass note is finished.

The playback files `mmath-clarinet.wav`, `mmath-saxophone.wav`,
`mmath-flute.wav`, `mmath-trumpet.wav`, and `mmath-trombone.wav` are
those notes, K-weighted to −20 LUFS over 1–2 s (ITU-R BS.1770-4) with
a peak ceiling of −1 dBFS. Each file measured −20.0 LUFS, so the five
sit within 1 LU. That gain is playback only. The assertions read the
unscaled samples. Full instrument complete stays false: the tone-hole lattice
still has no wall loss and no fingering chart, and there is no measured
bell. The lattice is not what these five files step.

The spectral bar for this pass is the written playback WAV, read back
by the test. A 16384-point symmetric Hann window centered in 1.00–1.85 s,
level \(20\log_{10}(|X|)\) of the windowed magnitude, counts harmonics
of the YIN fundamental while \(h f_0\) stays under 12 kHz. The peak in
±3 bins is refined by the same log-magnitude parabola as the pitch
estimator, so a Hann bin halfway between frequencies is not scored a
scallop under the partial. The test requires clarinet at least 4, alto
saxophone at least 6 with at least two even partials inside 40 dB,
flute at least 3, and trumpet and trombone at least 5 inside 40 dB and
at least 5 inside 20 dB, with at least eight brass harmonics in total
(a harmonic counts toward that total when it sits 10 dB above the
inter-harmonic floor). Five magnitude partials inside 20 dB are five
partials inside 40 dB on a \(20\log_{10}(|X|^2)\) reading. The previous
bores counted 4 and 3 on that reading. Those counts are computed from
the file. A stored total is not the check. Harmonics 1–10 of the same
windows, \(20\log_{10}(|X|)\) re the strongest partial: clarinet
0, −16.1, −1.5, −12.4, −4.3, −12.7, −8.8, −17.4, −17.5, −28.1;
saxophone 0, −1.9, −9.1, −9.4, −10.6, −17.6, −23.2, −27.2, −32.8, −38.6;
flute 0, −14.6, −16.0, −20.7, −31.1, −26.5, −41.1, −33.1, −45.7, −41.7;
trumpet 0, −8.5, −12.4, −14.8, −19.5, −24.6, −27.8, −30.0, −32.6, −34.1;
trombone 0, −11.1, −14.4, −16.2, −18.8, −22.2, −25.6, −27.7, −29.9, −31.0.
YIN on 1.00–1.85 s is 183.06, 199.33, 283.73, 225.25, and 118.82 Hz.
The written files count 17, 17, 9, 17, and 21 partials inside 40 dB,
and the brass files count 5 inside 20 dB, with 28 and 35 harmonics
10 dB above the floor.
The goertzel in the test is the same \(20\log_{10}\) of magnitude, on
the unscaled render, referenced to the autocorrelation pitch rather
than to the strongest bin. The Old Castle G#4 is a different note and
is not finished. The inter-harmonic floors of the five files, 200 Hz
to 6 kHz in that Hann window, are −49.5, −47.2, −49.0, −58.5, and −56.7 dB.
All five use the same aperture breath. On a reed or a lip the fraction
also seeds the bore as \(Z\,\delta U\). On the flute the bore seed is
the centerline offset, and the fraction is only the radiated monopole.
The held G#4 floor is the separate check at −48.4 dB.

The same file renders an audition excerpt for each wind, `mmath-excerpt-clarinet.wav`, `mmath-excerpt-saxophone.wav`, `mmath-excerpt-flute.wav`, `mmath-excerpt-trumpet.wav`, and `mmath-excerpt-trombone.wav`. The melodies are note data in `tests/wind_excerpts.h`: concert pitch, duration, articulation, and the score's dynamic. No engraved edition is copied, and no samples are played. Pitch is the delay the voice already holds. `BoreLatticeVoice` is not stepped: it has no runtime fingering, no wall loss, and no envelope. Stopping the breath, or ending the blow, parks a lip on the closed stop and forces that opening to zero. The stop arithmetic can leave a roundoff slit, and the pressure search then fails against a bore that is already sounding. A tongue back on the mouthpiece is on hold. A note is graded when it lasts at least 0.45 s, so the measurement window sits inside it. YIN on a graded note is taken from 0.45 s into the note through the end, every 0.05 s, and the largest absolute error has to be within ±25 cents of the concert target. A shorter note is not graded for speech; its end-of-window YIN has to be within ±50 cents. The saxophone D#4 pickup is 0.4 s and sits inside the 0.40 s breath attack; its end-window YIN is +3 cents. Every graded note speaks. Gershwin's Rhapsody in Blue opening is not the clarinet line: the bore glide is 35 ms at the start of a slur, not a smear across a bar.

- Clarinet: Mozart, Clarinet Concerto K.622, second movement, opening, concert pitch (the A-clarinet part sounded a minor third down), quarter = 60, 12.0 s, dynamic p. Sounding length at the 1500 Hz corner, the same corner and the same radiated turbulence 0.40 as the two-second note. The exact reed spring plays flat of \(c/(4L)\), more so as the note rises, so each concert pitch is the shortened tube whose autocorrelation is that pitch. The opening is the blow's breath attack (no tongue release on the blow): reed-gap noise, then the fundamental. YIN on the graded notes: A3 220.1 Hz (+0.5 cents), D4 294.3, F4 351.0, E4 329.3, A4 440.6, G4 393.6. Worst graded error about 10 cents.
- Alto saxophone: Mussorgsky / Ravel, Pictures at an Exhibition, "Il vecchio castello", the piano melody Ravel gave the alto, concert pitch, dotted quarter = 50, from the D#4 pickup through the low G#3, 12.8 s, dynamic p. Acciaccaturas are omitted: they are shorter than a bore retune. Truncated-cone length, apex 0.20 m, the same 1500 Hz corner, 837 Hz envelope, and turbulence 0.55 as the two-second note. The held G#4, measured on the written file from 1.20 s to 2.80 s, has YIN 416.5 Hz and a median inter-harmonic floor of −48.4 dB from 200 Hz to 6 kHz. The acceptance argument is no quieter than −50 dB. Each length is the tube this reed plays at 4200 Pa, read by YIN on the phrase, not a published correction. The earlier scale factors lengthened G#4 onto a second-mode tube; autocorrelation at the expected period then reported 416 Hz while the spectrum peaked at 829 Hz. Graded notes last at least 0.45 s. Their YIN from 0.45 s into the note stays within ±25 cents, and the strongest Hann peak on that window stays on the first mode. F#4 is 0.3315 m. At 0.330 m the carrier was about 8 cents sharp, and the 12-cent vibrato crest then sat in a CMNDF valley that includes the 127-sample lag, \(48000/127\,\mathrm{Hz}\), +37 cents of 369.99 Hz. At 0.3315 m the crest is about +12 cents and the trough is −12.6 cents (367.6 Hz); the end-window peak is 370.0 Hz. G#4's steady-region extreme is −19.6 cents on the 2.8 s note and −9.6 cents on the 0.8 s note; the end-window peaks are 413.5 Hz and 413.7 Hz, first mode. G#3's extreme is −19.7 cents, peak 206.9 Hz. Shorter notes, end-window YIN: B4 −0.7 cents, A#4 −12.6 cents, the D#4s within a few cents once the ramp has arrived, E4 about −25 to −30 cents on 0.390 m. C#4 is 0.471 m. This 0.4 s slur, arriving from D#4, speaks the first mode at 277.5 Hz (+2 cents), peak 277.6 Hz. The 0.52 m tube was first mode near 256 Hz, about −140 cents. A note started from silence near 0.47 m can lock the octave; this slur does not, and the D#4 and G#3 after it stay on the first mode. The line keeps one pressure, 4200 Pa. The three long notes use the vibrato arguments, 5.5 Hz and 12 cents.
- Flute: Debussy, Prélude à l'après-midi d'un faune, bars 1–2, quarter = 44, 12.27 s, dynamic p. The descent is one slur; the repeat takes a new breath. No vibrato. Bore length, jet delay, and blowing pressure together. The corner is 1800 Hz and the radiated turbulence is 0.56, both the two-second note: 400 Hz left the third partial under −40 dB, and 0.40 left the median inter-harmonic floor under −50 dB. This line does not reach the C7 that needed a separate argument. A tongued note is a 20 ms pressure dip to 0.55 of the note. YIN on the graded notes stays within about 8 cents: the repeat C#5 reaches +8.3 cents. Shorter notes are not graded for speech; their end-window YIN stays inside the ±50 cent bound.
- Trumpet: Mussorgsky / Ravel, Pictures, Promenade, the monophonic piano right hand before the first chord, quarter = 91, tenuto tongued, 7.25 s, dynamic f. The next bar is harmonized, so the line stops short of 8 s. Sounding length plus lip resonance \(k=m(2\pi f)^2\). Not a piston chart. The open second register uses lip \(0.75\,c/L\). Higher notes sit the lip at 0.8 of a cone partial. Length is not glided. A tongued attack retunes while the tongue is shut. Setting the new lip on the open valve leaves the finite range. The slurred F5 into D5 stays open, and that pair is the length that completes (F5 1.410 m, D5 1.386 m). F5 at 1.39 m into D5 at 1.38 m dies on the slur. Played G4 389.7, F4 346.9, Bb4 462.0, C5 520.7, F5 690.1, D5 582.9 Hz. The quarters sit about 10 to 15 cents flat on this wider bore. The F5 eighths are −21 cents, inside the ±50 cent bound for a note under 0.45 s, and are not graded for speech. The opening lip is free during the 40 ms pressure rise. Later notes still close for 40 ms.
- Trombone: Ravel, Boléro, trombone theme, quarter = 72, mf sostenuto, from the opening Bb4 through the repeated high Db, 13.33 s. The Mozart Requiem "Tuba mirum" was not needed: this high register speaks. A glissando marked in the part is not a slide here; the length changes at the note. The later acciaccatura is outside the cut. Slide length plus the same lip. The lengths are the tubes this \(3.0\times 10^{-4}\,\mathrm{m}^2\) bore plays on pitch. YIN on the graded notes: the opening Bb4 reaches −18 cents in the steady region; the later Bb4, G4, E4, and Db5 stay within about 6 cents. End-window peaks stay on the written pitch. Shorter notes are not graded for speech; their end-window YIN stays inside ±50 cents. The opening lip is free during the 128 ms pressure rise. Later notes still close for 40 ms. No vibrato.

Slurred woodwind notes glide the length over 35 ms. A brass slur keeps the bore open and changes length at the written attack. A tongued brass attack retunes while the tongue is shut. The blowing-pressure gain stays 1: the dynamic in the note data is the score marking, not a second pressure law. An 8% drop flattened this saxophone's low register. Playback of each excerpt is K-weighted from 0.5 s to the end (ITU-R BS.1770-4), aimed at −20 LUFS with a −1 dBFS ceiling. Clarinet, saxophone, flute, and trombone land on −20. The trumpet excerpt measures −22.8 LUFS: from −2.87 LUFS the −1 dBFS ceiling limits the gain to 0.101. The file continues 0.50 s after the score so the last breath ring is not cut off. Quieter than −20 is the same rule as the two-second files. The last note that lasts at least 0.40 s keeps energy above \(0.4 f_s\) under 1%.

## Brass ring

The rings in this section are breath stops. Ending the blow forces the lip onto the closed stop. That seal is not a tongue, and `tongueReleaseFrames` stays 0. A tongue back on the mouthpiece is on hold. The graded notes in the next section use the same loop. Δf = f/Q is the half-power bandwidth of the loop the sample stepper runs: the wall one-pole, the reflection-cutoff one-pole, and, when a bell radius is set, the Levine–Schwinger open end. The traveling wave is multiplied by that loop magnitude once per round trip, so a mode's amplitude is exp(−π Δf t). In decibels per second that is 20 log10(e) · π · Δf. 27.29 is that product. The free decay of the partial that is ringing, read from the rendered samples, has to sit within ±20% of 27.29·Δf, using the model's Δf at the ringing frequency. The published windows below are on Δf at the named mode, which is not always the frequency the lip was holding.

Trumpet, Bb. Mode 4, about 465 Hz, is graded when the model's Δf is 10.9–13.9 Hz. Fréour et al., JASA 148:748 (2020), report an eight-horn mean Q of 42.6 at that peak. Petiot, Poirson, and Gilbert (2004) report one horn at Q 32–38. The bore is the excerpt's \(1.09\times 10^{-4}\,\mathrm{m}^2\). The note is the Promenade Bb4 fingering, 1.023 m with the lip at 0.8 of cone mode 3, played on the mild log-mean wall so the lip still locks that pitch. Kirchhoff loss on the throat is fitted when the breath stops. The bell is the Bach #37 mouth on the Thibault and Chabassier Bessel horn, not a radius solved onto the window. On the render: played 461.5 Hz (−17 cents), ringing 428.6 Hz, Q 36.1, Δf 12.91 Hz, inside 10.9–13.9, tail −264 dB/s against a model −303 dB/s (ratio 0.87).

Mode 8, about 929 Hz, passes when Δf is 21.6–35.7 Hz. The source is Petiot, Poirson, and Gilbert (2004), Q 26–43. The length is the cone, apex 0.26 m, whose eighth root of \(\tan(kL)=-k x_0\) is 929 Hz. The bell radius is that cone's own geometric bell, \(r_\mathrm{throat}(x_0+L)/x_0\), and the wall one-pole is Kirchhoff on the throat at that root. The radius is not solved against the window. On the render: played 941.2 Hz (+23 cents), ringing 923.1 Hz, Q 39.5, Δf 23.53 Hz, tail −652 dB/s against a model −636 dB/s (ratio 1.02).

Trombone. Ablitzer, Acta Acustica 5:53 (2021), Tables 1–2, Courtois bass trombone in first position, with Δf = 2ξf. Each range is the minimum and maximum of the complex-mode and real-mode fits. A mode passes when the model's Δf at that published frequency is within ±20% of the range. The ±20% is UNSOURCED: the tables print the fits, not an acceptance band. No tenor per-mode bandwidth was found, so the bass trombone is a proxy for this cone. Modes 2–9 only.

- Mode 2, 115.6 Hz: 4.83–4.85 Hz. Model Δf 4.84 Hz, window 3.86–5.82. Played 117.9 Hz (+35 cents), ringing 105.0 Hz, Q 23.9, tail −99 dB/s, model −113 dB/s, ratio 0.87.
- Mode 3, 173.5 Hz: 6.00 Hz. Model Δf 5.93 Hz, window 4.80–7.20. Played 179.8 Hz (+62 cents), ringing 161.6 Hz, Q 29.2, tail −129 dB/s, model −145 dB/s, ratio 0.89.
- Mode 4, 233.6 Hz: 6.68–6.73 Hz. Model Δf 6.89 Hz, window 5.34–8.08. Played 242.4 Hz (+64 cents), ringing 221.2 Hz, Q 33.9, tail −163 dB/s, model −174 dB/s, ratio 0.94.
- Mode 5, 294.1 Hz: 7.64–7.71 Hz. Model Δf 7.73 Hz, window 6.11–9.25. Played 305.7 Hz (+67 cents), ringing 285.7 Hz, Q 38.0, tail −187 dB/s, model −203 dB/s, ratio 0.92.
- Mode 6, 345.6 Hz: 8.70–8.99 Hz. Model Δf 8.39 Hz, window 6.96–10.79. Played 366.4 Hz (+101 cents), ringing 342.9 Hz, Q 41.2, tail −233 dB/s, model −226 dB/s, ratio 1.03.
- Mode 7, 406.4 Hz: 9.42–10.08 Hz. Model Δf 9.10 Hz, window 7.54–12.10. Played 428.6 Hz (+92 cents), ringing 406.8 Hz, Q 44.7, tail −234 dB/s, model −249 dB/s, ratio 0.94.
- Mode 8, 463.8 Hz: 11.13–11.87 Hz. Model Δf 9.73 Hz, window 8.90–14.24. Played 494.8 Hz (+112 cents), ringing 484.8 Hz, Q 47.7, tail −243 dB/s, model −281 dB/s, ratio 0.86.
- Mode 9, 519.7 Hz: 12.57–14.03 Hz. Model Δf 10.30 Hz, window 10.06–16.84. Played 558.1 Hz (+124 cents), ringing 539.3 Hz, Q 50.5, tail −238 dB/s, model −294 dB/s, ratio 0.81.

Ablitzer does not print a bore radius. The radius here is the cylinder whose Kirchhoff Δf at 115.6 Hz equals 4.84 Hz, the center of the mode-2 range. Modes 3–9 are that same cylinder, not eight separate fits. The reference cone is 2.70 m with apex 0.60 m, the excerpt's apex. It is not a Courtois length, and it is not a length solved so that each cone root sits on the published frequency. The excerpt bore \(3.0\times 10^{-4}\,\mathrm{m}^2\) is unchanged and is not this radius. The Promenade and Boléro tubes are slide positions. Their per-mode windows are the next section, not these first-position cylinders. The lip fraction (0.75 on modes 2–3, 0.80 on modes 4–9) and the pressure that holds the lip at 30% of its opening rail are arguments. Modes 2 and 3 use the excerpt's lip-jet seed, 0.20, so those roots speak; the seed ends with the breath.

## Graded breath rings

A tongue back on the mouthpiece is on hold (step 2a). These notes end when the breath stops. The blow frames run out, the lip is parked on the closed stop, and the radiated sample is not scaled. There is no fade-out and no added reverb. While the lip is sealed, the wall one-pole is Kirchhoff on the throat at that fingering's input-impedance peak, the cone root of \(\tan(kL)=-k x_0\). Mode 0, the open second register, uses the cone root nearest the written pitch. The reflection-cutoff one-pole stays. The flare's throat reflectance joins that loop for the ring. No decay constant is added. After the pole changes, `setSoundingLength` rebuilds the delay and takes the flare one-pole's phase off the line, so the geometric length stays the round trip.

The mouths are manufacturer geometry, not a fit to Q. Conn-Selmer's Bach 42 page prints an 8-1/2 in one-piece hand-hammered bell and a .547 in large bore. The mouth radius is half of \(8.5\times 0.0254\,\mathrm{m}\). The Bach 180S37 page names a #37 bell and a .459 in medium-large bore and does not print a diameter. The Vincent Bach catalog prints \(4\,13/16\) in (122.24 mm) for Bb trumpet bells and says every Bb trumpet bell has that diameter; the bell number is the mandrel. 4.8 in is that figure rounded and is not used. The .459 in bore is not substituted: the trumpet cone stays \(1.09\times 10^{-4}\,\mathrm{m}^2\). The trombone cone stays \(3.0\times 10^{-4}\,\mathrm{m}^2\), the area these lengths play. Kirchhoff's ring on the trombone uses the printed .547 in slide, because that formula is a cylinder. Putting the .547 in area on the cone itself moved the Boléro E4 about +33 cents, so the cone area was left. The cone apex stays 0.26 m and 0.60 m. The geometric mouth of that spherical wave is still about 2.5 in; it is not the published bell.

The flare is in the bore. Trumpet: Thibault and Chabassier, JASA 150 (2021), a natural trumpet, cylinder radius 6 mm, then a Bessel horn of exponent 0.7 from 0.716 m to 1.335 m. Their mouth is 60 mm. \(b\) and \(x_0\) are the pair that meets the 6 mm throat and the Bach #37 mouth. Trombone: Scott, Measurement and Synthesis of the Trombone (SFU M.Sc., 2012), Table 2.2, \(a(x)=b(x+x_0)^{-\gamma}\) with \(\gamma=0.7\), length 0.502 m, \(b=0.0063\), \(x_0=0.0174\). The table prints a 0.108 m mouth. Levine–Schwinger ends the horn at the Bach 42 radius. The stepper is 160 cylindrical slices of that profile, and the loop one-pole is matched to the throat reflectance at the impedance peak. The horn's own travel is not added to the sounding length: that length is already the turning plane. While the lip is driving, the flare pole stays a wire, because a match at \(c/(2L)\) sits far under the note. \(0.6133a\) is not applied. The radius was not reduced to enter a window. A straight-pipe Levine–Schwinger load at the published mouth, on a cone that never opens to it, is the load that missed high; it is not this horn.

Free decay is \(27.29\cdot\Delta f\) dB/s. Li, Almeida, Smith, and Wolfe, JASA 139:825 (2016), Eq. 3: \(27.29 = 20\log_{10}(e)\cdot\pi\), and \(\Delta f\) is the half-power bandwidth. The Goertzel slope of the ringing partial, read from the rendered samples that are written to the WAV, has to sit within ±20% of \(-27.29\cdot\Delta f\) at the impedance peak. The ±20% is UNSOURCED. Q is \(f_\mathrm{peak}/\Delta f\).

Published windows, not fitted. Trumpet: mode 4, 10.9–13.9 Hz (Fréour et al., JASA 148:748, 2020, eight-horn mean Q 42.6 at 464.7 Hz, Δf 10.9 Hz, about 298 dB/s; Petiot, Poirson, and Gilbert, 2004, one horn Q 32–38). Mode 8, 21.6–35.7 Hz (Petiot, Q 26–43 near 929 Hz). No Promenade note is mode 8. Trombone: Ablitzer, Acta Acustica 5:53 (2021), Tables 1–2, per mode, the ±20% windows already listed above, modes 2–9 only. The loose 4.8–14 Hz span is not the bar. Valve combinations below are the ordinary Bb-trumpet chart and are UNSOURCED. Slide positions are the Bb-tenor positions of those harmonics.

Each row is a graded excerpt note, at least 0.45 s, on the excerpt bore, then a 0.55 s tail. Repeated fingerings measured again and agreed. Files: `mmath-trumpet-breath-release.wav` and `mmath-trombone-breath-release.wav`.

Trumpet, throat \(1.09\times 10^{-4}\,\mathrm{m}^2\), apex 0.26 m, Bach #37 mouth 0.06112 m on the 0.619 m Bessel horn, 2800 Pa.

| Note | Fingering | Mode | Window (Hz) | Peak Hz | Q | Δf (Hz) | WAV (dB/s) | −27.29·Δf | Ratio |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|
| G4 | 1+2 | — | none | 356.4 | 32.5 | 10.98 | −289.7 | −299.6 | 0.967 |
| F4 | open | — | none | 317.3 | 31.1 | 10.20 | −250.4 | −278.4 | 0.899 |
| Bb4 | open | 4 | 10.9–13.9 | 443.0 | 35.7 | 12.42 | −327.6 | −338.9 | 0.967 |
| D5 | open | — | none | 571.1 | 38.2 | 14.94 | −404.2 | −407.6 | 0.992 |
| C5 | 1 | — | none | 508.8 | 37.9 | 13.41 | −342.7 | −365.9 | 0.936 |

Bb4 is inside 10.9–13.9 Hz. Played pitch on that fingering is 461.5 Hz, −17 cents. Q 35.7 sits in Petiot's 32–38. No graded note is trumpet mode 8.

Trombone, cone area \(3.0\times 10^{-4}\,\mathrm{m}^2\), Kirchhoff radius the Bach 42 .547 in slide, apex 0.60 m, Scott's horn ended at the Bach 42 mouth 0.10795 m, 1600 Pa.

| Note | Position | Mode | Window (Hz) | Peak Hz | Q | Δf (Hz) | WAV (dB/s) | −27.29·Δf | Ratio |
|---|---|---:|---|---:|---:|---:|---:|---:|---:|
| Bb4 | 1st | 8 | 8.90–14.24 | 459.3 | 38.9 | 11.81 | −326.5 | −322.2 | 1.013 |
| Bb4 later | 1st | 8 | 8.90–14.24 | 459.3 | 38.9 | 11.81 | −325.6 | −322.2 | 1.011 |
| G4 | 2nd | 7 | 7.54–12.10 | 384.3 | 37.8 | 10.17 | −281.5 | −277.6 | 1.014 |
| E4 | 2nd | 6 | 6.96–10.79 | 318.7 | 35.7 | 8.94 | −239.9 | −243.9 | 0.984 |
| Db5 | — | — | none (not 2–9) | 550.2 | 41.5 | 13.24 | −363.9 | −361.4 | 1.007 |

Bb4, G4, and E4 are inside their per-mode windows. Q on those three is 35.7–38.9, inside the published 24–40. Db5, 554 Hz, is not an integer partial in modes 2–9 of a Bb tenor slide, so it has no Ablitzer window. Both graded Db5s measured the same slope. Played E4 is 328.8 Hz, −5 cents.

A rest, and the 0.50 s after the score, stop the breath the same way: mouth pressure is zero, the lip seals, and the wall is Kirchhoff at the note's peak until the next blow restores the mild log-mean pole. Woodwinds have no lip resonance and do not take this path. Saxophone is a cone, and its lip frequency is zero, so it stays on the reed.

## Still planned

A tongue back on the mouthpiece, ending a brass note in a few lip
cycles, is on hold. The rings above stop the breath and leave the bore.
The audio-rate bow, felt hammer, mallet, bore valve, and cone valve
write samples. Clarinet, flute, and saxophone close excitation, bore,
and bell radiation. Harp, harpsichord, and classical guitar close a
pluck, a waveguide, a modal soundboard, and monopole radiation. The
three toms close a mallet, both heads, a closed cavity, and monopole
radiation. Trumpet and trombone close the lip, the cup, the cone, and
the open end. The four bowed strings close the bow, the waveguide, a
modal soundboard, and monopole radiation. The piano closes a felt
hammer, a stiff waveguide, a modal soundboard, and monopole radiation.
The bass-drum port and snare wires are still outside those steppers.
Cymbal nonlinearity is not discretized: no cubic coefficient is stored.
Images after the first from each hammer end are not in this layer. A
viscous reed-channel flow law is not in this layer. The saxophone adds
a zero-mean fraction of the radiated pressure after the loop; that
fraction is an argument. A lip skips it. Brassiness is a fraction of
the cylindrical Burgers advance at the bell and is not written back
into the bore. Clarinet, flute, and
saxophone holes are a lattice of caller radii; a register key is one
open hole, and no fingering chart is stored. Viscous wall loss and
hole interaction are not in that lattice. The valve applies Kirchhoff
loss only when the caller sets a reflection corner. A positive
wall-match frequency fits that loss on the throat at the named
frequency; zero keeps the log-mean radius. A non-radiating
flute can add an end correction; embouchure-hole radiation and the
spatial growth of the jet are not in this layer. Trumpet valves and the trombone F attachment are an extra
length on the cone. A caller Bessel area exponent is the flare and is
not a chain of cones. A measured bell curve and a stored slide chart are
not. A saxophone mouthpiece volume beyond the apex distance is not in
this layer. Full-instrument complete stays false for every row. The
stick-slip capture still uses rigid ends. Snare wires, shell air, a bass-drum port, and a hi-hat clutch
gap are not in the drum kit. A double-slip wolf cycle is not stepped. A
measured classical-guitar mode table and a fret table are not in this layer. An
electric-guitar pickup and the other 83 piano keys are not in this layer.
