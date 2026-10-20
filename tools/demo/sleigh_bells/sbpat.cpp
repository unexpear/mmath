// Render a sleigh-bell strap from a gesture timeline file (see
// parseSleighBellTimeline in sleigh_bell.h) to raw float64 mono.
// usage: sbpat <timeline.txt> <out.f64> [seconds] [bells] [seed] [sinusoid 0/1]
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include "mmathen/instruments/sleigh_bell.h"
int main(int argc, char** argv) {
  if (argc < 3) return 1;
  std::ifstream in(argv[1]);
  std::stringstream ss; ss << in.rdbuf();
  const auto tl = mmathen::parseSleighBellTimeline(ss.str());
  const double secs = argc > 3 ? std::atof(argv[3]) : 9.8125;
  const int bells = argc > 4 ? std::atoi(argv[4]) : 12;
  const unsigned long long seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 7;
  const double fs = 44100.0;
  mmathen::SleighBellDesign d;
  if (const char* v = std::getenv("SB_LAG")) d.strapLagS = std::atof(v);
  if (const char* v = std::getenv("SB_CLICK")) d.clickGain = std::atof(v);
  if (const char* v = std::getenv("SB_E")) d.restitution = std::atof(v);
  mmathen::SleighBellStrap strap(bells, seed, fs, d);
  std::vector<double> y(std::size_t(secs * fs));
  mmathen::ShakeGesture shape;
  if (argc > 6) shape.minimumJerkStrokes = std::atoi(argv[6]) == 0;
  strap.render(y.data(), y.size(), tl, shape);
  std::FILE* f = std::fopen(argv[2], "wb");
  std::fwrite(y.data(), sizeof(double), y.size(), f);
  std::fclose(f);
  std::printf("%zu events, %llu impacts\n", tl.size(), (unsigned long long)strap.impacts());
}
