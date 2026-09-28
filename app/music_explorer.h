#pragma once
#include <array>
#include <numbers>
#include <vector>
#include <mmathen/sink/wav_reader.h>
#include "tuner_controls.h"

namespace app::music {
struct Quality { const char* name; std::array<int,4> steps; int count; };
inline constexpr Quality qualities[] = {
  {"Major", {0,4,7,0}, 3}, {"Minor", {0,3,7,0}, 3},
  {"Dominant 7", {0,4,7,10}, 4}, {"Major 7", {0,4,7,11}, 4},
  {"Minor 7", {0,3,7,10}, 4}, {"Diminished", {0,3,6,0}, 3},
  {"Suspended 2", {0,2,7,0}, 3}, {"Suspended 4", {0,5,7,0}, 3}
};
inline bool blackKey(int midi) {
  const int pc=midi%12;
  return pc==1 || pc==3 || pc==6 || pc==8 || pc==10;
}
// Root/quality are chosen by the user for study, never fed to recognition.
inline std::vector<int> voicing(int root, int quality, int inversion) {
  if(root<21 || root>108 || quality<0 || quality>=static_cast<int>(std::size(qualities))) return {};
  const auto& q=qualities[quality];
  if(inversion<0 || inversion>=q.count) return {};
  std::vector<int> notes;
  for(int i=0;i<q.count;++i) {
    const int midi=root+q.steps[static_cast<std::size_t>(i)]+(i<inversion?12:0);
    if(midi>108) return {};
    notes.push_back(midi);
  }
  std::sort(notes.begin(),notes.end());
  return notes;
}
inline const char* interval(int steps) {
  constexpr const char* names[]{"Unison","Minor 2nd","Major 2nd","Minor 3rd","Major 3rd","Perfect 4th",
    "Tritone","Perfect 5th","Minor 6th","Major 6th","Minor 7th","Major 7th","Octave"};
  return steps>=0 && steps<=12 ? names[steps] : "Compound interval";
}
// Deliberately plain reference tones, not a claim to synthesize a real piano.
inline mmathen::WavData referenceTones(const std::vector<int>& notes, double a4=440) {
  mmathen::WavData w;
  if(notes.empty() || notes.size()>4 || !std::isfinite(a4) || a4<430 || a4>450 ||
      std::any_of(notes.begin(),notes.end(),[](int n){return n<21 || n>108;})) return w;
  constexpr double rate=48000, duration=2;
  w.rate=mmathen::SampleRateHz{rate}; w.channels=1; w.bitsPerSample=32; w.isFloat=true;
  w.frames=static_cast<std::size_t>(rate*duration); w.samples.resize(w.frames);
  for(std::size_t i=0;i<w.frames;++i) {
    const double t=static_cast<double>(i)/rate;
    const double envelope=std::clamp(std::min(t/.015,(duration-t)/.2),0.0,1.0);
    double value=0;
    for(int note:notes) value+=std::sin(2*std::numbers::pi*tuner::frequency(note,a4)*t);
    w.samples[i]=.3*envelope*value/static_cast<double>(notes.size());
  }
  return w;
}
}
