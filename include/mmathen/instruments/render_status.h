#pragma once

// Which instruments currently meet the audible-physics bar.
//
// An instrument is done only when the physics that affects its sound
// is stepped in time and that stepper is what the WAV contains. A
// single-instant mouth, a modal formula, or an envelope is not enough.
// rendersAudio is that bar. render/voice.h pulls any IVoice into a
// WavSink. Plucked courses, a bowed course, a felt hammer, a mallet,
// and a bore valve each step. Clarinet, flute, and saxophone now step
// excitation, bore, and bell radiation in one voice. The brass cup,
// bridge feedback, and the drum's air cavity are still outside, so
// those rows stay false.
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
  const char* gap;
};

// Twenty-six rows: four bowed strings, harp, harpsichord, two guitars,
// the five-string piano bank counted as one instrument, five winds,
// six drum heads, six cymbals. The piano bank is one instrument
// because the five strings share one (absent) soundboard.
inline constexpr InstrumentRenderStatus kInstrumentRenderStatus[] = {
    {"violin", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "Bow and pizzicato step. Bridge feedback and radiation are absent."},
    {"viola", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "Bow and pizzicato step. Bridge feedback and radiation are absent."},
    {"cello", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "Bow and pizzicato step. Bridge feedback and radiation are absent."},
    {"double bass", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "Bow and pizzicato step. Bridge feedback and radiation are absent."},
    {"harp", "pluck position steps a waveguide",
     "lossy tuned waveguide; stiffness is a caller argument",
     "no soundboard", "none", false,
     "The plucked course steps. Soundboard and radiation are absent."},
    {"harpsichord", "pluck position steps a waveguide",
     "lossy tuned waveguide; stiffness is a caller argument",
     "no soundboard", "none", false,
     "The plucked course steps. Soundboard and radiation are absent."},
    {"classical guitar", "pluck position steps a waveguide",
     "lossy tuned waveguide on the 650 mm course",
     "no soundboard", "none", false,
     "The plucked course steps. Soundboard and radiation are absent."},
    {"electric guitar", "pluck position steps a waveguide",
     "lossy tuned waveguide on the 25.5 in course",
     "no body filter",
     "pickup position comb and coil velocity; no inductance", false,
     "String and pickup step. The body filter is absent."},
    {"piano", "felt hammer steps a stiff string",
     "modal stiff string, loss and strike are arguments",
     "no soundboard", "none", false,
     "The felt hammer steps a stiff string. Soundboard and radiation are absent."},
    {"clarinet", "inward reed steps a bore delay",
     "cylindrical delay; an open end replaces the constant magnitude", "none",
     "Levine-Schwinger open end; radius and end correction are arguments", true,
     "Tone holes and a register key are outside this bore."},
    {"flute", "jet steps an open embouchure",
     "open-open cylindrical delay", "none",
     "Levine-Schwinger open end at the far end; the embouchure stays an ideal release",
     true, "Tone holes are outside this bore."},
    {"trumpet", "outward lip steps a cone",
     "truncated cone of the trumpet length; the cup is not stepped", "none",
     "Levine-Schwinger open end on the cone", false,
     "The lip, cone, and open end step. The cup is absent."},
    {"trombone", "outward lip steps the slide length on a cone",
     "truncated cone at L = L0 + 2x; the cup is not stepped", "none",
     "Levine-Schwinger open end on the cone", false,
     "The lip, slide cone, and open end step. The cup is absent."},
    {"saxophone", "inward reed steps the spherical-wave cone",
     "truncated-cone delay; the cylinder is not this stepper",
     "none", "Levine-Schwinger open end; bell radius is an argument", true,
     "Tone holes are outside this cone."},
    {"bass drum", "felt mallet steps the head",
     "damped membrane modes; air cavity not loaded",
     "shell air not loaded", "none", false,
     "Mallet and head step. Air cavity and radiation are absent."},
    {"rack tom 10 in", "felt mallet steps the head",
     "damped membrane modes; air cavity not loaded",
     "shell air not loaded", "none", false,
     "Mallet and head step. Air cavity and radiation are absent."},
    {"rack tom 12 in", "felt mallet steps the head",
     "damped membrane modes; air cavity not loaded",
     "shell air not loaded", "none", false,
     "Mallet and head step. Air cavity and radiation are absent."},
    {"floor tom", "felt mallet steps the head",
     "damped membrane modes; air cavity not loaded",
     "shell air not loaded", "none", false,
     "Mallet and head step. Air cavity and radiation are absent."},
    {"snare 5.5 in", "felt mallet steps one head",
     "damped membrane modes; snare wires absent",
     "shell air not loaded", "none", false,
     "Mallet and one head step. Wires, cavity, and radiation are absent."},
    {"snare 6.5 in", "felt mallet steps one head",
     "damped membrane modes; snare wires absent",
     "shell air not loaded", "none", false,
     "Mallet and one head step. Wires, cavity, and radiation are absent."},
    {"splash", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
    {"hi-hat", "felt mallet steps one free plate",
     "damped Kirchhoff mode; plates uncoupled", "none", "none", false,
     "Mallet and one plate step. The pair, nonlinearity, and radiation are absent."},
    {"crash 16 in", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
    {"crash 18 in", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false,
     "Mallet and plate step. Nonlinearity and radiation are absent."},
    {"china", "felt mallet steps a free plate",
     "damped Kirchhoff mode; flange not modeled", "none", "none", false,
     "Mallet and plate step. The flange, nonlinearity, and radiation are absent."},
    {"ride", "felt mallet steps a free plate",
     "damped Kirchhoff mode", "none", "none", false,
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

}  // namespace mmathen
