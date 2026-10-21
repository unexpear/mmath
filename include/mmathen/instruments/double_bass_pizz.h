#pragma once
// Double bass pizzicato: the Guettler orchestral courses of double_bass.h
// plucked by the finger and radiated through a measured-band body
// (docs/48, band pro check). Physics: SoundboardVoice (tuned waveguide,
// yielding bridge, per-mode monopole areas).
//
// Body modes. A. Askenfelt, "Eigenmodes and tone quality of the double
// bass", STL-QPSR 23(4), 149-174 (1982): typical Helmholtz air mode A0
// 65 Hz and first top-plate mode T1 110 Hz for basses of normal size; the
// input admittance then shows "almost equally high and equally spaced"
// plate resonances from 150 to 400 Hz and, above 400 Hz, two wide bridge
// humps. A. W. Brown, ISMA 2003 / ISMA 2004 (Institut fuer Wiener
// Klangstil): A0 65-67 Hz, T1/A1 115 Hz, suspected A2/C3 150-158 Hz.
// Frequencies here: A0 65, T1 115 (Brown), C3 155 (cited); 200, 250, 300, 350 Hz
// as the evenly spaced 150-400 Hz plate set (VERIFY: positions are a
// reading of that sentence, not a table); bridge humps 450 and 900 Hz
// (VERIFY: Askenfelt's figure, not a table). Q, effective mass and area of
// every mode are TUNED to the VSCO 2 CE solo contrabass pizzicato (Sam
// Gossner, CC0, Strings/Solo Contrabass/Pizz): spectral centroid and H2-H8
// re H1 per note (tools/demo/band, bassref.py).
//
// String loss: two-point lossFromT60 at f0 and 6 f0, TUNED to the same
// recordings (fundamental T60 2-6 s, median about 3.5 s).
//
// Pluck: the finger plucks near the end of the fingerboard, VERIFY;
// kDoubleBassPizzFromBridgeM is that distance (UNSOURCED, 0.30 m of the
// 1.06 m string). A note ends when the left finger lifts or the right hand
// damps: kDoubleBassPizzDampSec (UNSOURCED).
#include <array>
#include <cmath>
#include <span>
#include "double_bass.h"
#include "body.h"
#include "plucked_string.h"
#include "soundboard_voice.h"

namespace mmathen {

inline constexpr double kDoubleBassPizzFromBridgeM = 0.30;
inline constexpr double kDoubleBassPizzDampSec = 0.04;
inline constexpr double kDoubleBassPizzT60AtF0 = 3.5;   // TUNED, VSCO 2 CE
inline constexpr double kDoubleBassPizzT60At6F0 = 1.8;  // TUNED, VSCO 2 CE
// A stopped string is damped at the stop by the finger pad as well as by
// the string; its upper partials die sooner. TUNED, VSCO 2 CE C#3-B3.
inline constexpr double kDoubleBassPizzStoppedT60At6F0 = 0.6;  // sweep 1.8/0.6/0.4 s: C#3-B3 centroid over the reference +0.40/+0.23/+0.18 oct, H2-H6 median -0/-4/-9 dB (all notes)

struct DoubleBassBodyMode { double frequencyHz; double q; double effectiveMassKg; double areaM2; };
inline constexpr std::array<DoubleBassBodyMode, 9> kDoubleBassBodyModes{{
    // A0 Q 30 -> 15 (TUNED, jb8): at 30 the A0 ring held G1 (49 Hz, under
    // A0) +35 c sharp over 80-270 ms (VSCO G1 +2 / +10 c; 15: +16, 10: +14)
    // and left C2, on A0, 6.5 dB over D2 (VSCO 0.7 dB; 15: 4.9). Centroid
    // and H2-H6 re VSCO unchanged (+0.05 oct, -5.7 dB) at 30, 15 and 10.
    {65.0, 15.0, 1.2, -0.30},   // A0 Helmholtz (Askenfelt 1982), Q/M/A TUNED
    {115.0, 25.0, 0.8, 0.25},   // T1/A1 115 Hz (Brown 2003/2004; Askenfelt 110). At 110 Hz the mode sat on A2 and drained it to T60 0.8-0.9 s (ref 3.4) at 0.8 and 2.0 kg
    {155.0, 25.0, 1.5, 0.12},   // C3/A2 (Brown 2003/2004)
    {200.0, 25.0, 2.0, 0.08},   // plate set, VERIFY
    {250.0, 25.0, 2.2, 0.07},
    {300.0, 25.0, 2.5, 0.06},
    {350.0, 25.0, 2.8, 0.05},
    {450.0, 6.0, 1.5, 0.04},    // bridge hump, VERIFY
    {900.0, 6.0, 2.5, 0.02},    // bridge hump, VERIFY
}};

[[nodiscard]] inline std::array<BodyModeSpec, 9> doubleBassBodyModes() {
  std::array<BodyModeSpec, 9> out{};
  for (std::size_t i = 0; i < out.size(); ++i)
    out[i] = BodyModeSpec{kDoubleBassBodyModes[i].frequencyHz, kDoubleBassBodyModes[i].q,
                          kDoubleBassBodyModes[i].effectiveMassKg};
  return out;
}
[[nodiscard]] inline std::array<double, 9> doubleBassBodyAreasM2() {
  std::array<double, 9> out{};
  for (std::size_t i = 0; i < out.size(); ++i) out[i] = kDoubleBassBodyModes[i].areaM2;
  return out;
}

// Stopped course for a sounding frequency: the highest open string at or
// below it (lowest position), stopped by fingerStop. stringIndex 0 = E1.
struct DoubleBassNote { StringCourse course; int stringIndex; bool stopped = false; };
[[nodiscard]] inline Result<DoubleBassNote, QuantityError> doubleBassNote(
    const StringInstrument& bass, double hertz) {
  using R = Result<DoubleBassNote, QuantityError>;
  for (int i = kStringInstrumentCourses - 1; i >= 0; --i) {
    const StringCourse& open = bass.courses[static_cast<std::size_t>(i)];
    const double f0 = open.tuning.frequency.value();
    if (f0 <= hertz * 1.0005) {
      if (hertz / f0 < 1.0005) return R::success(DoubleBassNote{open, i});
      auto stopped = fingerStop(open, 1.0 - f0 / hertz);
      if (!stopped.ok()) return R::failure(stopped.error());
      return R::success(DoubleBassNote{stopped.value(), i, true});
    }
  }
  return R::failure(QuantityError::RangeError);
}

[[nodiscard]] inline Result<LossModel, QuantityError> doubleBassPizzLoss(
    const StringCourse& c, bool stopped = false,
    double stoppedT60At6F0 = kDoubleBassPizzStoppedT60At6F0) {
  auto ideal = courseString(c);
  if (!ideal.ok()) return Result<LossModel, QuantityError>::failure(ideal.error());
  const double f = c.tuning.frequency.value();
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{kDoubleBassPizzT60AtF0},
                     FrequencyHz{6.0 * f},
                     DurationSeconds{stopped ? stoppedT60At6F0 : kDoubleBassPizzT60At6F0});
}

// One pizzicato voice. amplitudeMeters is the finger's pull at the pluck.
[[nodiscard]] inline Result<SoundboardVoice, QuantityError> doubleBassPizzVoice(
    const DoubleBassNote& note, double amplitudeMeters, SampleRateHz rate,
    double distanceMeters = 1.0, double stoppedT60At6F0 = kDoubleBassPizzStoppedT60At6F0) {
  using R = Result<SoundboardVoice, QuantityError>;
  static const auto modes = doubleBassBodyModes();
  static const auto areas = doubleBassBodyAreasM2();
  auto loss = doubleBassPizzLoss(note.course, note.stopped, stoppedT60At6F0);
  if (!loss.ok()) return R::failure(loss.error());
  const double L = note.course.geometry.vibratingLength.value();
  double frac = kDoubleBassPizzFromBridgeM / L;
  if (frac > 0.5) frac = 0.5;
  // Excitation position is from the nut; the bridge end is 1 - frac.
  return SoundboardVoice::create(note.course, Excitation{ExcitationKind::Pluck, 1.0 - frac, amplitudeMeters},
                                 loss.value(), std::span<const BodyModeSpec>(modes),
                                 std::span<const double>(areas), distanceMeters, 101325.0, 293.15, rate);
}

}  // namespace mmathen
