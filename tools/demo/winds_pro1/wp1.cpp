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
int main(int argc, char** argv) {
  if (argc < 3) return 1;
  const std::string which = argv[1];
  const double speed = U(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  std::vector<double> out; std::vector<PhraseNote> notes; double T = 0;
  PlayerAirway airway(PlayerAirwaySpec{});
  if (which == "clarinet") {
    const auto song = ensureLeadIn(excerpts::kProClarinet); T = total(song);
    double longest = 0; for (auto& s : song) if (s.hertz > 0) longest = std::max(longest, clarinetSoundingLengthMeters(s.hertz));
    ClarinetSpec spec{longest * 1.04, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0, 1.0e-6, 1.0e-4, 1.0e-3, 0.11};
    auto cl = U(makeClarinet(spec));
    ValveBlow blow = sustained(4000.0, 1500.0, kClarinetAttack); blow.frames = (std::uint64_t)std::llround(T * kFs);
    blow.reedTurbulence = kClarinetReedTurbulence; blow.lockedBreathFraction = kClarinetLockedBreathFraction;
    auto voice = U(BoreValveVoice::fromClarinet(cl, blow, kRate));
    double start = 0, held = longest;
    for (auto& s : song) { const double L = s.hertz > 0 ? clarinetSoundingLengthMeters(s.hertz) : held; push(notes, s, start, L, 0, 4000.0, 0); if (s.hertz > 0) held = L; start += s.seconds; }
    out = playPhrase(voice, notes, T, false, 0.0, &airway, kFs);
  } else if (which == "saxophone") {
    const auto song = ensureLeadIn(excerpts::kProSaxophone); T = total(song);
    double longest = 0; for (auto& s : song) if (s.hertz > 0) longest = std::max(longest, saxophoneSoundingLengthMeters(s.hertz));
    SaxophoneSpec spec{longest * 1.08, 0.20, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0, 2.0e-6, 1.4e-4, 7.0e-4, 0.09};
    auto sax = U(makeSaxophone(spec));
    ValveBlow blow = sustained(4200.0, 1500.0, kSaxophoneAttack); blow.frames = (std::uint64_t)std::llround(T * kFs);
    blow.initialDisplacementMeters = 1.0e-5; blow.reedTurbulence = kSaxophoneReedTurbulence;
    blow.lockedBreathFraction = kSaxophoneLockedBreathFraction; blow.spectrumBreakHz = 837.0;
    auto voice = U(ConeValveVoice::fromSaxophone(sax, blow, kRate));
    double start = 0, held = longest;
    for (auto& s : song) { const double L = s.hertz > 0 ? saxophoneSoundingLengthMeters(s.hertz) : held; push(notes, s, start, L, 0, 4200.0, 0); if (s.hertz > 0) held = L; start += s.seconds; }
    out = playPhrase(voice, notes, T, false, 0.0, &airway, kFs);
  } else if (which == "flute") {
    const auto song = ensureLeadIn(excerpts::kProFlute); T = total(song);
    const double span = 7.0e-3, sigma = 0.45, thickness = 1.0e-3, end = fluteEndCorrectionMeters();
    auto bore = [&](double f) { return fluteSoundingLengthMeters(speed, f, end); };
    double longest = 0, lowest = 1e9; for (auto& s : song) if (s.hertz > 0) { longest = std::max(longest, bore(s.hertz)); lowest = std::min(lowest, s.hertz); }
    FluteSpec spec{longest * 1.04, 2.8e-4, kAir, kTemp, 1.2e-5, thickness, span, sigma};
    auto fl = U(makeFlute(spec));
    auto pressureFor = [&](double f) { const double v = 2.0 * span * f / sigma; return 0.5 * fl.airDensityKgPerM3 * v * v; };
    ValveBlow jet = sustained(pressureFor(lowest) * 0.5, 1800.0, kFluteAttack); jet.frames = (std::uint64_t)std::llround(T * kFs);
    jet.reflectionMagnitude = 0.985; jet.initialDisplacementMeters = 0.0; jet.jetAimMeters = 0.40 * thickness;
    jet.jetNoiseMeters = 0.035 * thickness; jet.reedTurbulence = kFluteJetTurbulence;
    jet.lockedBreathFraction = kFluteLockedBreathFraction; jet.endCorrectionMeters = end;
    auto voice = U(BoreValveVoice::fromFlute(fl, jet, kRate));
    double start = 0, hL = longest, hJ = 0, hP = pressureFor(lowest);
    for (auto& s : song) {
      const double p = s.hertz > 0 ? pressureFor(s.hertz) : 0.0, L = s.hertz > 0 ? bore(s.hertz) : hL;
      const double j = s.hertz > 0 ? kFs * U(fluteConvectionTime(fl, p)) : hJ;
      push(notes, s, start, L, 0, p > 0 ? p : hP, j); if (s.hertz > 0) { hL = L; hJ = j; hP = p; } start += s.seconds;
    }
    out = playPhrase(voice, notes, T, true, 0.0, &airway, kFs);
  } else if (which == "trumpet") {
    const auto song = ensureLeadIn(excerpts::kProTrumpet); T = total(song);
    auto tp = U(makeTrumpet(phraseTrumpetSpec(speed, kAir, kTemp)));
    ValveBlow lip = sustained(2800.0, 10000.0, kTrumpetAttack); lip.frames = (std::uint64_t)std::llround(T * kFs);
    lip.reflectionMagnitude = 0.999; lip.initialDisplacementMeters = 0.0; lip.tongueReleaseFrames = 0;
    lip.brassiness = 1.0; lip.reedTurbulence = kTrumpetReedTurbulence; applyBach37Bell(lip);
    auto voice = U(ConeValveVoice::fromTrumpet(tp, 0.26, lip, kRate));
    double start = 0; BrassFinger held = kTrumpetFingers[0];
    for (auto& s : song) {
      const BrassFinger r = s.hertz > 0 ? trumpetFinger(s.hertz) : held;
      const double lipHz = r.mode == 0 ? 0.75 * speed / r.length : 0.80 * conePartial(r.length, 0.26, r.mode);
      push(notes, s, start, r.length, lipHz, 2800.0, 0); notes.back().peakHz = impedancePeakHz(r.length, 0.26, r.mode, r.hertz);
      if (s.hertz > 0) held = r; start += s.seconds;
    }
    out = playPhrase(voice, notes, T, false, 0.50, &airway, kFs);
  } else if (which == "trombone") {
    const auto song = ensureLeadIn(excerpts::kProTrombone); T = total(song);
    auto horn = U(makeTrombone(phraseTromboneSpec(speed, kAir, kTemp)));
    ValveBlow bone = sustained(1600.0, 5000.0, kTromboneAttack); bone.frames = (std::uint64_t)std::llround(T * kFs);
    bone.reflectionMagnitude = 0.999; bone.initialDisplacementMeters = 0.0; bone.tongueReleaseFrames = 0;
    bone.brassiness = 1.0; bone.reedTurbulence = kTromboneReedTurbulence; applyScottBell(bone);
    bone.wallRadiusMeters = kBach42BoreRadiusMeters;
    auto voice = U(ConeValveVoice::fromTrombone(horn, 0.60, bone, kRate));
    double start = 0; BrassFinger held = kTromboneFingers[0];
    for (auto& s : song) {
      const BrassFinger r = s.hertz > 0 ? tromboneFinger(s.hertz) : held;
      push(notes, s, start, r.length, 0.80 * conePartial(r.length, 0.60, r.mode), 1600.0, 0);
      notes.back().peakHz = impedancePeakHz(r.length, 0.60, r.mode, r.hertz);
      if (s.hertz > 0) held = r; start += s.seconds;
    }
    out = playPhrase(voice, notes, T, false, 0.50, &airway, kFs);
  } else return 1;
  std::printf("%s %.3f s, %zu samples, lead-in %.3f s\n", which.c_str(), T, out.size(), notes.empty() ? 0.0 : notes[1].startSec);
  FILE* o = std::fopen(argv[2], "wb"); std::fwrite(out.data(), 8, out.size(), o); std::fclose(o);
}
