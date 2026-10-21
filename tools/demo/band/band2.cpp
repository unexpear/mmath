// Band player-profile harness (tools/demo/band, step 12): band1's renderers
// (bass, piano, cello, snare) plus gp5's guitar, each part passed through
// its player profile (player_profile.h) when BP_PROFILE=1. Without it the
// written part is rendered exactly as band1 / gp5 render it (byte-identical;
// for the guitar since step 18 only with BP_BODY13=1, see renderGuitar).
// Usage:  band2 <bass|piano|cello|snare|guitar> score in.csv out.f64 T
// band1's original notes:
//   band1 bass notes out.f64            isolated notes, 3.5 s apart (refs list)
//   band1 bass score in.csv out.f64 T    csv rows start,dur,midi,vel
//   band1 piano notes|score ...          same, piano (PianoNoteVoice); the
//     damper falls at the written end (or the next strike of the key).
//   band1 cello notes|score ...          same, bowed cello (cello_voice.h):
//     each note a new bow stroke on its string (rigid-bridge force), all
//     forces through one CelloBody. Level = vel / 80 (bow speed, beta).
//   band1 snare notes|score ...          snare drum (snare_drum.h), snares
//     on; stick speed 6 (vel/127)^2 m/s (UNSOURCED mapping); each hit
//     rendered 0.8 s. notes: speeds 1, 3, 6 m/s and 3 m/s snares off.
#include <mmathen/instruments/double_bass_pizz.h>
#include <mmathen/instruments/piano_voice.h>
#include <mmathen/instruments/cello_voice.h>
#include <mmathen/instruments/snare_drum.h>
#include <mmathen/instruments/player_profile.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <optional>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace mmathen;
static const double FS = 48000.0;
static double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
struct Ev { double s, d; int m; double v; };
static bool PROFILE = false;
// The written part through a player's feel (PartNote round trip).
static std::vector<Ev> perform(const std::vector<Ev>& ev, const PlayerFeel& feel) {
  std::vector<PartNote> w; for (const Ev& e : ev) w.push_back({e.s, e.d, e.m, e.v});
  auto p = performNotes(w, feel); std::vector<Ev> out;
  for (const PartNote& n : p) out.push_back({n.startSec, n.durSec, n.midi, n.velocity});
  return out;
}
// BP_DUMP=<path>: write the performed part (start,dur,midi,velocity) and,
// with BP_DUMPONLY=1, stop before rendering.
static bool dumpPart(const std::vector<Ev>& ev) {
  const char* p = std::getenv("BP_DUMP"); if (!p) return false;
  FILE* f = std::fopen(p, "w"); for (const Ev& e : ev) std::fprintf(f, "%.6f,%.6f,%d,%.4f\n", e.s, e.d, e.m, e.v); std::fclose(f);
  return std::getenv("BP_DUMPONLY") != nullptr;
}
static void logShift(const char* part, const std::vector<Ev>& a, const std::vector<Ev>& b) {
  double s2 = 0, sv = 0, mv = 0; for (size_t i = 0; i < a.size(); ++i) { const double d = b[i].s - a[i].s; s2 += d * d; mv += d;
    sv += (b[i].v / a[i].v - 1) * (b[i].v / a[i].v - 1); }
  const double n = double(a.size()); std::printf("%s performed: onset shift mean %.2f ms rms %.2f ms, velocity rms %.1f %%\n", part,
    1e3 * mv / n, 1e3 * std::sqrt(s2 / n), 100 * std::sqrt(sv / n));
}
static int renderBass(const std::vector<Ev>& ev0, double total, const char* out) {
  auto bass = doubleBass(); if (!bass.ok()) return 2;
  const PluckedPlayerProfile prof = PROFILE ? bassistPlayerProfile() : PluckedPlayerProfile{};
  const std::vector<Ev> ev = perform(ev0, prof.feel); if (PROFILE) logShift("bass", ev0, ev);
  if (dumpPart(ev)) return 0;
  const double fromBridge = prof.pluckFromBridgeM > 0.0 ? prof.pluckFromBridgeM : kDoubleBassPizzFromBridgeM;
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  const double amp = std::getenv("BB_AMP") ? std::atof(std::getenv("BB_AMP")) : 2.0e-3;
  for (size_t k = 0; k < ev.size(); ++k) {
    const Ev& e = ev[k]; auto note = doubleBassNote(bass.value(), midiHz(e.m)); if (!note.ok()) { ++fail; continue; }
    // A note rings until its written end or the next note on the same string.
    double end = e.s + e.d;
    for (size_t j = k + 1; j < ev.size(); ++j) { auto nj = doubleBassNote(bass.value(), midiHz(ev[j].m));
      if (nj.ok() && nj.value().stringIndex == note.value().stringIndex) { end = std::min(end, ev[j].s); break; } }
    const size_t a = (size_t)(e.s * FS); if (a >= mix.size()) continue;
    const size_t damp = (size_t)((prof.fingerDampSec > 0.0 ? prof.fingerDampSec : kDoubleBassPizzDampSec) * FS);
    size_t len = std::min((size_t)((end - e.s) * FS) + damp, mix.size() - a);
    const double t6s = std::getenv("BB_T6S") ? std::atof(std::getenv("BB_T6S")) : kDoubleBassPizzStoppedT60At6F0;
    auto vo = doubleBassPizzVoice(note.value(), amp * e.v / 127.0 * prof.pluckForceScale, SampleRateHz{FS}, 1.0, t6s, fromBridge); if (!vo.ok()) { ++fail; continue; }
    auto v = std::move(vo).value(); std::vector<double> x(len);
    if (!v.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    for (size_t i = 0; i < len; ++i) { double g = 1.0; if (i + damp >= len) g = double(len - i) / damp; mix[a + i] += x[i] * g; }
  }
  std::printf("bass notes=%zu fail=%d\n", ev.size(), fail);
  FILE* o = std::fopen(out, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
static int renderPiano(const std::vector<Ev>& ev0, double total, const char* out) {
  const StruckPlayerProfile prof = PROFILE ? pianistPlayerProfile() : StruckPlayerProfile{};
  std::vector<PartNote> w; for (const Ev& e : ev0) w.push_back({e.s, e.d, e.m, e.v});
  std::vector<Ev> ev; for (const PartNote& n : performPiano(w, prof)) ev.push_back({n.startSec, n.durSec, n.midi, n.velocity});
  if (PROFILE) logShift("piano", ev0, ev);
  if (dumpPart(ev)) return 0;
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  const double amp = std::getenv("PN_AMP") ? std::atof(std::getenv("PN_AMP")) : 1.0;
  const size_t damp = (size_t)(kPianoDamperSec * FS);
  for (size_t k = 0; k < ev.size(); ++k) {
    const Ev& e = ev[k]; double end = e.s + e.d;
    for (size_t j = k + 1; j < ev.size(); ++j) if (ev[j].m == e.m) { end = std::min(end, ev[j].s); break; }
    const bool damped = e.m < kPianoFirstUndampedKey;
    if (!damped) end = std::max(end, e.s + 4.0);
    const size_t a = (size_t)(e.s * FS); if (a >= mix.size()) continue;
    size_t len = std::min((size_t)((end - e.s) * FS) + damp, mix.size() - a);
    const double vel = std::getenv("PN_V") ? std::atof(std::getenv("PN_V")) : pianoSpeedFor(prof, e.v);
    const double ks = std::getenv("PN_K") ? std::atof(std::getenv("PN_K")) : 0.0;
    auto vo = PianoNoteVoice::create(e.m, vel, SampleRateHz{FS}, 1.0, ks);
    if (!vo.ok()) { std::fprintf(stderr, "piano %d err %d\n", e.m, (int)vo.error()); ++fail; continue; }
    auto v = std::move(vo).value(); std::vector<double> x(len, 0.0);
    if (!v.renderAdd(x.data(), len).ok()) { ++fail; continue; }
    for (size_t i = 0; i < len; ++i) { double g = 1.0; if (damped && i + damp >= len) g = double(len - i) / damp; mix[a + i] += amp * x[i] * g; }
  }
  std::printf("piano notes=%zu fail=%d\n", ev.size(), fail);
  FILE* o = std::fopen(out, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
// PR #2's existing piano (tests/test_contact_voice.cpp, "a hammered bridge
// radiates through the soundboard"): a Table III string, strike {0.12, 8 g,
// 0.8 m/s}, felt K 3200 p 1, courseLoss(4.0, 0.4), one body mode at f0
// (light Q40 5 kg, or heavy Q200 1000 kg), area 0.02 m^2, 1 m.
static int renderOldPiano(bool heavy, const char* out) {
  auto bank = concertGrandStringBank(); if (!bank.ok()) return 2;
  const double T = 0.5 + 6.0 * bank.value().strings.size() + 0.5;
  std::vector<double> mix((size_t)(T * FS), 0.0); int fail = 0; double s = 0.5;
  for (const PianoStringSpec& spec : bank.value().strings) {
    auto course = tunePianoString(spec); auto stiff = tunedPianoString(spec);
    if (!course.ok() || !stiff.ok()) { ++fail; s += 6.0; continue; }
    const double f = course.value().tuning.frequency.value();
    auto ideal = courseString(course.value());
    auto lo = ideal.ok() ? lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{4.0}, FrequencyHz{6.0 * f}, DurationSeconds{0.4})
                         : Result<LossModel, QuantityError>::failure(ideal.error());
    if (!lo.ok()) { ++fail; s += 6.0; continue; }
    const LossModel loss = lo.value();
    const BodyModeSpec mode = heavy ? BodyModeSpec{f, 200.0, 1000.0} : BodyModeSpec{f, 40.0, 5.0};
    const HammerBodyDrive body{std::span<const BodyModeSpec>(&mode, 1), 0.02, 1.0, 101325.0, 293.15};
    auto v = HammeredStringVoice::create(course.value(), HammerStrike{0.12, 0.008, 0.8}, loss,
                                         stiff.value().stiffness, 3200.0, 1.0, 0.0, 4, 4, SampleRateHz{FS}, body);
    if (!v.ok()) { std::fprintf(stderr, "old %s err %d\n", spec.note, (int)v.error()); ++fail; s += 6.0; continue; }
    auto voice = std::move(v).value(); const size_t len = (size_t)(5.5 * FS); std::vector<double> x(len);
    if (!voice.renderBlock(x.data(), FrameCount{len}).ok()) ++fail;
    else for (size_t i = 0; i < len; ++i) mix[(size_t)(s * FS) + i] += x[i];
    s += 6.0;
  }
  std::printf("old piano (%s) fail=%d\n", heavy ? "heavy" : "light", fail);
  FILE* o = std::fopen(out, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
static int renderCello(const std::vector<Ev>& ev0, double total, const char* out) {
  auto vc = cello(); if (!vc.ok()) return 2;
  const BowedPlayerProfile prof = PROFILE ? cellistPlayerProfile() : BowedPlayerProfile{};
  const std::vector<Ev> ev = perform(ev0, prof.feel); if (PROFILE) logShift("cello", ev0, ev);
  if (dumpPart(ev)) return 0;
  const size_t N = (size_t)(total * FS); std::vector<double> force(N, 0.0), mix(N, 0.0); int fail = 0;
  const double amp = std::getenv("VC_AMP") ? std::atof(std::getenv("VC_AMP")) : 1.0;
  for (const Ev& e : ev) {
    const double bowSec = std::max(0.05, e.d - 0.02);
    auto v = makeCelloNote(vc.value(), midiHz(e.m), bowSec, e.v / 80.0, SampleRateHz{FS}, true, prof.cello);
    if (!v.ok()) { ++fail; continue; }
    auto voice = std::move(v).value();
    const size_t a = (size_t)(e.s * FS); if (a >= N) continue;
    const size_t len = std::min((size_t)((bowSec + 1.5) * FS), N - a); std::vector<double> x(len);
    if (!voice.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    for (size_t i = 0; i < len; ++i) force[a + i] += x[i];
  }
  auto made = CelloBody::create(SampleRateHz{FS}); if (!made.ok()) return 2;
  auto body = std::move(made).value();
  if (!body.processAdd(force.data(), mix.data(), N)) ++fail;
  for (double& y : mix) y *= amp;
  std::printf("cello notes=%zu fail=%d\n", ev.size(), fail);
  FILE* o = std::fopen(out, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
static int renderSnare(const std::vector<Ev>& ev0, double total, const char* out, bool offLast) {
  const StruckPlayerProfile prof = PROFILE ? snareDrummerPlayerProfile() : StruckPlayerProfile{};
  const std::vector<Ev> ev = perform(ev0, prof.feel); if (PROFILE) logShift("snare", ev0, ev);
  if (dumpPart(ev)) return 0;
  const size_t N = (size_t)(total * FS); std::vector<double> mix(N, 0.0); int fail = 0;
  const double amp = std::getenv("SD_AMP") ? std::atof(std::getenv("SD_AMP")) : 1.0;
  for (size_t k = 0; k < ev.size(); ++k) {
    const Ev& e = ev[k]; const double x = e.v / 127.0;
    auto v = snareDrumHit(6.0 * x * x, !(offLast && k + 1 == ev.size()), SampleRateHz{FS});
    if (!v.ok()) { ++fail; continue; }
    auto voice = std::move(v).value();
    const size_t a = (size_t)(e.s * FS); if (a >= N) continue;
    const size_t len = std::min((size_t)(0.8 * FS), N - a); std::vector<double> y(len);
    if (!voice.renderBlock(y.data(), FrameCount{len}).ok()) { ++fail; continue; }
    for (size_t i = 0; i < len; ++i) mix[a + i] += amp * y[i];
  }
  std::printf("snare notes=%zu fail=%d\n", ev.size(), fail);
  FILE* o = std::fopen(out, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
static StringCourse classical(const char* label, int stringNumber) {
  auto g = classicalGuitar(); const auto* s = findClassicalGuitarString(g.value(), label);
  const double L = g.value().vibratingLength.value(); const double f = makePitch(label).value().frequency.value();
  if (std::getenv("GP_UNIFORM")) { const double t = 2.0e-3 * (2 * L * f) * (2 * L * f);
    return tuneClassicalGuitarString(g.value(), *s, 1.0, 3.0, t * 0.5, t * 1.5).value(); }
  const double T = classicalGuitarStringTensionN(stringNumber, L), mu = 1e3 * T / ((2 * L * f) * (2 * L * f));
  return tuneClassicalGuitarString(g.value(), *s, mu * 0.995, mu * 1.005, T * 0.99, T * 1.01).value();
}
// gp5's guitar (bass damping, pluck 0.13 m) on the performed part.
static int renderGuitar(const char* inPath, double total, const char* outPath) {
  const PluckedPlayerProfile prof = PROFILE ? guitaristPlayerProfile() : PluckedPlayerProfile{};
  const double dB = prof.pluckFromBridgeM > 0.0 ? prof.pluckFromBridgeM : kClassicalGuitarRoundPluckFromBridgeM;
  FILE* in = std::fopen(inPath, "r"); if (!in) return 1;
  std::vector<Ev> ev0; { Ev e; int vi; while (std::fscanf(in, "%lf,%lf,%d,%d", &e.s, &e.d, &e.m, &vi) == 4) { e.v = vi; ev0.push_back(e); } }
  const std::vector<Ev> ev = perform(ev0, prof.feel); if (PROFILE) logShift("guitar", ev0, ev);
  if (dumpPart(ev)) return 0;
  std::vector<StringCourse> open; { int sn = 6; for (const char* l : {"E2","A2","D3","G3","B3","E4"}) open.push_back(classical(l, sn--)); }
  struct N { double s; int m; double v; int str; std::optional<StringCourse> c; };
  std::vector<N> notes;
  for (const Ev& e : ev) {
    const double s = e.s; const int m = e.m; const double v = e.v;
    const double f = midiHz(m); N n{s, m, v, 0, {}};
    for (int i = 5; i >= 0; --i) { const double f0 = open[i].tuning.frequency.value();
      if (f0 <= f * 1.0005) { n.str = 6 - i;
        if (f / f0 < 1.0005) n.c.emplace(open[i]); else { auto st = fingerStop(open[i], 1.0 - f0 / f); if (st.ok()) n.c.emplace(st.value()); }
        break; } }
    notes.push_back(n);
  }
  std::vector<double> on, en; std::vector<int> sn; for (auto& n : notes) { on.push_back(n.s); sn.push_back(n.str); }
  auto ends = letRingEndsBassDamped(on, sn, total);
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  // Step 18: the BR2 composite plus Woodhouse 2004's statistical modes
  // 1.4-5.19 kHz (guitar.h classicalGuitarBR2StatisticalBodyModes).
  // BP_BODY13=1 renders the earlier 13-mode body (gp5's, byte-identical to
  // the pre-step-18 band renders).
  const bool body13 = std::getenv("BP_BODY13") && std::atoi(std::getenv("BP_BODY13")) == 1;
  static const auto body13Modes = classicalGuitarBR2CompositeBodyModes();
  const std::vector<BodyModeSpec> body = body13 ? std::vector<BodyModeSpec>(body13Modes.begin(), body13Modes.end())
                                                : classicalGuitarBR2StatisticalBodyModes();
  std::vector<double> areaV = body13 ? std::vector<double>(kClassicalGuitarBR2CompositeAreasM2.begin(), kClassicalGuitarBR2CompositeAreasM2.end())
                                     : classicalGuitarBR2StatisticalAreasM2();
  const std::span<const double> areas(areaV);
  for (size_t k = 0; k < notes.size(); ++k) { auto& n = notes[k]; if (!n.c) { ++fail; continue; }
    const size_t a = (size_t)(n.s * FS); if (a >= mix.size()) continue;
    size_t len = std::min((size_t)((ends[k] - n.s) * FS), mix.size() - a);
    auto loss = classicalGuitarStringLoss(*n.c, n.str); if (!loss.ok()) { ++fail; continue; }
    const double frac = pluckFractionFromBridge(dB, n.c->geometry.vibratingLength.value());
    auto vo = SoundboardVoice::create(*n.c, Excitation{ExcitationKind::Pluck, frac, 1.0e-4 * n.v / 127.0 * prof.pluckForceScale}, loss.value(), std::span<const BodyModeSpec>(body.data(), body.size()), areas, 1.0, 101325.0, 293.15, SampleRateHz{FS});
    if (!vo.ok()) { ++fail; continue; }
    std::vector<double> x(len); auto vv = std::move(vo).value(); if (!vv.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    const size_t fo = std::min<size_t>(len, prof.fingerDampSec > 0.0 ? (size_t)(prof.fingerDampSec * FS) : 480);  // 10 ms damp at re-pluck (finger contact), VERIFY
    for (size_t i = 0; i < len; ++i) { double g = 1.0; if (i >= len - fo) g = double(len - i) / fo; mix[a + i] += x[i] * g; }
  }
  std::printf("guitar notes=%zu fail=%d\n", notes.size(), fail);
  FILE* o = std::fopen(outPath, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
int main(int argc, char** argv) {
  if (argc < 4) return 1;
  PROFILE = std::getenv("BP_PROFILE") && std::atoi(std::getenv("BP_PROFILE")) == 1;
  if (!std::strcmp(argv[1], "guitar")) return renderGuitar(argv[3], std::atof(argv[5]), argv[4]);
  std::vector<Ev> ev; double total = 0;
  if (!std::strcmp(argv[1], "oldpiano")) return renderOldPiano(!std::strcmp(argv[2], "heavy"), argv[3]);
  const bool piano = !std::strcmp(argv[1], "piano");
  if (piano && !std::strcmp(argv[2], "table")) {  // the five Table III keys, as oldpiano
    double s = 0.5; for (int m : {27, 36, 53, 73, 91}) { ev.push_back({s, 5.5, m, 80}); s += 6.0; } total = s + 0.5;
    return renderPiano(ev, total, argv[3]);
  }
  if (piano && !std::strcmp(argv[2], "notes")) {
    double s = 0.5; for (int i = 6; i <= 36; i += 3) { ev.push_back({s, 5.5, 21 + 2 * i, 80}); s += 6.0; } total = s + 0.5;
    return renderPiano(ev, total, argv[3]);
  }
  const bool vcl = !std::strcmp(argv[1], "cello");
  const bool snr = !std::strcmp(argv[1], "snare");
  if (snr && !std::strcmp(argv[2], "notes")) {  // 6 (v/127)^2 = 1, 3, 6 m/s; last 3 m/s snares off
    double s = 0.3; for (int v : {52, 90, 127, 90}) { ev.push_back({s, 0.8, 0, double(v)}); s += 1.5; } total = s + 0.5;
    return renderSnare(ev, total, argv[3], true);
  }
  if (vcl && !std::strcmp(argv[2], "notes")) {  // C2..G4, the Sarabande compass
    double s = 0.3; for (int m : {36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67}) { ev.push_back({s, 1.3, m, 40}); s += 2.0; } total = s + 0.7;
    return renderCello(ev, total, argv[3]);
  }
  if (!std::strcmp(argv[2], "notes")) {
    const int ms[] = {28, 30, 31, 34, 36, 38, 40, 42, 44, 45, 49, 52, 56, 59};  // E1 .. B3 (VSCO refs)
    double s = 0.5; for (int m : ms) { ev.push_back({s, 3.0, m, 100}); s += 3.5; } total = s + 0.5;
    return renderBass(ev, total, argv[3]);
  }
  FILE* in = std::fopen(argv[3], "r"); if (!in) return 1; total = std::atof(argv[5]);
  Ev e; int vi; while (std::fscanf(in, "%lf,%lf,%d,%d", &e.s, &e.d, &e.m, &vi) == 4) { e.v = vi; ev.push_back(e); }
  return piano ? renderPiano(ev, total, argv[4]) : vcl ? renderCello(ev, total, argv[4]) : snr ? renderSnare(ev, total, argv[4], false) : renderBass(ev, total, argv[4]);
}
