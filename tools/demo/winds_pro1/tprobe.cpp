// Winds pro check 1 harness. Plays the phrase transcribed from each CC/PD
// reference recording (tests/wind_excerpts.h kPro*) on the engine voices,
// with the same voice setup as tests/test_wind_tone.cpp "wind songs"
// (copied: the hidden [.songs] case is not used as the harness).
// Usage: wp1 clarinet|saxophone|flute|trumpet|trombone out.f64
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/instruments/phrase_player.h>
#include <mmathen/physics/medium.h>
#include "../../../tests/wind_excerpts.h"
using namespace mmathen;
template <typename R> auto U(R r) { if (!r.ok()) { std::fprintf(stderr, "engine call failed\n"); std::exit(2); } return std::move(r).value(); }
constexpr double kAir = 101325.0, kTemp = 293.15, kFs = 48000.0;
const SampleRateHz kRate{kFs};
constexpr std::uint64_t kClarinetAttack = 19200, kSaxophoneAttack = 19200, kFluteAttack = 960,
                        kTrumpetAttack = 1920, kTromboneAttack = 6144;
ValveBlow sustained(double pressure, double cutoffHz, std::uint64_t attack) {
  ValveBlow b; b.pressurePa = pressure; b.frames = 96000; b.reflectionMagnitude = 0.995;
  b.initialDisplacementMeters = 2.0e-5; b.attackFrames = attack; b.listenerDistanceMeters = 1.0;
  b.reflectionCutoffHz = cutoffHz; return b;
}
double conePartial(double length, double apex, int mode) {
  SaxophoneSpec spec{length, apex, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0, 2.0e-6, 1.4e-4, 7.0e-4, 0.09};
  return U(saxophoneConeMode(U(makeSaxophone(spec)), mode)).value();
}
double impedancePeakHz(double length, double apex, int mode, double targetHz) {
  if (mode >= 1) return conePartial(length, apex, mode);
  double best = conePartial(length, apex, 1), gap = std::fabs(best - targetHz);
  for (int n = 2; n <= 8; ++n) { const double f = conePartial(length, apex, n); if (std::fabs(f - targetHz) < gap) { best = f; gap = std::fabs(f - targetHz); } }
  return best;
}
void push(std::vector<PhraseNote>& notes, const ScoreNote& src, double start, double length, double lip, double pressure, double jet) {
  PhraseNote n; n.name = src.name; n.startSec = start; n.targetHz = src.hertz; n.lengthM = length; n.lipHz = lip;
  n.pressure = src.hertz > 0.0 ? pressure : 0.0; n.jetSamples = jet; n.tongue = src.tongued; n.vibrato = src.vibrato;
  n.dynamic = src.dynamic; n.rest = !(src.hertz > 0.0); notes.push_back(n);
}
double total(const std::vector<ScoreNote>& s) { double t = 0; for (auto& n : s) t += n.seconds; return t; }

// Tongue probe: one held note, tongue shut for 40 ms at 0.6 s; prints 5 ms RMS (dB) and tongue fraction.
int main(int argc, char** argv) {
  const std::string which = argc > 1 ? argv[1] : "trumpet";
  const double speed = U(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  PlayerAirway airway(PlayerAirwaySpec{});
  auto run = [&](auto& voice, double L, double lip, double p) {
    voice.attachAirway(&airway); airway.placeTongue(1.0);
    std::vector<double> x(48000);
    for (int b = 0; b < 48000 / 64; ++b) {
      const double t = b * 64 / kFs;
      airway.setTongueTarget(t >= 0.6 && t < 0.64 ? 0.0 : 1.0);
      if (!voice.setSoundingLength(L) || !voice.setBlowingPressure(p)) return;
      if (lip > 0) if (!voice.setLipResonanceHz(lip)) return;
      if (!voice.renderBlock(x.data() + b * 64, FrameCount{64}).ok()) { std::printf("fail\n"); return; }
      if (b % 4 == 0 && t > 0.55 && t < 0.8) {
        double e = 0; for (int i = 0; i < 64; ++i) e += x[b * 64 + i] * x[b * 64 + i];
        std::printf("%.3f %.1f dB tongue %.2f\n", t, 10 * std::log10(e / 64 + 1e-30), airway.tongueOpenFraction());
      }
    }
  };
  if (which == "trumpet") {
    auto tp = U(makeTrumpet(phraseTrumpetSpec(speed, kAir, kTemp)));
    ValveBlow lip = sustained(2800.0, 10000.0, kTrumpetAttack); lip.frames = 48000;
    lip.reflectionMagnitude = 0.999; lip.initialDisplacementMeters = 0.0; lip.brassiness = 1.0;
    lip.reedTurbulence = kTrumpetReedTurbulence; applyBach37Bell(lip);
    auto voice = U(ConeValveVoice::fromTrumpet(tp, 0.26, lip, kRate));
    const BrassFinger r = trumpetFinger(392.0);
    run(voice, r.length, r.mode == 0 ? 0.75 * speed / r.length : 0.80 * conePartial(r.length, 0.26, r.mode), 2800.0);
  } else {
    ClarinetSpec spec{0.40, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0, 1.0e-6, 1.0e-4, 1.0e-3, 0.11};
    auto cl = U(makeClarinet(spec));
    ValveBlow blow = clarinetBoreLosses(sustained(4000.0, 1500.0, kClarinetAttack)); blow.frames = 48000;
    auto voice = U(BoreValveVoice::fromClarinet(cl, blow, kRate));
    run(voice, clarinetSoundingLengthMeters(392.0), 0.0, 4000.0);
  }
}
