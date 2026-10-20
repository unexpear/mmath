// Band pro-check harness (tools/demo/band): double bass pizzicato (and,
// later, piano) from the engine headers. Usage:
//   band1 bass notes out.f64            isolated notes, 3.5 s apart (refs list)
//   band1 bass score in.csv out.f64 T    csv rows start,dur,midi,vel
//   band1 piano notes|score ...          same, piano (PianoNoteVoice); the
//     damper falls at the written end (or the next strike of the key).
#include <mmathen/instruments/double_bass_pizz.h>
#include <mmathen/instruments/piano_voice.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace mmathen;
static const double FS = 48000.0;
static double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
struct Ev { double s, d; int m, v; };
static int renderBass(const std::vector<Ev>& ev, double total, const char* out) {
  auto bass = doubleBass(); if (!bass.ok()) return 2;
  std::vector<double> mix((size_t)(total * FS), 0.0); int fail = 0;
  const double amp = std::getenv("BB_AMP") ? std::atof(std::getenv("BB_AMP")) : 2.0e-3;
  for (size_t k = 0; k < ev.size(); ++k) {
    const Ev& e = ev[k]; auto note = doubleBassNote(bass.value(), midiHz(e.m)); if (!note.ok()) { ++fail; continue; }
    // A note rings until its written end or the next note on the same string.
    double end = e.s + e.d;
    for (size_t j = k + 1; j < ev.size(); ++j) { auto nj = doubleBassNote(bass.value(), midiHz(ev[j].m));
      if (nj.ok() && nj.value().stringIndex == note.value().stringIndex) { end = std::min(end, ev[j].s); break; } }
    const size_t a = (size_t)(e.s * FS); if (a >= mix.size()) continue;
    const size_t damp = (size_t)(kDoubleBassPizzDampSec * FS);
    size_t len = std::min((size_t)((end - e.s) * FS) + damp, mix.size() - a);
    const double t6s = std::getenv("BB_T6S") ? std::atof(std::getenv("BB_T6S")) : kDoubleBassPizzStoppedT60At6F0;
    auto vo = doubleBassPizzVoice(note.value(), amp * e.v / 127.0, SampleRateHz{FS}, 1.0, t6s); if (!vo.ok()) { ++fail; continue; }
    auto v = std::move(vo).value(); std::vector<double> x(len);
    if (!v.renderBlock(x.data(), FrameCount{len}).ok()) { ++fail; continue; }
    for (size_t i = 0; i < len; ++i) { double g = 1.0; if (i + damp >= len) g = double(len - i) / damp; mix[a + i] += x[i] * g; }
  }
  std::printf("bass notes=%zu fail=%d\n", ev.size(), fail);
  FILE* o = std::fopen(out, "wb"); std::fwrite(mix.data(), 8, mix.size(), o); std::fclose(o); return fail ? 3 : 0;
}
static int renderPiano(const std::vector<Ev>& ev, double total, const char* out) {
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
    const int step = (int)std::lround(e.v * 5.0 / 127.0);
    const double vel = std::getenv("PN_V") ? std::atof(std::getenv("PN_V")) : pianoHammerSpeed(step);
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
int main(int argc, char** argv) {
  if (argc < 4) return 1;
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
  if (!std::strcmp(argv[2], "notes")) {
    const int ms[] = {28, 30, 31, 34, 36, 38, 40, 42, 44, 45, 49, 52, 56, 59};  // E1 .. B3 (VSCO refs)
    double s = 0.5; for (int m : ms) { ev.push_back({s, 3.0, m, 100}); s += 3.5; } total = s + 0.5;
    return renderBass(ev, total, argv[3]);
  }
  FILE* in = std::fopen(argv[3], "r"); if (!in) return 1; total = std::atof(argv[5]);
  Ev e; while (std::fscanf(in, "%lf,%lf,%d,%d", &e.s, &e.d, &e.m, &e.v) == 4) ev.push_back(e);
  return piano ? renderPiano(ev, total, argv[4]) : renderBass(ev, total, argv[4]);
}
