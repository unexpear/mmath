#pragma once
#include <mmathen/analysis/chords.h>

namespace app {
// Selected on development recordings, then frozen for the 60-track holdout.
// Keep the public library defaults stable; this is the desktop guitar profile.
inline mmathen::ChordOptions guitarChordOptions() {
  mmathen::ChordOptions o;
  o.chroma.fMaxHz = 2500;
  o.chroma.compression = 10;
  return o;
}
// Guitar retains its validated range. Music includes all 88 piano keys with
// margin around A0/C8; the upper limit still depends on the source sample rate.
enum class PitchRange { Guitar, Music };
inline double tunerMinimum(PitchRange range) { return range == PitchRange::Music ? 25 : 50; }
inline double tunerMaximum(PitchRange range, double rate) {
  return std::min(range == PitchRange::Music ? 4500.0 : 1500.0, .45 * rate);
}
inline std::size_t tunerFrameSize(double rate, PitchRange range = PitchRange::Guitar) {
  if (!std::isfinite(rate) || rate <= 0) return 2048;
  // 2048 at 44.1/48 kHz. Preserve enough samples for two periods of 50 Hz
  // at higher source rates, including the lag interpolator's neighbours.
  return std::max<std::size_t>(2048, 2 * static_cast<std::size_t>(std::ceil(rate / tunerMinimum(range))) + 4);
}
}
