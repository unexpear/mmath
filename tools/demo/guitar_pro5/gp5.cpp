// Guitar pro check 5 harness: gp3 + engine BR2 composite body (Roberts 2015 Table 4.3) + bass damping (letRingEndsBassDamped). Was gp2:
// pluck point from bridge, let-ring). No per-note peak normalisation: each
// pluck uses the score velocity as displacement amplitude (1e-4 m * vel/127,
// same scale as solo4). Usage: gp2 score.csv out.f64 totalSeconds [pluckFromBridgeM]
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/guitar_pluck.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <optional>
#include <algorithm>
using namespace mmathen;
static const double FS = 48000.0;
double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
StringCourse classical(const char* label) {
  auto g = classicalGuitar(); const auto* s = findClassicalGuitarString(g.value(), label);
  const double L = g.value().vibratingLength.value(); const double f = makePitch(label).value().frequency.value();
  const double t = 2.0e-3 * (2 * L * f) * (2 * L * f);
  return tuneClassicalGuitarString(g.value(), *s, 1.0, 3.0, t * 0.5, t * 1.5).value();
}
int main(int argc, char** argv) {
  FILE* in = std::fopen(argv[1], "r"); const double total = std::atof(argv[3]);
  const double dB = argc > 4 ? std::atof(argv[4]) : kClassicalGuitarRoundPluckFromBridgeM;
  std::vector<StringCourse> open; for (const char* l : {"E2","A2","D3","G3","B3","E4"}) open.push_back(classical(l));
  struct N { double s; int m, v, str; std::optional<StringCourse> c; };
  std::vector<N> notes; double s, d; int m, v;
  while (std::fscanf(in, "%lf,%lf,%d,%d", &s, &d, &m, &v) == 4) {
    const double f = midiHz(m); N n{s, m, v, 0, {}};
    for (int i = 5; i >= 0; --i) { const double f0 = open[i].tuning.frequency.value();
      if (f0 <= f * 1.0005) { n.str = 6 - i;
        if (f / f0 < 1.0005) n.c.emplace(open[i]); else { auto st = fingerStop(open[i], 1.0 - f0 / f); if (st.ok()) n.c.emplace(st.value()); }
        break; } }
    notes.push_back(n);
  }
  std::vector<double> on, en; std::vector<int> sn; for (auto& n : notes) { on.push_back(n.s); sn.push_back(n.str); }
  auto ends = (argc > 6 && std::atoi(argv[6]) == 0) ? letRingEnds(on, sn, total) : letRingEndsBassDamped(on, sn, total);
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  static const auto body = classicalGuitarBR2CompositeBodyModes();
  // argv6 = 0: plain let-ring (no bass damping).
  std::vector<double> areaV(kClassicalGuitarBR2CompositeAreasM2.begin(), kClassicalGuitarBR2CompositeAreasM2.end());
  const std::span<const double> areas(areaV);
  for (size_t k = 0; k < notes.size(); ++k) { auto& n = notes[k]; if (!n.c) { ++fail; continue; }
    const size_t a = (size_t)(n.s * FS); if (a >= mix.size()) continue;
    size_t len = std::min((size_t)((ends[k] - n.s) * FS), mix.size() - a);
    auto loss = classicalGuitarStringLoss(*n.c, n.str); if (!loss.ok()) { ++fail; continue; }
    const double frac = pluckFractionFromBridge(dB, n.c->geometry.vibratingLength.value());
    auto vo = SoundboardVoice::create(*n.c, Excitation{ExcitationKind::Pluck, frac, 1.0e-4 * n.v / 127.0}, loss.value(), std::span<const BodyModeSpec>(body), areas, 1.0, 101325.0, 293.15, SampleRateHz{FS});
    if (!vo.ok()) { ++fail; continue; }
    std::vector<double> x(len); auto vv = std::move(vo).value(); if (!vv.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    const size_t fo = std::min<size_t>(len, 480);  // 10 ms damp at re-pluck (finger contact), VERIFY
    for (size_t i = 0; i < len; ++i) { double g = 1.0; if (i >= len - fo) g = double(len - i) / fo; mix[a + i] += x[i] * g; }
  }
  std::printf("notes=%zu fail=%d\n", notes.size(), fail);
  FILE* o = std::fopen(argv[2], "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o);
}
