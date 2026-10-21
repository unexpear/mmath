// hb1: handbells F3, C6, G6, C7 (= Freesound 557142 F3 and radwoc c1, g1, c2; CC0).
//   hb1 out.f64 [speed] [sec]; env HK/HM clapper stiffness/mass (0 = engine defaults), HKB/HMB size exponents, HP listener angle from the axis (deg), HS swing speed m/s (setSwing; 0 = fixed clapper speed, -1 = kHandbellRingerSwingMps)
#include <mmathen/instruments/handbell.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mmathen;
static double E(const char* k, double d) { const char* v = std::getenv(k); return v ? std::atof(v) : d; }
int main(int argc, char** argv) {
  if (argc < 2) return 1;
  const double speed = argc > 2 ? std::atof(argv[2]) : 1.0, sec = argc > 3 ? std::atof(argv[3]) : 5.0, fs = 48000.0;
  const double hzs[] = {174.61, 1046.5, 1568.0, 2093.0};
  std::vector<double> all;
  for (double hz : hzs) {
    const double sc = hz / 174.61;
    auto v = HandbellVoice::create(hz, speed, SampleRateHz{fs}, 1.0, E("HK", 0.0) * std::pow(sc, E("HKB", 0.0)), 4,
                                   E("HM", 0.0) * std::pow(sc, -E("HMB", 0.0)), E("HP", 90.0) * constants::pi / 180.0);
    if (!v.ok()) { std::fprintf(stderr, "create %d\n", (int)v.error()); return 2; }
    auto voice = std::move(v).value();
    const double hs = E("HS", 0.0) < 0.0 ? kHandbellRingerSwingMps : E("HS", 0.0);
    if (hs > 0.0 && !voice.setSwing(hs)) return 4;
    std::vector<double> y((size_t)(sec * fs));
    if (!voice.renderBlock(y.data(), FrameCount{y.size()}).ok()) return 3;
    double pk = 0; for (double s : y) pk = std::max(pk, std::fabs(s));
    std::printf("f1 %.0f modes %zu contact %.2f ms peak %.3g Pa strikes %d strike %.2f m/s\n", hz, voice.modeCount(), voice.contactSeconds() * 1e3, pk, voice.strikeCount(), voice.strikeSpeedMps());
    all.insert(all.end(), y.begin(), y.end());
  }
  FILE* o = std::fopen(argv[1], "wb"); std::fwrite(all.data(), 8, all.size(), o); std::fclose(o);
}
