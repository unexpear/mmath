// cel1: celesta notes A4 B4 C5 D5 E5 G5 for the pro check against the
// Freesound Macsat-Rd celesta pack (CC0).  cel1 out.f64 [speed] [sec]
// env CC coupling, CK hammer stiffness, CM hammer mass.
#include <mmathen/instruments/celesta.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace mmathen;
static double E(const char* k, double d) { const char* v = std::getenv(k); return v ? std::atof(v) : d; }
int main(int argc, char** argv) {
  if (argc < 2) return 1;
  const double speed = argc > 2 ? std::atof(argv[2]) : 1.0, sec = argc > 3 ? std::atof(argv[3]) : 3.0, fs = 48000.0;
  const int midi[] = {69, 71, 72, 74, 76, 79};
  std::vector<double> all;
  for (int m : midi) {
    const double hz = 440.0 * std::pow(2.0, (m - 69) / 12.0);
    BarMallet h = celestaHammer(); h.stiffness = E("CK", h.stiffness); h.massKg = E("CM", h.massKg);
    auto v = celestaNote(hz, speed, SampleRateHz{fs}, E("CC", -1.0), &h);
    if (!v.ok()) { std::fprintf(stderr, "create %d\n", (int)v.error()); return 2; }
    auto voice = std::move(v).value();
    std::vector<double> y((size_t)(sec * fs));
    if (!voice.renderBlock(y.data(), FrameCount{y.size()}).ok()) return 3;
    double pk = 0; for (double s : y) pk = std::max(pk, std::fabs(s));
    std::printf("midi %d L %.1f mm contact %.2f ms peak %.3g Pa\n", m, celestaBar(hz).lengthM * 1e3, voice.contactSeconds() * 1e3, pk);
    all.insert(all.end(), y.begin(), y.end());
  }
  FILE* o = std::fopen(argv[1], "wb"); std::fwrite(all.data(), 8, all.size(), o); std::fclose(o);
}
