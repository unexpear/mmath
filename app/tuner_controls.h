#pragma once
// Presentation/target helpers adapted from StandardTune (MIT); see
// third_party/standardtune. The pitch detector never receives these targets.
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include "chord_shapes.h"

namespace app::tuner {
struct Tuning { const char* name; std::array<int, 6> midi; };
inline constexpr Tuning tunings[] = {
  {"Standard E", {40, 45, 50, 55, 59, 64}},
  {"Drop D", {38, 45, 50, 55, 59, 64}},
  {"Open G", {38, 43, 50, 55, 59, 62}},
  {"Open D", {38, 45, 50, 54, 57, 62}},
  {"Open C", {36, 43, 48, 55, 60, 64}},
  {"Open E", {40, 47, 52, 56, 59, 64}},
  {"DADGAD", {38, 45, 50, 55, 57, 62}},
  {"Half step down", {39, 44, 49, 54, 58, 63}},
};
inline constexpr const char* pitchNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
inline double frequency(int midi, double a4 = 440) { return a4 * std::pow(2.0, (midi - 69) / 12.0); }
inline std::string note(int midi) {
  if (midi < 0 || midi > 127) return "--";
  return std::string(pitchNames[midi % 12]) + std::to_string(midi / 12 - 1);
}
inline double cents(double hz, double target) {
  if (!std::isfinite(hz) || !std::isfinite(target) || hz <= 0 || target <= 0)
    return std::numeric_limits<double>::quiet_NaN();
  return 1200 * std::log2(hz / target);
}
struct Reading {
  bool valid = false;
  int midi = -1, string = -1;
  double rawHz = 0, hz = 0, targetHz = 0, deviation = 0, harmonic = 1;
};
inline Reading read(double hz, bool voiced, int tuning, int selectedString,
                    double a4, bool assist) {
  Reading r;
  if (!voiced || !std::isfinite(hz) || hz <= 0 || !std::isfinite(a4) || a4 < 430 || a4 > 450)
    return r;
  const double midi = 69 + 12 * std::log2(hz / a4);
  if (!std::isfinite(midi) || midi < 0 || midi > 127) return r;
  r.valid = true; r.rawHz = r.hz = hz; r.midi = static_cast<int>(std::lround(midi));
  if (tuning >= 0 && tuning < static_cast<int>(std::size(tunings))) {
    const auto& t = tunings[tuning];
    if (selectedString >= 0 && selectedString < 6) r.string = selectedString;
    else {
      double best = std::numeric_limits<double>::infinity();
      for (int i = 0; i < 6; ++i) {
        const double distance = std::abs(cents(hz, frequency(t.midi[i], a4)));
        if (distance < best) { best = distance; r.string = i; }
      }
    }
    r.targetHz = frequency(t.midi[r.string], a4);
    if (assist && selectedString >= 0 && selectedString < 6) {
      double best = std::abs(cents(hz, r.targetHz));
      for (double ratio : {2.0, 3.0, 4.0, 0.5}) {
        const double distance = std::abs(cents(hz / ratio, r.targetHz));
        if (distance < best && distance <= 45) { best = distance; r.hz = hz / ratio; r.harmonic = ratio; }
      }
    }
  } else r.targetHz = frequency(r.midi, a4);
  r.deviation = cents(r.hz, r.targetHz);
  return r;
}
inline const char* verdict(const Reading& r, double tolerance) {
  if (!r.valid || !std::isfinite(r.deviation)) return "Waiting for a clear note";
  if (std::abs(r.deviation) <= std::clamp(tolerance, 1.0, 5.0)) return "In tune";
  return r.deviation < 0 ? "Flat - raise the pitch" : "Sharp - lower the pitch";
}
inline int firstFret(const Shape& s) {
  int low = 25, high = 0;
  for (int fret : s.frets) if (fret > 0) { low = std::min(low, fret); high = std::max(high, fret); }
  return high <= 5 ? 1 : low;
}
inline std::string shapeNotes(const Shape& s) {
  std::string result;
  for (int i = 0; i < 6; ++i) if (s.frets[i] >= 0) {
    if (!result.empty()) result += "  ";
    result += note(tunings[0].midi[i] + s.frets[i]);
  }
  return result;
}
// Convert a triad state to an authored voicing, accepting enharmonic spellings.
inline const Shape* shapeForState(int state) {
  if (state < 0 || state >= 24) return nullptr;
  const std::string desired = std::string(pitchNames[state % 12]) + (state >= 12 ? "m" : "");
  static constexpr const char* flat[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
  const std::string alternate = std::string(flat[state % 12]) + (state >= 12 ? "m" : "");
  for (const auto& s : shapes) if (s.name == desired || s.name == alternate) return &s;
  return nullptr;
}
}
