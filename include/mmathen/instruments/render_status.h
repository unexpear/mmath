#pragma once

// Which instruments currently meet the audible-physics bar.
//
// An instrument is done only when the physics that affects its sound
// is stepped in time and that stepper is what the WAV contains. A
// single-instant mouth, a modal formula, or an envelope is not enough.
// rendersAudio is the audio column: the stepper produces samples.
// fullInstrumentComplete is stricter. It is true only when the full
// instrument and the physics that shape its sound are in that stepper.
// None of these rows are complete: viscous wall loss, a measured bell
// curve, snare wires, a bass-drum port, cymbal nonlinearity, an
// electric-guitar body, unison courses, and a wolf note are still out.
// render/voice.h pulls any IVoice into a
// WavSink. Plucked courses, a bowed course, a felt hammer, a mallet,
// and a bore valve each step. Clarinet, flute, and saxophone step
// excitation, bore, and bell radiation in one voice. Trumpet and
// trombone step the lip, the cup compliance, the cone, and the open
// end in one voice. Violin, viola, cello, and double bass step the
// bow, the lossy waveguide, a yielding modal soundboard, and a
// compact monopole in one voice. Harp, harpsichord, and classical
// guitar step a pluck through that same soundboard. The piano steps
// a felt hammer, a stiff waveguide, that soundboard, and the same
// monopole. The three toms step a mallet, both heads, a closed
// cavity, and a monopole. The bass-drum port and snare wires stay
// outside, so those rows stay false.
//
// The strings are the status, not stored physical constants.

#include <cstddef>

namespace mmathen {

struct InstrumentRenderStatus {
  const char* name;
  const char* excitation;
  const char* resonator;
  const char* body;
  const char* radiation;
  bool rendersAudio;
  bool fullInstrumentComplete;
  const char* gap;
};

// Twenty-six rows: four bowed strings, harp, harpsichord, two guitars,
// the five-string piano bank counted as one instrument, five winds,
// six drum heads, six cymbals. The piano bank is one instrument
// because the five strings share one (absent) soundboard.
inline constexpr InstrumentRenderStatus kInstrumentRenderStatus[] = {
    {"violin", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "bow closes on a modal soundboard; bridge force is the drive",
     "compact monopole of the bowed bridge acceleration", true, false,
     "Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored."},
    {"viola", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "bow closes on a modal soundboard; bridge force is the drive",
     "compact monopole of the bowed bridge acceleration", true, false,
     "Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored."},
    {"cello", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "bow closes on a modal soundboard; bridge force is the drive",
     "compact monopole of the bowed bridge acceleration", true, false,
     "Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored."},
    {"double bass", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "bow closes on a modal soundboard; bridge force is the drive",
     "compact monopole of the bowed bridge acceleration", true, false,
     "Mode frequency, Q, and mass are arguments. A wolf note and A0 are not stored."},
    {"harp", "pluck position steps a waveguide",
     "lossy tuned waveguide; stiffness is a caller argument",
     "modal soundboard driven by bridge force, with feedback",
     "compact monopole of the bridge acceleration; area and distance are arguments",
     true, false,
     "Mode frequency, Q, and mass are arguments. Other courses are not coupled."},
    {"harpsichord", "pluck position steps a waveguide",
     "lossy tuned waveguide; stiffness is a caller argument",
     "modal soundboard driven by bridge force, with feedback",
     "compact monopole of the bridge acceleration; area and distance are arguments",
     true, false,
     "Mode frequency, Q, and mass are arguments. Other courses are not coupled."},
    {"classical guitar", "pluck position steps a waveguide",
     "lossy tuned waveguide on the 650 mm course",
     "modal soundboard driven by bridge force, with feedback",
     "compact monopole of the bridge acceleration; area and distance are arguments",
     true, false,
     "Mode frequency, Q, and mass are arguments. Other courses are not coupled."},
    {"electric guitar", "pluck position steps a waveguide",
     "lossy tuned waveguide on the 25.5 in course",
     "no body filter",
     "pickup position comb and coil velocity; no inductance", false, false,
     "String and pickup step. The body filter is absent."},
    {"piano", "felt hammer steps a stiff waveguide",
     "lossy stiff tuned waveguide; strike and stiffness are arguments",
     "hammer closes on a modal soundboard; bridge force is the drive",
     "compact monopole of the hammered bridge acceleration", true, false,
     "Mode frequency, Q, and mass are arguments. The other four courses are not coupled."},
    {"clarinet", "inward reed steps a bore delay",
     "cylindrical delay; an open end replaces the constant magnitude", "none",
     "Levine-Schwinger open end; radius and end correction are arguments", true, false,
     "Caller holes and a register key sit on the lattice. Viscous wall loss is outside this bore."},
    {"flute", "jet steps an open embouchure",
     "open-open cylindrical delay", "none",
     "Levine-Schwinger open end at the far end; the embouchure stays an ideal release",
     true, false, "Caller tone holes sit on the lattice. Viscous wall loss is outside this bore."},
    {"trumpet", "outward lip steps a cone with the cup",
     "truncated cone; cup compliance C dp/dt = U_lip - U_bore", "none",
     "Levine-Schwinger open end on the cone", true, false,
     "Engaged valve lengths are an argument. A caller Bessel area exponent is the flare. No measured bell curve is stored."},
    {"trombone", "outward lip steps the slide length on a cone with the cup",
     "truncated cone at L = L0 + 2x; cup compliance is stepped", "none",
     "Levine-Schwinger open end on the cone", true, false,
     "The F-attachment length is an argument. A caller Bessel area exponent is the flare. No measured slide chart is stored."},
    {"saxophone", "inward reed steps the spherical-wave cone",
     "truncated-cone delay; the cylinder is not this stepper",
     "none", "Levine-Schwinger open end; bell radius is an argument", true, false,
     "Caller tone holes sit on the stepped cone. Viscous wall loss is outside this bore."},
    {"bass drum", "felt mallet steps the batter head",
     "damped membrane modes; a port is not opened",
     "closed cavity, not a ported shell",
     "monopole of the batter volume acceleration", false, false,
     "Two heads and a closed cavity radiate. The port is absent."},
    {"rack tom 10 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads",
     "closed cavity couples the heads; shell depth is the volume",
     "monopole of the batter volume acceleration", true, false,
     "Head tension and Q are arguments. No port is in this shell."},
    {"rack tom 12 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads",
     "closed cavity couples the heads; shell depth is the volume",
     "monopole of the batter volume acceleration", true, false,
     "Head tension and Q are arguments. No port is in this shell."},
    {"floor tom", "felt mallet steps the batter head",
     "damped membrane modes of both heads",
     "closed cavity couples the heads; shell depth is the volume",
     "monopole of the batter volume acceleration", true, false,
     "Head tension and Q are arguments. No port is in this shell."},
    {"snare 5.5 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads; snare wires absent",
     "closed cavity couples the heads",
     "monopole of the batter volume acceleration", false, false,
     "Two heads, the cavity, and radiation step. Snare wires are absent."},
    {"snare 6.5 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads; snare wires absent",
     "closed cavity couples the heads",
     "monopole of the batter volume acceleration", false, false,
     "Two heads, the cavity, and radiation step. Snare wires are absent."},
    {"splash", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false, false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
    {"hi-hat", "felt mallet steps one free plate",
     "damped Kirchhoff mode; plates uncoupled", "none", "none", false, false,
     "Mallet and one plate step. The pair, nonlinearity, and radiation are absent."},
    {"crash 16 in", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false, false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
    {"crash 18 in", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false, false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
    {"china", "felt mallet steps a free plate",
     "damped Kirchhoff mode; flange not modeled", "none", "none", false, false,
     "Mallet and plate step. The flange, nonlinearity, and radiation are absent."},
    {"ride", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false, false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
};

inline constexpr std::size_t kInstrumentRenderStatusCount =
    sizeof(kInstrumentRenderStatus) / sizeof(kInstrumentRenderStatus[0]);

[[nodiscard]] inline constexpr std::size_t instrumentsThatRenderAudio() {
  std::size_t n = 0;
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (kInstrumentRenderStatus[i].rendersAudio) ++n;
  }
  return n;
}

[[nodiscard]] inline constexpr std::size_t instrumentsFullyComplete() {
  std::size_t n = 0;
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (kInstrumentRenderStatus[i].fullInstrumentComplete) ++n;
  }
  return n;
}

}  // namespace mmathen
