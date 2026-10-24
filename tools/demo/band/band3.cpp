// Step 13 player-profile harness (tools/demo/band): the instruments that
// had no player profile after step 12, each part rendered as written or,
// with BP_PROFILE=1, through its player profile (player_profile.h).
// Usage:  band3 <violin|viola|vbass|harp|harpsichord|eguitar|kit> in.csv out.f64 T
//   csv rows start,dur,midi,vel; out is raw float64 at 48 kHz, T seconds.
//   BP_DUMP=path writes the performed part (BP_DUMPONLY=1 stops there).
// Families:
//   bowed   violin / viola / arco double bass: makeBowedNote (bowed_note.h)
//           on the instrument's BowedVoicing; the part's bridge force goes
//           through the voicing's BowedBody (step 14). Level = vel / 80, as
//           band1's cello.
//   plucked harp / harpsichord / electric guitar: PluckedStringVoice (ideal
//           string, instrument_pluck_demo's T60 pair 2.0 s at f0 and 0.25 s
//           at 6 f0, listen node 0.23); the electric guitar reads the coil
//           velocity. Default pluck point: harp and guitar mid-string
//           (instrument_pluck_demo's 0.5), harpsichord the catalogue's
//           plucking point. Notes between the tabulated strings are a stop
//           on the next string down (harp: a pedal disc; harpsichord: the
//           catalogue gives C and F only, so the length is interpolated
//           this way, UNSOURCED).
//   kit     GM drum map: 36 kick, 38 snare, 42 closed hi-hat, 49 crash,
//           43 floor tom, 45 tom 12 in, 48 tom 10 in. Snare: snare_drum.h;
//           kick and toms: CavityDrumVoice on the Yamaha kit's shells;
//           hi-hat: PlateMalletVoice on the XSR plate; crash: the von Karman plate (cymbal_vk.h). Stick
//           speed 6 (vel/127)^2 m/s (band1's snare mapping, UNSOURCED); with
//           BP_PROFILE=1 a crash takes the kit profile's stroke (kitCrashStickSpeed).
// Harness numbers that are not physics data are marked in place.
#include <mmathen/instruments/player_profile.h>
#include <mmathen/instruments/bowed_note.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/electric_guitar.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/snare_drum.h>
#include <mmathen/instruments/cavity_drum.h>
#include <mmathen/instruments/drum.h>
#include <mmathen/instruments/cymbal.h>
#include <mmathen/instruments/cymbal_vk.h>
#include <mmathen/instruments/mallet_voice.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <vector>
using namespace mmathen;
static const double FS = 48000.0;
static double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
static bool PROFILE = false;
// Von Karman crash mix gain (its output is an acceleration, m/s^2): TUNED so a velocity-110 crash
// (profile stroke 8.25 m/s) has the 0-2 s RMS of the old linear crash at its 2e4 gain (8.3 vs
// 5454, docs/49-cymbal-von-karman.md), keeping the kit balance. Not physics.
static constexpr double kVkCrashMixGain = 1.52e-3;
static std::vector<PartNote> readPart(const char* path) {
  std::vector<PartNote> v; FILE* f = std::fopen(path, "r"); if (!f) return v;
  double s, d; int m, vel; while (std::fscanf(f, "%lf,%lf,%d,%d", &s, &d, &m, &vel) == 4) v.push_back({s, d, m, double(vel)});
  std::fclose(f); return v;
}
static bool dumpPart(const std::vector<PartNote>& ev) {
  const char* p = std::getenv("BP_DUMP"); if (!p) return false;
  FILE* f = std::fopen(p, "w"); for (const PartNote& e : ev) std::fprintf(f, "%.6f,%.6f,%d,%.4f\n", e.startSec, e.durSec, e.midi, e.velocity); std::fclose(f);
  return std::getenv("BP_DUMPONLY") != nullptr;
}
static void logShift(const char* part, const std::vector<PartNote>& a, const std::vector<PartNote>& b) {
  if (!PROFILE || a.empty()) return;
  double s2 = 0, sv = 0, mv = 0; for (size_t i = 0; i < a.size(); ++i) { const double d = b[i].startSec - a[i].startSec; s2 += d * d; mv += d;
    sv += (b[i].velocity / a[i].velocity - 1) * (b[i].velocity / a[i].velocity - 1); }
  const double n = double(a.size()); std::printf("%s performed: onset shift mean %.2f ms rms %.2f ms, velocity rms %.1f %%\n", part,
    1e3 * mv / n, 1e3 * std::sqrt(s2 / n), 100 * std::sqrt(sv / n));
}
static void addInto(std::vector<double>& mix, size_t a, const std::vector<double>& x, double g = 1.0) {
  for (size_t i = 0; i < x.size() && a + i < mix.size(); ++i) mix[a + i] += g * x[i];
}
static int writeOut(const std::vector<double>& mix, const char* out) {
  FILE* o = std::fopen(out, "wb"); if (!o) return 1; std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return 0;
}

// --- bowed ---
static int renderBowed(const char* which, const std::vector<PartNote>& w, double total, const char* out) {
  const bool vn = !std::strcmp(which, "violin"), va = !std::strcmp(which, "viola");
  auto inst = vn ? violin() : va ? viola() : doubleBass(); if (!inst.ok()) return 2;
  const BowedVoicing voicing = vn ? violinBowedVoicing() : va ? violaBowedVoicing() : doubleBassBowedVoicing();
  const BowedPlayerProfile prof = !PROFILE ? BowedPlayerProfile{} : vn ? violinistPlayerProfile() : va ? violistPlayerProfile() : arcoBassistPlayerProfile();
  const std::vector<PartNote> ev = performNotes(w, prof.feel); logShift(which, w, ev);
  if (dumpPart(ev)) return 0;
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  for (const PartNote& e : ev) {
    const size_t a = (size_t)(e.startSec * FS); if (a >= mix.size()) continue;
    auto vo = makeBowedNote(inst.value(), midiHz(e.midi), e.durSec, e.velocity / 80.0, SampleRateHz{FS}, voicing, true, prof.strings);
    if (!vo.ok()) { ++fail; continue; }
    auto v = std::move(vo).value();
    const size_t len = std::min((size_t)((e.durSec + voicing.releaseSeconds + 0.4) * FS), mix.size() - a);  // 0.4 s ring after the release (harness)
    std::vector<double> x(len); if (!v.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    addInto(mix, a, x);
  }
  // Step 14: the voicing's body (BowedBody) on the part's summed bridge
  // force. BP_NOBODY=1 keeps the string force (harness check).
  if (voicing.body.present && !(std::getenv("BP_NOBODY") && std::atoi(std::getenv("BP_NOBODY")) == 1)) {
    BowedBody body(voicing.body, FS);
    body.processInPlace(mix);
  }
  std::printf("%s notes=%zu fail=%d\n", which, ev.size(), fail);
  return writeOut(mix, out) ? 1 : (fail ? 3 : 0);
}

// --- plucked ---
static LossModel pluckLoss(const StringCourse& c) {
  auto ideal = courseString(c); const double f = c.tuning.frequency.value();
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{2.0}, FrequencyHz{6.0 * f}, DurationSeconds{0.25}).value();
}
static std::optional<StringCourse> stopTo(const StringCourse& open, double f) {
  const double f0 = open.tuning.frequency.value();
  if (f / f0 < 1.0005) return open;
  auto st = fingerStop(open, 1.0 - f0 / f); if (!st.ok()) return std::nullopt; return st.value();
}
static std::optional<StringCourse> harpCourse(double f) {
  static const auto harp = concertPedalHarp().value();
  const HarpStringGeometry* best = nullptr;
  for (const auto& s : harp.strings) if (s.publishedFrequencyHz <= f * 1.0005) best = &s;
  if (!best) return std::nullopt;
  const double L = best->vibratingLengthCm * 1e-2, f0 = best->publishedFrequencyHz;
  const double t = 2.0e-3 * (2 * L * f0) * (2 * L * f0);  // instrument_pluck_demo's 2 g/m (harness)
  auto c = tuneHarpString(*best, 1.0, 3.0, t * 0.5, t * 1.5); if (!c.ok()) return std::nullopt;
  return stopTo(c.value(), f);
}
static std::optional<StringCourse> harpsichordCourse(double f, double* pointMm) {
  static const auto hc = frenchDouble().value();
  const HarpsichordSpeakingLength* best = nullptr;
  for (const auto& s : hc.strings) if (s.choir == HarpsichordChoir::LongEightFoot && makePitch(s.label).value().frequency.value() <= f * 1.0005) best = &s;
  if (!best) return std::nullopt;
  const double L = best->vibratingLengthMm * 1e-3, f0 = makePitch(best->label).value().frequency.value();
  const double t = 1.0e-3 * (2 * L * f0) * (2 * L * f0);  // instrument_pluck_demo's 1 g/m (harness)
  auto c = tuneHarpsichordString(*best, 0.5, 1.5, t * 0.5, t * 1.5); if (!c.ok()) return std::nullopt;
  *pointMm = best->pluckingPointMm.value_or(0.0);
  return stopTo(c.value(), f);
}
static std::optional<StringCourse> electricCourse(double f) {
  static const auto g = electricGuitar().value();
  std::optional<StringCourse> best;
  for (const char* l : {"E2", "A2", "D3", "G3", "B3", "E4"}) {
    const auto* s = findElectricGuitarString(g, l); const double L = g.vibratingLength.value();
    const double f0 = makePitch(l).value().frequency.value(); if (f0 > f * 1.0005) break;
    const double newtons = s->tensionPounds * kPoundForceNewtons;
    const double grams = newtons / ((2 * L * f0) * (2 * L * f0)) * 1e3;
    auto c = tuneElectricGuitarString(g, *s, grams); if (c.ok()) best = c.value();
  }
  if (!best) return std::nullopt; return stopTo(*best, f);
}
static int renderPlucked(const char* which, const std::vector<PartNote>& w, double total, const char* out) {
  const bool hp = !std::strcmp(which, "harp"), hc = !std::strcmp(which, "harpsichord");
  const PluckedPlayerProfile prof = !PROFILE ? PluckedPlayerProfile{} : hp ? harpistPlayerProfile() : hc ? harpsichordistPlayerProfile() : electricGuitaristPlayerProfile();
  const std::vector<PartNote> ev = performNotes(w, prof.feel); logShift(which, w, ev);
  if (dumpPart(ev)) return 0;
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  for (const PartNote& e : ev) {
    const double f = midiHz(e.midi); double pointMm = 0.0;
    const auto c = hp ? harpCourse(f) : hc ? harpsichordCourse(f, &pointMm) : electricCourse(f);
    if (!c) { ++fail; continue; }
    const double L = c->geometry.vibratingLength.value();
    double frac = 0.5;  // instrument_pluck_demo's mid-string pluck (harness default)
    if (hc && pointMm > 0.0) frac = pluckFractionFromBridge(pointMm * 1e-3, L);
    if (prof.pluckFromBridgeM > 0.0) frac = pluckFractionFromBridge(prof.pluckFromBridgeM, L);
    else if (prof.pluckFraction > 0.0) frac = prof.pluckFraction;
    // The plectrum releases at its own displacement: the harpsichord ignores velocity.
    const double amp = 1.0e-4 * (hc ? 1.0 : e.velocity / 127.0) * prof.pluckForceScale;  // 0.1 mm at vel 127 (instrument_pluck_demo's)
    auto vo = PluckedStringVoice::create(*c, Excitation{ExcitationKind::Pluck, frac, amp}, pluckLoss(*c), 0.0, 4, 0, SampleRateHz{FS}, 0.23,
                                         hp || hc ? StringReadout::Displacement : StringReadout::CoilVelocity);
    if (!vo.ok()) { ++fail; continue; }
    auto v = std::move(vo).value();
    const size_t a = (size_t)(e.startSec * FS); if (a >= mix.size()) continue;
    const double damp = prof.fingerDampSec > 0.0 ? prof.fingerDampSec : (hc ? 0.03 : 0.05);  // damper / hand fade, UNSOURCED
    const size_t len = std::min((size_t)((e.durSec + damp) * FS), mix.size() - a), fo = (size_t)(damp * FS);
    std::vector<double> x(len); if (!v.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    for (size_t i = len > fo ? len - fo : 0; i < len; ++i) x[i] *= double(len - i) / fo;
    addInto(mix, a, x);
  }
  std::printf("%s notes=%zu fail=%d\n", which, ev.size(), fail);
  return writeOut(mix, out) ? 1 : (fail ? 3 : 0);
}

// --- kit ---
static MembraneHead kitHead(const MembraneShell& shell, double hz) {
  // tests/test_cavity_drum.cpp's tuning: 0.20-0.30 kg/m^2 head, tension from the (0,1) mode.
  const double dmin = 0.20, dmax = 0.30, d = 0.5 * (dmin + dmax);
  const double alpha = besselJZero(0, 1).value();
  const double wave = 2.0 * 3.141592653589793 * shell.headRadius.value() * hz / alpha;
  const double t = d * wave * wave;
  return tuneMembraneHead(shell, hz, dmin, dmax, t * 0.5, t * 1.5).value();
}
static DrumLimb limbFor(int m) { return m == 36 ? DrumLimb::RightFoot : (m == 42 || m == 49) ? DrumLimb::RightHand : DrumLimb::LeftHand; }
static int renderKit(const std::vector<PartNote>& w, double total, const char* out) {
  const DrumKitPlayerProfile prof = PROFILE ? drumKitPlayerProfile() : DrumKitPlayerProfile{};
  std::vector<KitNote> wk; for (const PartNote& n : w) wk.push_back({n, limbFor(n.midi)});
  const std::vector<KitNote> pk = performDrumKit(wk, prof);
  std::vector<PartNote> ev; for (const KitNote& k : pk) ev.push_back(k.note);
  logShift("kit", w, ev);
  if (dumpPart(ev)) return 0;
  const auto kit = yamahaTourCustomKit().value();
  const auto cym = sabianXsrCompleteSet().value();
  const CymbalMaterial bronze{0.001, 1.0e11, 7800.0, 0.33};  // tests/test_contact_voice.cpp's plate numbers (the XSR page prints none), UNSOURCED
  std::vector<int> nd, rd; // Plate modes (n nodal diameters, radial index r >= 1): n 2-14 at r 1, n 0-8 at r 2, n 0-4 at r 3 (harness choice).
  for (int n = 2; n <= 14; ++n) { nd.push_back(n); rd.push_back(1); } for (int n = 0; n <= 8; ++n) { nd.push_back(n); rd.push_back(2); } for (int n = 0; n <= 4; ++n) { nd.push_back(n); rd.push_back(3); }
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  for (const PartNote& e : ev) {
    const size_t a = (size_t)(e.startSec * FS); if (a >= mix.size()) continue;
    const double speed = 6.0 * (e.velocity / 127.0) * (e.velocity / 127.0);
    // Per-piece mix gains (the voices radiate in different units): kick 0.25,
    // hi-hat 3e4, crash 2e4, snare and toms 1. TUNED by peak level for the
    // demo balance, not physics.
    std::vector<double> x; double g = e.midi == 36 ? 0.25 : e.midi == 42 ? 3.0e4 : e.midi == 49 ? 2.0e4 : 1.0;
    auto run = [&](auto& voice, double sec) { x.assign(std::min((size_t)(sec * FS), mix.size() - a), 0.0); return voice.renderBlock(x.data(), FrameCount{x.size()}).ok(); };
    bool ok = false;
    if (e.midi == 38) { auto v = snareDrumHit(speed, true, SampleRateHz{FS}); if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 0.8); } }
    else if (e.midi == 36 || e.midi == 43 || e.midi == 45 || e.midi == 48) {
      const MembraneShell& sh = e.midi == 36 ? kit.bassDrum : e.midi == 43 ? kit.floorTom : e.midi == 45 ? kit.rackTom12 : kit.rackTom10;
      const double hz = e.midi == 36 ? 60.0 : e.midi == 43 ? 80.0 : e.midi == 45 ? 110.0 : 140.0;  // kick 60 Hz UNSOURCED; toms the cavity test's tunings
      const MembraneHead head = kitHead(sh, hz);
      // Kick: felt beater (test mallet), toms: the snare stick. Beater 0.03 kg UNSOURCED.
      const MalletStrike st = e.midi == 36 ? MalletStrike{0.0, 0.0, 0.03, speed, 8.0e4, 1.0, 0.0, 40.0}
                                           : MalletStrike{0.3, 0.0, kSnareStickMassKg, speed, kSnareStickStiffness, kSnareStickExponent, 0.0, 40.0};
      auto v = CavityDrumVoice::create(head, &head, sh, st, 0, 1, SampleRateHz{FS}, 101325.0, 293.15, 1.0);
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, e.midi == 36 ? 0.8 : 1.2); }
    } else if (e.midi == 49) {
      // Crash: the von Karman plate (cymbal_vk.h, step 19), XSR 16" radius, B20, struck by the stick at
      // 0.85 a. Stroke speed from the kit profile (kitCrashStickSpeed) or the snare map. 6 s of ring.
      static const auto crashPlate = [&] { VonKarmanPlateSpec sp; sp.radiusM = cym.crash16.radius.value(); return buildVonKarmanPlate(sp); }();
      const double cs = kitCrashStickSpeed(prof, e.velocity);
      const MalletStrike st{0.85, 0.0, kSnareStickMassKg, cs > 0.0 ? cs : speed, kSnareStickStiffness, kSnareStickExponent, 0.0, 600.0};
      if (crashPlate.ok()) {
        auto v = VonKarmanCymbalVoice::create(crashPlate.value(), st, SampleRateHz{FS});
        if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 6.0); }
      }
      g = kVkCrashMixGain;
    } else if (e.midi == 42 || e.midi == 49) {
      const bool hat = e.midi == 42;
      // Q 30 closed hat / 600 crash and a 0.12 s closed-hat choke: UNSOURCED harness choices.
      const MalletStrike st{hat ? 0.6 : 0.85, 0.0, kSnareStickMassKg, speed, kSnareStickStiffness, kSnareStickExponent, 0.0, hat ? 30.0 : 600.0};
      auto v = PlateMalletVoice::create(hat ? cym.hiHatTop : cym.crash16, bronze, st, nd.data(), rd.data(), int(nd.size()), SampleRateHz{FS}, 0.45, 0.3);
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, hat ? 0.12 : 2.5); }
      if (ok && hat) { const size_t fo = std::min<size_t>(x.size(), 960); for (size_t i = x.size() - fo; i < x.size(); ++i) x[i] *= double(x.size() - i) / fo; }
    }
    if (!ok) { ++fail; continue; }
    addInto(mix, a, x, g);
  }
  std::printf("kit notes=%zu fail=%d\n", ev.size(), fail);
  return writeOut(mix, out) ? 1 : (fail ? 3 : 0);
}
int main(int argc, char** argv) {
  if (argc < 5) { std::fprintf(stderr, "band3 <violin|viola|vbass|harp|harpsichord|eguitar|kit> in.csv out.f64 T\n"); return 1; }
  PROFILE = std::getenv("BP_PROFILE") && std::atoi(std::getenv("BP_PROFILE")) == 1;
  const std::vector<PartNote> w = readPart(argv[2]); const double T = std::atof(argv[4]);
  const char* k = argv[1];
  if (!std::strcmp(k, "violin") || !std::strcmp(k, "viola") || !std::strcmp(k, "vbass")) return renderBowed(k, w, T, argv[3]);
  if (!std::strcmp(k, "harp") || !std::strcmp(k, "harpsichord") || !std::strcmp(k, "eguitar")) return renderPlucked(k, w, T, argv[3]);
  if (!std::strcmp(k, "kit")) return renderKit(w, T, argv[3]);
  return 1;
}
