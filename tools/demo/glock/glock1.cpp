// glock1: glockenspiel notes for the pro check against VSCO 2 CE
// glock_medium (sounding G5 C6 G6 C7 G7 C8 = the files' G4..C7 names).
//   glock1 out.f64 [speed m/s] [seconds per note]   (env GQ, GC, GL, GS override Q_int, dashpot, lambda, strike; GX off-centre metres)
#include <mmathen/instruments/glockenspiel.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mmathen;
static double E(const char* k, double d) { const char* v = std::getenv(k); return v ? std::atof(v) : d; }
int main(int argc, char** argv) {
  if (argc < 2) return 1;
  const double speed = argc > 2 ? std::atof(argv[2]) : 1.5, sec = argc > 3 ? std::atof(argv[3]) : 4.0;
  const double fs = 48000.0;
  const int midi[] = {79, 84, 91, 96, 103, 108};
  std::vector<double> all;
  for (int m : midi) {
    const double hz = 440.0 * std::pow(2.0, (m - 69) / 12.0);
    BarLosses l = glockenspielLosses(); l.internalQ = E("GQ", l.internalQ); l.supportDashpotNsPerM = E("GC", l.supportDashpotNsPerM);
    BarMallet ml = glockenspielMallet(); ml.dampingPerMeterPerSecond = E("GL", ml.dampingPerMeterPerSecond); ml.stiffness *= E("GK", 1.0); ml.massKg = E("GM", ml.massKg);
    BarListener li; li.polarRad = E("GP", 30.0) * constants::pi / 180.0; li.azimuthRad = E("GA", 30.0) * constants::pi / 180.0; li.distanceM = E("GR", 1.0); li.baffle = E("GB", kGlockCaseBaffle);
    auto v = StruckBarVoice::create(glockenspielBar(hz), l, ml, speed, (E("GX", 0.0) > 0.0 ? 0.5 - E("GX", 0.0) / glockenspielBar(hz).lengthM : E("GS", glockenspielStrikeFraction(glockenspielBar(hz).lengthM))), kGlockStrikeOffset, SampleRateHz{fs}, li);
    if (!v.ok()) { std::fprintf(stderr, "create %d at %d\n", (int)v.error(), m); return 2; }
    auto voice = std::move(v).value();
    std::vector<double> y((size_t)(sec * fs));
    if (!voice.renderBlock(y.data(), FrameCount{y.size()}).ok()) return 3;
    std::printf("midi %d hz %.1f L %.1f mm modes %zu contact %.2f ms peak %.3g Pa\n", m, hz, glockenspielBar(hz).lengthM * 1e3,
                voice.modeCount(), voice.contactSeconds() * 1e3, [&] { double p = 0; for (double s : y) p = std::max(p, std::fabs(s)); return p; }());
    for (size_t i = 0; i < voice.modeCount(); ++i) std::printf("   %.0f Hz sigma %.2f\n", voice.modeHz(i), voice.modeSigma(i));
    all.insert(all.end(), y.begin(), y.end());
  }
  FILE* o = std::fopen(argv[1], "wb"); std::fwrite(all.data(), 8, all.size(), o); std::fclose(o);
}
