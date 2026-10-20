// Sleigh-bell pro check 2 harness: minimum-jerk strokes with pauses (engine default).
// Usage: sb1 out.f64 seconds bells seed [amplitudeM] [rateHz] [sampleRate] [lateral] [sinusoid 0/1]
#include <mmathen/instruments/sleigh_bell.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mmathen;
int main(int argc, char** argv) {
  const double sec = std::atof(argv[2]); const int bells = std::atoi(argv[3]); const auto seed = (std::uint64_t)std::atoll(argv[4]);
  const double fs = argc > 7 ? std::atof(argv[7]) : 44100.0;
  ShakeGesture g; if (argc > 5) g.amplitudeM = std::atof(argv[5]); if (argc > 6) g.rateHz = std::atof(argv[6]); if (argc > 8) g.lateralFraction = std::atof(argv[8]); if (argc > 9) g.minimumJerkStrokes = std::atoi(argv[9]) == 0;
  SleighBellStrap s(bells, seed, fs); std::vector<double> x((size_t)(sec * fs)); s.render(x.data(), x.size(), g);
  std::printf("impacts/s/bell %.2f ear %.2f mm\n", s.impacts() / sec / bells, s.earLengthM() * 1e3);
  FILE* o = std::fopen(argv[1], "wb"); std::fwrite(x.data(), 8, x.size(), o); std::fclose(o);
}
