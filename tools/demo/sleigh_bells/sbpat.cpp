// Render a sleigh-bell strap from a gesture timeline file (see
// parseSleighBellTimeline in sleigh_bell.h) to raw float64 mono.
// usage: sbpat <timeline.txt> <out.f64> [seconds] [bells] [seed] [sinusoid 0/1]
// SB_PROFILE=1 plays the timeline through sleighBellPlayerProfile()
// (player_profile.h); without it the timeline is rendered as written.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include "mmathen/instruments/sleigh_bell.h"
#include "mmathen/instruments/player_profile.h"
int main(int argc, char** argv) {
  if (argc < 3) return 1;
  std::ifstream in(argv[1]);
  std::stringstream ss; ss << in.rdbuf();
  auto tl = mmathen::parseSleighBellTimeline(ss.str());
  if (const char* v = std::getenv("SB_PROFILE"); v && std::atoi(v) == 1) {
    const auto played = mmathen::performSleighBells(tl, mmathen::sleighBellPlayerProfile());
    double s2 = 0; int n = 0;
    for (std::size_t i = 0; i < tl.size(); ++i) if (tl[i].kind == mmathen::SleighBellEvent::Kind::Strike) { const double d = played[i].timeSec - tl[i].timeSec; s2 += d * d; ++n; }
    std::printf("sleigh performed: %d strikes, onset shift rms %.2f ms\n", n, n ? 1e3 * std::sqrt(s2 / n) : 0.0);
    tl = played;
  }
  const double secs = argc > 3 ? std::atof(argv[3]) : 9.8125;
  const int bells = argc > 4 ? std::atoi(argv[4]) : 12;
  const unsigned long long seed = argc > 5 ? std::strtoull(argv[5], nullptr, 10) : 7;
  const double fs = 44100.0;
  mmathen::SleighBellDesign d;
  if (const char* v = std::getenv("SB_LAG")) d.strapLagS = std::atof(v);
  if (const char* v = std::getenv("SB_CLICK")) d.clickGain = std::atof(v);
  if (const char* v = std::getenv("SB_E")) d.restitution = std::atof(v);
  if (const char* v = std::getenv("SB_STRAP")) d.strapHz = std::atof(v);
  if (const char* v = std::getenv("SB_ZETA")) d.strapZeta = std::atof(v);
  if (const char* v = std::getenv("SB_GAP")) d.bellGapM = std::atof(v);
  if (const char* v = std::getenv("SB_DAMP")) d.dampingFactor = std::atof(v);
  if (const char* v = std::getenv("SB_FULL")) d.fullRing = std::atoi(v) != 0;
  mmathen::SleighBellStrap strap(bells, seed, fs, d);
  std::vector<double> y(std::size_t(secs * fs));
  mmathen::ShakeGesture shape;
  if (argc > 6) shape.minimumJerkStrokes = std::atoi(argv[6]) == 0;
  strap.render(y.data(), y.size(), tl, shape);
  std::FILE* f = std::fopen(argv[2], "wb");
  std::fwrite(y.data(), sizeof(double), y.size(), f);
  std::fclose(f);
  std::printf("%zu events, %llu impacts, %llu knocks\n", tl.size(), (unsigned long long)strap.impacts(), (unsigned long long)strap.knocks());
}
