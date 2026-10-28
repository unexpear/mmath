#include <mmathen/instruments/bowed_string.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/struck_string.h>
#include <mmathen/instruments/piano.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <mmathen/instruments/violin.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <optional>
#include <string>
#include <algorithm>
using namespace mmathen;
static const double FS = 48000.0; static double FORCE = 0.15; static double PLUCK = 0.5; static bool EXPR = false;
LossModel lossT(const StringCourse& c, double t60, double t60hi) {
  auto ideal = courseString(c); const double f = c.tuning.frequency.value();
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{t60},
                     FrequencyHz{6.0 * f}, DurationSeconds{t60hi}).value();
}
double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
StringCourse classical(const char* label) {
  auto g = classicalGuitar(); const auto* s = findClassicalGuitarString(g.value(), label);
  const double L = g.value().vibratingLength.value(); const double f = makePitch(label).value().frequency.value();
  const double t = 2.0e-3 * (2 * L * f) * (2 * L * f);
  return tuneClassicalGuitarString(g.value(), *s, 1.0, 3.0, t * 0.5, t * 1.5).value();
}
bool stopOn(const std::vector<StringCourse>& open, double f, std::optional<StringCourse>& out) {
  for (int i = (int)open.size() - 1; i >= 0; --i) {
    const double f0 = open[i].tuning.frequency.value();
    if (f0 <= f * 1.0005) {
      if (f / f0 < 1.0005) { out.emplace(open[i]); return true; }
      auto s = fingerStop(open[i], 1.0 - f0 / f); if (s.ok()) { out.emplace(s.value()); return true; }
    }
  }
  return false;
}
int main(int argc, char** argv) {
  std::string inst = argv[1]; if (argc > 5) FORCE = std::atof(argv[5]); if (argc > 6) PLUCK = std::atof(argv[6]); if (argc > 7) EXPR = true;
  FILE* in = std::fopen(argv[2], "r"); double total = std::atof(argv[4]);
  std::vector<double> mix((size_t)(total * FS), 0.0);
  double s, d; int m, v; int n = 0, fail = 0;
  std::vector<StringCourse> open;
  if (inst == "guitar") for (const char* l : {"E2","A2","D3","G3","B3","E4"}) open.push_back(classical(l));
  if (inst == "violin") { auto vn = violin().value(); for (auto& c : vn.courses) open.push_back(c); }
  std::optional<PianoStringBank> bank; if (inst == "piano") bank.emplace(concertGrandStringBank().value());
  while (std::fscanf(in, "%lf,%lf,%d,%d", &s, &d, &m, &v) == 4) {
    const double f = midiHz(m); const double vel = v / 127.0;
    double ring = inst == "violin" ? d : d + (inst == "piano" ? 0.6 : 0.8);
    const size_t a = (size_t)(s * FS); size_t len = (size_t)(ring * FS);
    if (a >= mix.size()) continue; len = std::min(len, mix.size() - a);
    std::vector<double> x(len); bool ok = false;
    if (inst == "piano") {
      const PianoStringSpec* best = nullptr;
      for (const auto& r : bank->strings) if (!best || std::fabs(std::log(r.publishedF0.value() / f)) < std::fabs(std::log(best->publishedF0.value() / f))) best = &r;
      PianoStringSpec sp = *best; const double ratio = sp.publishedF0.value() / f;
      sp.length = DistanceMeters{sp.length.value() * ratio}; sp.publishedF0 = FrequencyHz{f}; sp.note = "harness";
      auto c = tunePianoString(sp); auto st = tunedPianoString(sp);
      if (c.ok() && st.ok()) {
        auto vo = FeltHammerVoice::create(c.value(), HammerStrike{0.12, 0.008, 0.8 * vel + 0.2}, lossT(c.value(), 4.0, 0.4), st.value().stiffness, 3200.0, 1.0, 0.0, 4, SampleRateHz{FS}, 0.3);
        if (vo.ok()) { auto vv = std::move(vo).value(); ok = vv.renderBlock(x.data(), FrameCount{len}).ok(); }
      }
    } else {
      std::optional<StringCourse> oc; if (stopOn(open, f, oc)) { const StringCourse& c = *oc;
        if (inst == "guitar") {
          static const auto body = classicalGuitarBodyModes(); auto vo = SoundboardVoice::create(c, Excitation{ExcitationKind::Pluck, PLUCK, 1.0e-4 * vel}, lossT(c, 2.0, 0.25), std::span<const BodyModeSpec>(body), 0.02, 1.0, 101325.0, 293.15, SampleRateHz{FS});
          if (vo.ok()) { auto vv = std::move(vo).value(); ok = vv.renderBlock(x.data(), FrameCount{len}).ok(); }
        } else {
          const double mu = c.material.linearDensity.value(), T = c.tension.value();
          const double Z = mu * std::sqrt(T / mu); const double beta = 0.125, sp = 0.2;
          const double fmax = 2.0 * Z * sp / (beta * 0.5);
          auto vo = BowedStringVoice::create(c, BowStroke{beta, sp, FORCE * fmax, BowFriction{0.8, 0.3}}, lossT(c, 2.0, 0.25), 0.0, 4, 0, SampleRateHz{FS}, (std::uint64_t)(len > 960 ? len - 960 : len));
          if (vo.ok()) { auto vv = std::move(vo).value(); if (EXPR) { BowExpression e{6.0, 15.0, std::min(0.25, d * 0.4), 0.3, std::min(0.08, d * 0.3), 0.2, std::min(0.06, d * 0.2)}; (void)vv.setExpression(e); } ok = vv.renderBlock(x.data(), FrameCount{len}).ok(); }
        }
      }
    }
    ++n; if (!ok) { ++fail; std::printf("fail note %d at %.2f\n", m, s); continue; }
    double pk = 0; for (double y : x) pk = std::max(pk, std::fabs(y)); if (!(pk > 0) || !std::isfinite(pk)) { ++fail; continue; }
    const size_t fo = std::min<size_t>(len, 480);
    for (size_t i = 0; i < len; ++i) { double g = vel / pk; if (i >= len - fo) g *= double(len - i) / fo; mix[a + i] += x[i] * g; }
  }
  std::printf("%s notes=%d fail=%d\n", inst.c_str(), n, fail);
  FILE* o = std::fopen(argv[3], "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o);
}
