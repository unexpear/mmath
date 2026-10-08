#pragma once

// Which instruments currently meet the audible-physics bar.
//
// An instrument is done only when the physics that affects its sound
// is stepped in time and that stepper is what the WAV contains. A
// single-instant mouth, a modal formula, or an envelope is not enough.
// rendersAudio is that bar. Piece 1 of the render path (render/voice.h)
// can pull any IVoice into a WavSink. No instrument is an IVoice yet,
// so every row below is false.
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
    {"violin", "Helmholtz and rigid-end stick-slip, not at audio rate",
     "ideal string, not a stepped lossy waveguide",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "No voice. Pluck, bow, body, and radiation are not one stepped sound."},
    {"viola", "Helmholtz and rigid-end stick-slip, not at audio rate",
     "ideal string, not a stepped lossy waveguide",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "No voice. Pluck, bow, body, and radiation are not one stepped sound."},
    {"cello", "Helmholtz and rigid-end stick-slip, not at audio rate",
     "ideal string, not a stepped lossy waveguide",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "No voice. Pluck, bow, body, and radiation are not one stepped sound."},
    {"double bass", "Helmholtz and rigid-end stick-slip, not at audio rate",
     "ideal string, not a stepped lossy waveguide",
     "open-loop modal sum, no bridge feedback",
     "compact monopole formula, not driven by the string", false,
     "No voice. Pluck, bow, body, and radiation are not one stepped sound."},
    {"harp", "no time-stepped pluck",
     "ideal open strings, not a stepped lossy waveguide",
     "no soundboard", "none", false,
     "No voice. A plucked course does not yet step a waveguide."},
    {"harpsichord", "no time-stepped pluck",
     "published speaking lengths, not a stepped waveguide",
     "no soundboard", "none", false,
     "No voice. A plucked course does not yet step a waveguide."},
    {"classical guitar", "ideal-string pluck shape, not stepped",
     "ideal string on 650 mm, not a lossy waveguide",
     "no soundboard", "none", false,
     "No voice. A plucked course does not yet step a waveguide."},
    {"electric guitar", "ideal-string pluck shape, not stepped",
     "ideal string on 25.5 in, not a lossy waveguide",
     "no body filter", "no pickup", false,
     "No voice. No coil and no position comb on a stepped string."},
    {"piano", "hard-hammer and felt laws, one instant",
     "five stiff strings, not stepped against the hammer",
     "no soundboard", "none", false,
     "No voice. The hammer is not stepping a stiff string."},
    {"clarinet", "reed mouth, one pressure sample",
     "open cylinder, not a delay line", "none",
     "no bell radiation", false,
     "No voice. The reed is not stepping a bore."},
    {"flute", "jet formulas, one pressure sample",
     "open-open cylinder, not a delay line", "none",
     "no embouchure or end radiation", false,
     "No voice. The jet is not stepping a bore."},
    {"trumpet", "outward lip, one pressure sample",
     "cupped cylinder, not a delay line", "none",
     "no bell flare or radiation", false,
     "No voice. The lip is not stepping a bore."},
    {"trombone", "outward lip, one pressure sample",
     "U-slide cylinder, not a delay line", "none",
     "no bell flare or radiation", false,
     "No voice. The slide length is not a stepped bore."},
    {"saxophone", "inward reed, one pressure sample",
     "truncated-cone impedance, not a stepped cone", "none",
     "no bell radiation", false,
     "No voice. The reed is not stepping a cone."},
    {"bass drum", "impulse coefficients, not a mallet step",
     "fixed-membrane zeros, not a damped modal stepper",
     "shell air not loaded", "none", false,
     "No voice. The head is not stepping."},
    {"rack tom 10 in", "impulse coefficients, not a mallet step",
     "fixed-membrane zeros, not a damped modal stepper",
     "shell air not loaded", "none", false,
     "No voice. The head is not stepping."},
    {"rack tom 12 in", "impulse coefficients, not a mallet step",
     "fixed-membrane zeros, not a damped modal stepper",
     "shell air not loaded", "none", false,
     "No voice. The head is not stepping."},
    {"floor tom", "impulse coefficients, not a mallet step",
     "fixed-membrane zeros, not a damped modal stepper",
     "shell air not loaded", "none", false,
     "No voice. The head is not stepping."},
    {"snare 5.5 in", "impulse coefficients, not a mallet step",
     "one head, snare wires absent", "shell air not loaded", "none",
     false, "No voice. The head is not stepping."},
    {"snare 6.5 in", "impulse coefficients, not a mallet step",
     "one head, snare wires absent", "shell air not loaded", "none",
     false, "No voice. The head is not stepping."},
    {"splash", "impulse on a free plate, not stepped",
     "Kirchhoff eigenvalues, not a damped modal stepper", "none",
     "none", false, "No voice. The plate is not stepping."},
    {"hi-hat", "impulse on one free plate, plates uncoupled",
     "Kirchhoff eigenvalues, not a damped modal stepper", "none",
     "none", false, "No voice. The plate is not stepping."},
    {"crash 16 in", "impulse on a free plate, not stepped",
     "Kirchhoff eigenvalues, not a damped modal stepper", "none",
     "none", false, "No voice. The plate is not stepping."},
    {"crash 18 in", "impulse on a free plate, not stepped",
     "Kirchhoff eigenvalues, not a damped modal stepper", "none",
     "none", false, "No voice. The plate is not stepping."},
    {"china", "impulse on a free plate, flange not modeled",
     "Kirchhoff eigenvalues, not a damped modal stepper", "none",
     "none", false, "No voice. The plate is not stepping."},
    {"ride", "impulse on a free plate, not stepped",
     "Kirchhoff eigenvalues, not a damped modal stepper", "none",
     "none", false, "No voice. The plate is not stepping."},
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
