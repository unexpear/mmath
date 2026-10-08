#pragma once

// Which instruments currently meet the audible-physics bar.
//
// An instrument is done only when the physics that affects its sound
// is stepped in time and that stepper is what the WAV contains. A
// single-instant mouth, a modal formula, or an envelope is not enough.
// rendersAudio is that bar. render/voice.h pulls any IVoice into a
// WavSink. Plucked courses, a bowed course, a felt hammer, a mallet,
// and a bore valve each step. Clarinet, flute, and saxophone step
// excitation, bore, and bell radiation in one voice. Harp,
// harpsichord, and classical guitar step a pluck, a lossy waveguide,
// a yielding modal soundboard, and a compact monopole in one voice.
// The three toms step a mallet, both heads, a closed cavity, and a
// monopole. The bow, the felt hammer, the brass cup, the bass-drum
// port, and snare wires stay outside, so those rows stay false.
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
     "pizzicato can close on a modal soundboard; the bow cannot",
     "compact monopole of a plucked bridge; the bow does not drive it", false,
     "Pizzicato can radiate through a modal soundboard. The bow still reflects from a rigid bridge."},
    {"viola", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "pizzicato can close on a modal soundboard; the bow cannot",
     "compact monopole of a plucked bridge; the bow does not drive it", false,
     "Pizzicato can radiate through a modal soundboard. The bow still reflects from a rigid bridge."},
    {"cello", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "pizzicato can close on a modal soundboard; the bow cannot",
     "compact monopole of a plucked bridge; the bow does not drive it", false,
     "Pizzicato can radiate through a modal soundboard. The bow still reflects from a rigid bridge."},
    {"double bass", "bow friction and pizzicato both step",
     "lossy tuned waveguide; stiffness is a caller argument",
     "pizzicato can close on a modal soundboard; the bow cannot",
     "compact monopole of a plucked bridge; the bow does not drive it", false,
     "Pizzicato can radiate through a modal soundboard. The bow still reflects from a rigid bridge."},
    {"harp", "pluck position steps a waveguide",
     "lossy tuned waveguide; stiffness is a caller argument",
     "modal soundboard driven by bridge force, with feedback",
     "compact monopole of the bridge acceleration; area and distance are arguments",
     true,
     "Mode frequency, Q, and mass are arguments. Other courses are not coupled."},
    {"harpsichord", "pluck position steps a waveguide",
     "lossy tuned waveguide; stiffness is a caller argument",
     "modal soundboard driven by bridge force, with feedback",
     "compact monopole of the bridge acceleration; area and distance are arguments",
     true,
     "Mode frequency, Q, and mass are arguments. Other courses are not coupled."},
    {"classical guitar", "pluck position steps a waveguide",
     "lossy tuned waveguide on the 650 mm course",
     "modal soundboard driven by bridge force, with feedback",
     "compact monopole of the bridge acceleration; area and distance are arguments",
     true,
     "Mode frequency, Q, and mass are arguments. Other courses are not coupled."},
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
    {"bass drum", "felt mallet steps the batter head",
     "damped membrane modes; a port is not opened",
     "closed cavity, not a ported shell",
     "monopole of the batter volume acceleration", false,
     "Two heads and a closed cavity radiate. The port is absent."},
    {"rack tom 10 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads",
     "closed cavity couples the heads; shell depth is the volume",
     "monopole of the batter volume acceleration", true,
     "Head tension and Q are arguments. No port is in this shell."},
    {"rack tom 12 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads",
     "closed cavity couples the heads; shell depth is the volume",
     "monopole of the batter volume acceleration", true,
     "Head tension and Q are arguments. No port is in this shell."},
    {"floor tom", "felt mallet steps the batter head",
     "damped membrane modes of both heads",
     "closed cavity couples the heads; shell depth is the volume",
     "monopole of the batter volume acceleration", true,
     "Head tension and Q are arguments. No port is in this shell."},
    {"snare 5.5 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads; snare wires absent",
     "closed cavity couples the heads",
     "monopole of the batter volume acceleration", false,
     "Two heads, the cavity, and radiation step. Snare wires are absent."},
    {"snare 6.5 in", "felt mallet steps the batter head",
     "damped membrane modes of both heads; snare wires absent",
     "closed cavity couples the heads",
     "monopole of the batter volume acceleration", false,
     "Two heads, the cavity, and radiation step. Snare wires are absent."},
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
