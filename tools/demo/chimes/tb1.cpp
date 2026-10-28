// tb1: tubular bells C4 G4 C5 F5 (strike tones) for the pro check against
// VSCO 2 CE TB_hit (CC0).  tb1 out.f64 [speed] [sec]; env TQ (Q_int), TK (mallet E multiplier)
#include <mmathen/instruments/tubular_bells.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mmathen;
static double E(const char* k, double d) { const char* v = std::getenv(k); return v ? std::atof(v) : d; }
int main(int argc, char** argv) {
  if (argc < 2) return 1;
  const double speed = argc > 2 ? std::atof(argv[2]) : 2.0, sec = argc > 3 ? std::atof(argv[3]) : 8.0, fs = 48000.0;
  const int midi[] = {60, 67, 72, 77};
  std::vector<double> all;
  for (int m : midi) {
    const double hz = 440.0 * std::pow(2.0, (m - 69) / 12.0);
    BarMallet ml = chimeMallet(); ml.stiffness *= E("TK", 1.0);
    BarSpec b = chimeTube(hz);
    BarLosses l; l.internalQ = E("TQ", kChimeInternalQ);
    BarListener li; li.polarRad = constants::pi / 3.0;
    auto v = StruckBarVoice::create(b, l, ml, speed, 0.0, 0.0, SampleRateHz{fs}, li);
    if (!v.ok()) { std::fprintf(stderr, "create %d\n", (int)v.error()); return 2; }
    auto voice = std::move(v).value();
    std::vector<double> y((size_t)(sec * fs));
    if (!voice.renderBlock(y.data(), FrameCount{y.size()}).ok()) return 3;
    double pk = 0; for (double s : y) pk = std::max(pk, std::fabs(s));
    std::printf("midi %d L %.3f m contact %.2f ms peak %.3g Pa modes:", m, b.lengthM, voice.contactSeconds() * 1e3, pk);
    for (size_t i = 0; i < voice.modeCount() && i < 10; ++i) std::printf(" %.0f", voice.modeHz(i));
    std::printf("\n");
    all.insert(all.end(), y.begin(), y.end());
  }
  FILE* o = std::fopen(argv[1], "wb"); std::fwrite(all.data(), 8, all.size(), o); std::fclose(o);
}
