#pragma once

// Player builder audition (step 23): a short phrase played by any custom
// player (mmathen/instruments/player_builder.h), for the desktop app's
// Play button. The voices and their set-up are the band harnesses' own
// (tools/demo/band/band2.cpp, band3.cpp, tools/demo/winds_pro1/wp1.cpp,
// tools/demo/sleigh_bells/sbpat.cpp and the glock / celesta / chimes /
// handbell harnesses), copied here so the app needs no tool. Harness
// numbers that are not physics data (mix gains, ring lengths, the
// velocity-to-stroke mappings) are marked as in those files.
//
// renderAudition returns mono samples in the voice's own units (force,
// pressure, acceleration); the app scales it for playback with one gain
// per instrument taken from the built-in player, so a louder custom player
// is heard louder.

#include <mmathen/instruments/player_builder.h>
#include <mmathen/instruments/player_profile.h>
#include <mmathen/instruments/bowed_note.h>
#include <mmathen/instruments/cello_voice.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/double_bass_pizz.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/guitar_pluck.h>
#include <mmathen/instruments/soundboard_voice.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/electric_guitar.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/piano_voice.h>
#include <mmathen/instruments/snare_drum.h>
#include <mmathen/instruments/cavity_drum.h>
#include <mmathen/instruments/drum.h>
#include <mmathen/instruments/cymbal.h>
#include <mmathen/instruments/cymbal_vk.h>
#include <mmathen/instruments/mallet_voice.h>
#include <mmathen/instruments/glockenspiel.h>
#include <mmathen/instruments/celesta.h>
#include <mmathen/instruments/tubular_bells.h>
#include <mmathen/instruments/handbell.h>
#include <mmathen/instruments/sleigh_bell.h>
#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/instruments/phrase_player.h>
#include <mmathen/physics/medium.h>
#include <mmathen/music/expressive_timing.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace app::audition {
using namespace mmathen;

inline constexpr double kRate = 48000.0;
inline constexpr double kBeatSec = 0.30;  // 200 bpm quarter notes, the band's Jingle Bells pace (harness)

// Short: the tune's first four notes (the test suite's quick render).
enum class Phrase { Tune = 0, Scale = 1, Short = 2 };
// Phrase names in plain English for the instrument (drums play rhythms).
[[nodiscard]] inline const char* phraseLabel(PlayerInstrument i, Phrase p) {
  const bool drums = i == PlayerInstrument::SnareDrum || i == PlayerInstrument::DrumKit || i == PlayerInstrument::SleighBells;
  if (p == Phrase::Tune) return drums ? "Jingle Bells rhythm" : "Jingle Bells (first line)";
  return drums ? "Steady beat, getting louder" : "Scale up and down";
}

// The tonic (MIDI) each instrument plays the phrase from: inside its range.
[[nodiscard]] inline int tonicFor(PlayerInstrument i) {
  using I = PlayerInstrument;
  switch (i) {
    case I::Violin: return 72;
    case I::Viola: return 60;
    case I::Cello: return 48;
    case I::DoubleBassArco: case I::DoubleBassPizz: return 36;
    case I::Glockenspiel: return 84;
    case I::Celesta: case I::Handbells: case I::Flute: return 72;
    case I::Trombone: return 48;
    case I::Trumpet: return 65;  // the phrase trumpet's 1.70 m bore cannot reach C4 (1.90 m)
    default: return 60;
  }
}

// The written phrase: Jingle Bells' first line (E E E- | E E E- | E G C. D | E---)
// or a one-octave major scale up and down, from the tonic.
[[nodiscard]] inline std::vector<PartNote> writtenPhrase(PlayerInstrument i, Phrase p, double startSec = 0.25) {
  std::vector<PartNote> out;
  const int t = tonicFor(i);
  const double b = kBeatSec;
  auto add = [&](double beat, double beats, int semis, double vel) { out.push_back({startSec + beat * b, beats * b, t + semis, vel}); };
  if (p == Phrase::Tune || p == Phrase::Short) {
    const int jbS[] = {4, 4, 4, 4, 4, 4, 4, 7, 0, 2, 4};
    const double jbB[] = {1, 1, 2, 1, 1, 2, 1, 1, 1.5, 0.5, 4};
    double beat = 0;
    const int count = p == Phrase::Short ? 4 : 11;
    for (int k = 0; k < count; ++k) { add(beat, jbB[k], jbS[k], (k == 0 || k == 3 || k == 6 || k == 10) ? 92.0 : 76.0); beat += jbB[k]; }
  } else {
    const int sc[] = {0, 2, 4, 5, 7, 9, 11, 12, 11, 9, 7, 5, 4, 2, 0};
    for (int k = 0; k < 15; ++k) add(k, k == 14 ? 2.0 : 1.0, sc[k], 80.0);
  }
  return out;
}
[[nodiscard]] inline double phraseSeconds(PlayerInstrument i, Phrase p) {
  if (i == PlayerInstrument::DrumKit) return p == Phrase::Tune ? 0.25 + 20 * kBeatSec + 3.2 : p == Phrase::Short ? 0.25 + 4 * kBeatSec + 1.0 : 0.25 + 16 * kBeatSec + 1.2;  // kitPart
  const auto w = writtenPhrase(i, p);
  double end = 0;
  for (const auto& n : w) end = std::max(end, n.startSec + n.durSec);
  const bool ringing = i == PlayerInstrument::TubularBells || i == PlayerInstrument::Handbells || i == PlayerInstrument::DrumKit;
  return end + (ringing ? 2.5 : 1.2);  // ring-out tail (harness)
}

namespace detail {
inline double midiHz(int m) { return 440.0 * std::pow(2.0, (m - 69) / 12.0); }
inline void addInto(std::vector<double>& mix, std::size_t a, const std::vector<double>& x, double g = 1.0) {
  for (std::size_t i = 0; i < x.size() && a + i < mix.size(); ++i) mix[a + i] += g * x[i];
}
inline bool cancelled(const std::atomic<bool>* c) { return c && c->load(); }
inline void progress(std::atomic<float>* p, std::size_t k, std::size_t n) {
  if (p && n) p->store(static_cast<float>(k + 1) / static_cast<float>(n));
}

// One rendered note: samples to add at frame `at`, times `gain`.
struct NoteOut { std::size_t at = 0; std::vector<double> x; double gain = 1.0; bool ok = true; bool skip = false; };
// Renders notes 0..n-1 with f(k, out) on all cores (the notes are
// independent voices), then adds them in note order, so the sum is the same
// whatever the thread timing.
template <typename F>
bool renderNotes(std::size_t n, F&& f, std::vector<double>& mix, const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  std::vector<NoteOut> outs(n);
  std::atomic<std::size_t> next{0}, done{0};
  auto work = [&] {
    for (std::size_t k = next.fetch_add(1); k < n; k = next.fetch_add(1)) {
      if (cancelled(cancel)) { outs[k].ok = false; continue; }
      f(k, outs[k]);
      progress(prog, done.fetch_add(1), n);
    }
  };
  const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
  const std::size_t threads = std::min<std::size_t>(n, hw);
  std::vector<std::thread> pool;
  for (std::size_t t = 1; t < threads; ++t) pool.emplace_back(work);
  work();
  for (auto& t : pool) t.join();
  for (const NoteOut& o : outs) {
    if (!o.ok) return false;
    if (!o.skip) addInto(mix, o.at, o.x, o.gain);
  }
  return true;
}


// The phrasing rules (PlayerExpression -> TimingRules, expressive_timing.h)
// on a written part: notes sharing an onset move together; the level
// scales the velocity; the intonation offset (cents) is returned per note.
inline std::vector<PartNote> expressed(const std::vector<PartNote>& w, const PlayerRig& r, std::vector<double>* cents = nullptr) {
  if (cents) cents->assign(w.size(), 0.0);
  const PlayerExpression& x = r.expression;
  if (x.isDefault() || w.empty()) return w;
  std::vector<double> onsets;
  for (const PartNote& n : w) onsets.push_back(n.startSec);
  std::sort(onsets.begin(), onsets.end());
  onsets.erase(std::unique(onsets.begin(), onsets.end(), [](double a, double b) { return std::fabs(a - b) < 1e-6; }), onsets.end());
  std::vector<TimedScoreNote> score;
  for (std::size_t g = 0; g < onsets.size(); ++g) {
    double ioi = 0.0;
    if (g + 1 < onsets.size()) ioi = onsets[g + 1] - onsets[g];
    else for (const PartNote& n : w) if (std::fabs(n.startSec - onsets[g]) < 1e-6) ioi = std::max(ioi, n.durSec);
    score.push_back({onsets[g], ioi, 60, 64});
  }
  TimingRules tr;
  tr.phraseScoreSeconds = kPlayerPhraseArchSeconds;
  tr.phraseArchDepth = x.tempoArch;
  tr.levelArchDb = x.swellDb;
  tr.phraseLevelSdDb = x.phraseLevelSdDb;
  tr.intonationSdCents = x.intonationSdCents;
  tr.detachedRatio = x.noteLength;
  tr.finalRitardScoreSeconds = x.endingSlowdown != 1.0 ? 1.5 : 0.0;
  tr.finalTempoRatio = x.endingSlowdown;
  // Phrase arch needs a non-zero depth to apply the swell (applyExpressiveTiming).
  if (tr.phraseArchDepth == 0.0 && (tr.levelArchDb != 0.0 || tr.phraseLevelSdDb != 0.0)) tr.phraseArchDepth = 1e-9;
  const PlayerFeel& feel = r.family() == PlayerFamily::Kit ? r.kit.feel : r.family() == PlayerFamily::Plucked ? r.plucked.feel
                           : r.family() == PlayerFamily::Struck ? r.struck.feel : r.bowed.feel;
  tr.seed = static_cast<std::uint32_t>(feel.seed & 0xffffffffu);
  const std::vector<PerformedNote> perf = applyExpressiveTiming(score, tr);
  std::vector<PartNote> out = w;
  for (std::size_t k = 0; k < w.size(); ++k) {
    const std::size_t g = static_cast<std::size_t>(std::lower_bound(onsets.begin(), onsets.end(), w[k].startSec - 1e-6) - onsets.begin());
    const PerformedNote& p = perf[std::min(g, perf.size() - 1)];
    const double stretch = score[g].ioiSeconds > 0.0 ? p.ioiSeconds / score[g].ioiSeconds : 1.0;
    out[k].startSec = onsets.front() + (p.onsetSeconds - perf.front().onsetSeconds);
    out[k].durSec = std::max(0.02, w[k].durSec * stretch * std::clamp(x.noteLength, 0.0, 1.0));
    out[k].velocity = std::clamp(w[k].velocity * p.level, 1.0, 127.0);
    if (cents) (*cents)[k] = p.centsOffset;
  }
  return out;
}

// --- bowed (band2 renderCello, band3 renderBowed) ---
inline bool renderBowed(const PlayerRig& r, const std::vector<PartNote>& w0, std::vector<double>& mix,
                        const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  std::vector<double> cents;
  const std::vector<PartNote> w = expressed(w0, r, &cents);
  const std::vector<PartNote> ev = performNotes(w, r.bowed.feel);  // order kept, so cents[k] stays with note k
  const std::size_t N = mix.size();
  const PlayerExtras& xt = r.extras;
  const bool cello = r.instrument == PlayerInstrument::Cello;
  BowedVoicing voicing = cello ? celloBowedVoicing() : playerBowedVoicing(r.instrument);
  const bool voicingDefault = xt.bowAttackSec < 0.0 && xt.bowReleaseSec < 0.0 && xt.vibratoRampSec < 0.0;
  if (xt.bowAttackSec >= 0.0) voicing.attackSeconds = xt.bowAttackSec;
  if (xt.bowReleaseSec >= 0.0) voicing.releaseSeconds = xt.bowReleaseSec;
  if (xt.vibratoRampSec >= 0.0) voicing.vibratoRampSeconds = xt.vibratoRampSec;
  if (cello) {
    auto vc = mmathen::cello(); if (!vc.ok()) return false;
    // The cello's habits on makeBowedNote (bowed_note.h: with the cello
    // voicing and these habits it renders what makeCelloNote does); the
    // built-in path stays makeCelloNote.
    BowedPlayerHabits h;
    h.bowForceScale = r.bowed.cello.bowForceScale; h.bowSpeedScale = r.bowed.cello.bowSpeedScale;
    h.contactPointScale = r.bowed.cello.contactPointScale; h.vibratoHz = r.bowed.cello.vibratoHz;
    h.vibratoCents = r.bowed.cello.vibratoCents; h.vibratoOnsetSeconds = r.bowed.cello.vibratoOnsetSeconds;
    std::vector<double> force(N, 0.0);
    const bool ok = renderNotes(ev.size(), [&](std::size_t k, NoteOut& o) {
      const PartNote& e = ev[k];
      const double bowSec = std::max(0.05, e.durSec - 0.02);
      o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= N) { o.skip = true; return; }
      const double hz = midiHz(e.midi) * std::pow(2.0, cents[k] / 1200.0);
      auto v = voicingDefault ? makeCelloNote(vc.value(), hz, bowSec, e.velocity / 80.0, SampleRateHz{kRate}, true, r.bowed.cello)
                              : makeBowedNote(vc.value(), hz, bowSec, e.velocity / 80.0, SampleRateHz{kRate}, voicing, true, h);
      if (!v.ok()) { o.ok = false; return; }
      auto voice = std::move(v).value();
      if (xt.bowNoise > 0.0) voice.setForceJitter(xt.bowNoise);
      o.x.assign(std::min(static_cast<std::size_t>((bowSec + 1.5) * kRate), N - o.at), 0.0);
      o.ok = voice.renderBlock(o.x.data(), FrameCount{o.x.size()}).ok();
    }, force, cancel, prog);
    if (!ok) return false;
    auto made = CelloBody::create(SampleRateHz{kRate}); if (!made.ok()) return false;
    auto body = std::move(made).value();
    return body.processAdd(force.data(), mix.data(), N);
  }
  const bool vn = r.instrument == PlayerInstrument::Violin, va = r.instrument == PlayerInstrument::Viola;
  auto inst = vn ? violin() : va ? viola() : doubleBass(); if (!inst.ok()) return false;
  const bool ok = renderNotes(ev.size(), [&](std::size_t k, NoteOut& o) {
    const PartNote& e = ev[k];
    o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= N) { o.skip = true; return; }
    const double hz = midiHz(e.midi) * std::pow(2.0, cents[k] / 1200.0);
    auto vo = makeBowedNote(inst.value(), hz, e.durSec, e.velocity / 80.0, SampleRateHz{kRate}, voicing, true, r.bowed.strings);
    if (!vo.ok()) { o.ok = false; return; }
    auto v = std::move(vo).value();
    if (xt.bowNoise > 0.0) v.setForceJitter(xt.bowNoise);
    o.x.assign(std::min(static_cast<std::size_t>((e.durSec + voicing.releaseSeconds + 0.4) * kRate), N - o.at), 0.0);  // 0.4 s ring (harness)
    o.ok = v.renderBlock(o.x.data(), FrameCount{o.x.size()}).ok();
  }, mix, cancel, prog);
  if (!ok) return false;
  if (voicing.body.present) {
    BowedBody body(voicing.body, kRate);
    body.processInPlace(mix);
  }
  return true;
}

// --- plucked: classical guitar (band2 renderGuitar) ---
inline StringCourse classicalCourse(const char* label, int stringNumber) {
  auto g = classicalGuitar(); const auto* s = findClassicalGuitarString(g.value(), label);
  const double L = g.value().vibratingLength.value(); const double f = makePitch(label).value().frequency.value();
  const double T = classicalGuitarStringTensionN(stringNumber, L), mu = 1e3 * T / ((2 * L * f) * (2 * L * f));
  return tuneClassicalGuitarString(g.value(), *s, mu * 0.995, mu * 1.005, T * 0.99, T * 1.01).value();
}
inline bool renderGuitar(const PlayerRig& r, const std::vector<PartNote>& w, std::vector<double>& mix, double total,
                         const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  const PluckedPlayerProfile& prof = r.plucked;
  const double dB = prof.pluckFromBridgeM > 0.0 ? prof.pluckFromBridgeM : kClassicalGuitarRoundPluckFromBridgeM;
  const std::vector<PartNote> ev = performNotes(expressed(w, r), prof.feel);
  static const std::vector<StringCourse> open = [] {
    std::vector<StringCourse> o; int sn = 6;
    for (const char* l : {"E2", "A2", "D3", "G3", "B3", "E4"}) o.push_back(classicalCourse(l, sn--));
    return o;
  }();
  struct N { double s; double v; int str; std::optional<StringCourse> c; };
  std::vector<N> notes;
  for (const PartNote& e : ev) {
    const double f = midiHz(e.midi); N n{e.startSec, e.velocity, 0, {}};
    for (int i = 5; i >= 0; --i) {
      const double f0 = open[static_cast<std::size_t>(i)].tuning.frequency.value();
      if (f0 <= f * 1.0005) {
        n.str = 6 - i;
        if (f / f0 < 1.0005) n.c.emplace(open[static_cast<std::size_t>(i)]);
        else { auto st = fingerStop(open[static_cast<std::size_t>(i)], 1.0 - f0 / f); if (st.ok()) n.c.emplace(st.value()); }
        break;
      }
    }
    notes.push_back(n);
  }
  std::vector<double> on; std::vector<int> sn;
  for (auto& n : notes) { on.push_back(n.s); sn.push_back(n.str); }
  const auto ends = letRingEndsBassDamped(on, sn, total);
  const std::vector<BodyModeSpec> body = classicalGuitarBR2StatisticalBodyModes();
  const std::vector<double> areaV = classicalGuitarBR2StatisticalAreasM2();
  const std::span<const double> areas(areaV);
  const double dampSec = prof.fingerDampSec > 0.0 ? prof.fingerDampSec : kGuitarFingerDampSec;
  return renderNotes(notes.size(), [&](std::size_t k, NoteOut& o) {
    const auto& n = notes[k]; if (!n.c) { o.ok = false; return; }
    o.at = static_cast<std::size_t>(n.s * kRate); if (o.at >= mix.size()) { o.skip = true; return; }
    const std::size_t len = std::min(static_cast<std::size_t>(std::max(0.0, ends[k] - n.s) * kRate), mix.size() - o.at);
    auto loss = classicalGuitarStringLoss(*n.c, n.str); if (!loss.ok()) { o.ok = false; return; }
    const double frac = pluckFractionFromBridge(dB, n.c->geometry.vibratingLength.value());
    auto vo = SoundboardVoice::create(*n.c, Excitation{ExcitationKind::Pluck, frac, 1.0e-4 * n.v / 127.0 * prof.pluckForceScale}, loss.value(),
                                      std::span<const BodyModeSpec>(body.data(), body.size()), areas, 1.0, 101325.0, 293.15, SampleRateHz{kRate});
    if (!vo.ok()) { o.ok = false; return; }
    o.x.assign(len, 0.0); auto vv = std::move(vo).value();
    if (!vv.renderBlock(o.x.data(), FrameCount{len}).ok()) { o.ok = false; return; }
    const std::size_t fo = std::max<std::size_t>(1, std::min<std::size_t>(len, static_cast<std::size_t>(dampSec * kRate)));
    for (std::size_t i = len > fo ? len - fo : 0; i < len; ++i) o.x[i] *= double(len - i) / double(fo);
  }, mix, cancel, prog);
}

// --- plucked: pizzicato bass (band2 renderBass) ---
inline bool renderBassPizz(const PlayerRig& r, const std::vector<PartNote>& w, std::vector<double>& mix,
                           const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  auto bass = doubleBass(); if (!bass.ok()) return false;
  const PluckedPlayerProfile& prof = r.plucked;
  const std::vector<PartNote> ev = performNotes(expressed(w, r), prof.feel);
  const double fromBridge = prof.pluckFromBridgeM > 0.0 ? prof.pluckFromBridgeM : kDoubleBassPizzFromBridgeM;
  const double amp = 2.0e-3;  // band2's BB_AMP default
  const std::size_t damp = std::max<std::size_t>(1, static_cast<std::size_t>((prof.fingerDampSec > 0.0 ? prof.fingerDampSec : kDoubleBassPizzDampSec) * kRate));
  return renderNotes(ev.size(), [&](std::size_t k, NoteOut& o) {
    const PartNote& e = ev[k]; auto note = doubleBassNote(bass.value(), midiHz(e.midi)); if (!note.ok()) { o.ok = false; return; }
    double end = e.startSec + e.durSec;
    for (std::size_t j = k + 1; j < ev.size(); ++j) {
      auto nj = doubleBassNote(bass.value(), midiHz(ev[j].midi));
      if (nj.ok() && nj.value().stringIndex == note.value().stringIndex) { end = std::min(end, ev[j].startSec); break; }
    }
    o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= mix.size()) { o.skip = true; return; }
    const std::size_t len = std::min(static_cast<std::size_t>(std::max(0.0, end - e.startSec) * kRate) + damp, mix.size() - o.at);
    auto vo = doubleBassPizzVoice(note.value(), amp * e.velocity / 127.0 * prof.pluckForceScale, SampleRateHz{kRate}, 1.0,
                                  kDoubleBassPizzStoppedT60At6F0, fromBridge);
    if (!vo.ok()) { o.ok = false; return; }
    auto v = std::move(vo).value(); o.x.assign(len, 0.0);
    if (!v.renderBlock(o.x.data(), FrameCount{len}).ok()) { o.ok = false; return; }
    for (std::size_t i = 0; i < len; ++i) if (i + damp >= len) o.x[i] *= double(len - i) / double(damp);
  }, mix, cancel, prog);
}

// --- plucked: harp, harpsichord, electric guitar (band3 renderPlucked) ---
inline LossModel pluckLoss(const StringCourse& c) {
  auto ideal = courseString(c); const double f = c.tuning.frequency.value();
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{2.0}, FrequencyHz{6.0 * f}, DurationSeconds{0.25}).value();
}
inline std::optional<StringCourse> stopTo(const StringCourse& open, double f) {
  const double f0 = open.tuning.frequency.value();
  if (f / f0 < 1.0005) return open;
  auto st = fingerStop(open, 1.0 - f0 / f); if (!st.ok()) return std::nullopt; return st.value();
}
inline std::optional<StringCourse> harpCourse(double f) {
  static const auto harp = concertPedalHarp().value();
  const HarpStringGeometry* best = nullptr;
  for (const auto& s : harp.strings) if (s.publishedFrequencyHz <= f * 1.0005) best = &s;
  if (!best) return std::nullopt;
  const double L = best->vibratingLengthCm * 1e-2, f0 = best->publishedFrequencyHz;
  const double t = 2.0e-3 * (2 * L * f0) * (2 * L * f0);  // 2 g/m (harness)
  auto c = tuneHarpString(*best, 1.0, 3.0, t * 0.5, t * 1.5); if (!c.ok()) return std::nullopt;
  return stopTo(c.value(), f);
}
inline std::optional<StringCourse> harpsichordCourse(double f, double* pointMm) {
  static const auto hc = frenchDouble().value();
  const HarpsichordSpeakingLength* best = nullptr;
  for (const auto& s : hc.strings)
    if (s.choir == HarpsichordChoir::LongEightFoot && makePitch(s.label).value().frequency.value() <= f * 1.0005) best = &s;
  if (!best) return std::nullopt;
  const double L = best->vibratingLengthMm * 1e-3, f0 = makePitch(best->label).value().frequency.value();
  const double t = 1.0e-3 * (2 * L * f0) * (2 * L * f0);  // 1 g/m (harness)
  auto c = tuneHarpsichordString(*best, 0.5, 1.5, t * 0.5, t * 1.5); if (!c.ok()) return std::nullopt;
  *pointMm = best->pluckingPointMm.value_or(0.0);
  return stopTo(c.value(), f);
}
inline std::optional<StringCourse> electricCourse(double f) {
  static const auto g = electricGuitar().value();
  std::optional<StringCourse> best;
  for (const char* l : {"E2", "A2", "D3", "G3", "B3", "E4"}) {
    const auto* s = findElectricGuitarString(g, l); const double L = g.vibratingLength.value();
    const double f0 = makePitch(l).value().frequency.value(); if (f0 > f * 1.0005) break;
    const double newtons = s->tensionPounds * kPoundForceNewtons;
    const double grams = newtons / ((2 * L * f0) * (2 * L * f0)) * 1e3;
    auto c = tuneElectricGuitarString(g, *s, grams); if (c.ok()) best = c.value();
  }
  if (!best) return std::nullopt;
  return stopTo(*best, f);
}
inline bool renderPlucked(const PlayerRig& r, const std::vector<PartNote>& w, std::vector<double>& mix,
                          const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  const bool hp = r.instrument == PlayerInstrument::Harp, hc = r.instrument == PlayerInstrument::Harpsichord;
  const PluckedPlayerProfile& prof = r.plucked;
  const std::vector<PartNote> ev = performNotes(expressed(w, r), prof.feel);
  return renderNotes(ev.size(), [&](std::size_t k, NoteOut& o) {
    const PartNote& e = ev[k];
    const double f = midiHz(e.midi); double pointMm = 0.0;
    const auto c = hp ? harpCourse(f) : hc ? harpsichordCourse(f, &pointMm) : electricCourse(f);
    if (!c) { o.ok = false; return; }
    const double L = c->geometry.vibratingLength.value();
    double frac = 0.5;  // mid-string (harness default)
    if (hc && pointMm > 0.0) frac = pluckFractionFromBridge(pointMm * 1e-3, L);
    if (prof.pluckFromBridgeM > 0.0) frac = pluckFractionFromBridge(prof.pluckFromBridgeM, L);
    else if (prof.pluckFraction > 0.0) frac = prof.pluckFraction;
    const double amp = 1.0e-4 * (hc ? 1.0 : e.velocity / 127.0) * prof.pluckForceScale;  // 0.1 mm at vel 127 (harness)
    auto vo = PluckedStringVoice::create(*c, Excitation{ExcitationKind::Pluck, frac, amp}, pluckLoss(*c), 0.0, 4, 0, SampleRateHz{kRate}, 0.23,
                                         hp || hc ? StringReadout::Displacement : StringReadout::CoilVelocity);
    if (!vo.ok()) { o.ok = false; return; }
    auto v = std::move(vo).value();
    o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= mix.size()) { o.skip = true; return; }
    const double damp = prof.fingerDampSec > 0.0 ? prof.fingerDampSec : (hc ? kHarpsichordDamperSec : kHarpFingerDampSec);
    const std::size_t len = std::min(static_cast<std::size_t>((e.durSec + damp) * kRate), mix.size() - o.at);
    const std::size_t fo = std::max<std::size_t>(1, static_cast<std::size_t>(damp * kRate));
    o.x.assign(len, 0.0); if (!v.renderBlock(o.x.data(), FrameCount{len}).ok()) { o.ok = false; return; }
    for (std::size_t i = len > fo ? len - fo : 0; i < len; ++i) o.x[i] *= double(len - i) / double(fo);
  }, mix, cancel, prog);
}

// --- struck: piano (band2 renderPiano), snare, mallets, handbells ---
inline bool renderPiano(const PlayerRig& r, std::vector<PartNote> w, std::vector<double>& mix,
                        const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  // Left-hand chords on each bar (quieter), so the key-action lead is heard.
  const std::size_t melody = w.size();
  for (std::size_t k = 0; k < melody; ++k) {
    const double beat = (w[k].startSec - w[0].startSec) / kBeatSec;
    if (std::fabs(beat / 4.0 - std::round(beat / 4.0)) > 1e-6) continue;
    for (int s : {-12, -8, -5}) w.push_back({w[k].startSec, 4.0 * kBeatSec, tonicFor(r.instrument) + s, 56.0});
  }
  std::stable_sort(w.begin(), w.end(), [](const PartNote& a, const PartNote& b) { return a.startSec < b.startSec; });
  const std::vector<PartNote> ev = performPiano(expressed(w, r), r.struck);
  const std::size_t damp = std::max<std::size_t>(1, static_cast<std::size_t>(kPianoDamperSec * kRate));
  return renderNotes(ev.size(), [&](std::size_t k, NoteOut& o) {
    const PartNote& e = ev[k]; double end = e.startSec + e.durSec;
    for (std::size_t j = k + 1; j < ev.size(); ++j) if (ev[j].midi == e.midi) { end = std::min(end, ev[j].startSec); break; }
    const bool damped = e.midi < kPianoFirstUndampedKey;
    if (!damped) end = std::max(end, e.startSec + 4.0);
    o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= mix.size()) { o.skip = true; return; }
    const std::size_t len = std::min(static_cast<std::size_t>(std::max(0.0, end - e.startSec) * kRate) + damp, mix.size() - o.at);
    auto vo = PianoNoteVoice::create(e.midi, pianoSpeedFor(r.struck, e.velocity), SampleRateHz{kRate}, 1.0, 0.0);
    if (!vo.ok()) { o.ok = false; return; }
    auto v = std::move(vo).value(); o.x.assign(len, 0.0);
    if (!v.renderAdd(o.x.data(), len).ok()) { o.ok = false; return; }
    if (damped) for (std::size_t i = 0; i < len; ++i) if (i + damp >= len) o.x[i] *= double(len - i) / double(damp);
  }, mix, cancel, prog);
}
inline bool renderStruck(const PlayerRig& r, const std::vector<PartNote>& w, std::vector<double>& mix,
                         const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  using I = PlayerInstrument;
  const std::vector<PartNote> ev = performNotes(expressed(w, r), r.struck.feel);
  return renderNotes(ev.size(), [&](std::size_t k, NoteOut& o) {
    const PartNote& e = ev[k];
    o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= mix.size()) { o.skip = true; return; }
    const double hz = midiHz(e.midi), x80 = e.velocity / 80.0;
    auto run = [&](auto& voice, double sec) {
      o.x.assign(std::min(static_cast<std::size_t>(sec * kRate), mix.size() - o.at), 0.0);
      return voice.renderBlock(o.x.data(), FrameCount{o.x.size()}).ok();
    };
    bool ok = false;
    if (r.instrument == I::SnareDrum) {
      const double x = e.velocity / 127.0;
      auto v = snareDrumHit(r.extras.stickSpeedAt127 * x * x, true, SampleRateHz{kRate});  // band1 stick map 6 (v/127)^2, UNSOURCED
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 0.6); }
    } else if (r.instrument == I::Glockenspiel) {
      BarListener li; li.polarRad = 30.0 * constants::pi / 180.0; li.azimuthRad = 30.0 * constants::pi / 180.0; li.distanceM = 1.0; li.baffle = kGlockCaseBaffle;
      const BarSpec bar = glockenspielBar(hz);
      auto v = StruckBarVoice::create(bar, glockenspielLosses(), glockenspielMallet(), 1.5 * x80, std::clamp(glockenspielStrikeFraction(bar.lengthM) * r.extras.strikeSpotScale, 0.02, 0.5),
                                      kGlockStrikeOffset, SampleRateHz{kRate}, li);  // glock1 1.5 m/s at velocity 80 (harness)
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 1.8); }
    } else if (r.instrument == I::Celesta) {
      auto v = celestaNote(hz, 1.0 * x80, SampleRateHz{kRate});  // cel1 1 m/s (harness)
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 1.6); }
    } else if (r.instrument == I::TubularBells) {
      BarLosses l; l.internalQ = kChimeInternalQ;
      BarListener li; li.polarRad = constants::pi / 3.0;
      auto v = StruckBarVoice::create(chimeTube(hz), l, chimeMallet(), 2.0 * x80, 0.0, 0.0, SampleRateHz{kRate}, li);  // tb1 2 m/s (harness)
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 2.5); }
      if (ok) { const std::size_t fo = std::min<std::size_t>(o.x.size(), 9600); for (std::size_t i = o.x.size() - fo; i < o.x.size(); ++i) o.x[i] *= double(o.x.size() - i) / double(fo); }
    } else {  // handbells
      auto v = HandbellVoice::create(hz, 1.0 * x80, SampleRateHz{kRate}, 1.0, 0.0, 4, 0.0, constants::pi / 2.0);  // hb1 1 m/s (harness)
      if (v.ok()) {
        auto vv = std::move(v).value();
        // Swung ringing: the swing speed follows the note's loudness like the struck speed does.
        ok = (r.extras.handbellSwingMps <= 0.0 || vv.setSwing(r.extras.handbellSwingMps * x80)) && run(vv, 3.0);
      }
    }
    o.ok = ok;
  }, mix, cancel, prog);
}

// --- drum kit (band3 renderKit) ---
inline MembraneHead kitHead(const MembraneShell& shell, double hz) {
  const double dmin = 0.20, dmax = 0.30, d = 0.5 * (dmin + dmax);
  const double alpha = besselJZero(0, 1).value();
  const double wave = 2.0 * 3.141592653589793 * shell.headRadius.value() * hz / alpha;
  const double t = d * wave * wave;
  return tuneMembraneHead(shell, hz, dmin, dmax, t * 0.5, t * 1.5).value();
}
inline DrumLimb limbFor(int m) { return m == 36 ? DrumLimb::RightFoot : (m == 42 || m == 49) ? DrumLimb::RightHand : DrumLimb::LeftHand; }
inline std::vector<PartNote> kitPart(Phrase p, double startSec = 0.25) {
  std::vector<PartNote> v; const double e8 = kBeatSec;  // eighth notes at 100 bpm (harness)
  if (p == Phrase::Tune) {
    // Two bars of a rock beat with a crash on the downbeat, then a tom fill and a final crash.
    for (int bar = 0; bar < 2; ++bar)
      for (int k = 0; k < 8; ++k) {
        const double t = startSec + (bar * 8 + k) * e8;
        if (!(bar == 0 && k == 0)) v.push_back({t, e8, 42, k % 2 ? 60.0 : 80.0});
        if (k == 0 || k == 4) v.push_back({t, e8, 36, 100.0});
        if (k == 2 || k == 6) v.push_back({t, e8, 38, 96.0});
      }
    v.push_back({startSec, e8, 49, 110.0});
    const int fill[] = {38, 38, 48, 48, 45, 45, 43, 43};
    for (int k = 0; k < 8; ++k) v.push_back({startSec + 16 * e8 + k * e8 / 2, e8 / 2, fill[k], 70.0 + 5.0 * k});
    v.push_back({startSec + 20 * e8, 4 * e8, 49, 118.0});
    v.push_back({startSec + 20 * e8, 4 * e8, 36, 110.0});
  } else {
    for (int k = 0; k < 16; ++k) {
      const double t = startSec + k * kBeatSec;
      v.push_back({t, e8 / 2, 42, 40.0 + 5.0 * k});
      v.push_back({t, e8 / 2, k % 2 ? 38 : 36, 40.0 + 5.0 * k});
    }
  }
  std::stable_sort(v.begin(), v.end(), [](const PartNote& a, const PartNote& b) { return a.startSec < b.startSec; });
  if (p == Phrase::Short) {  // the first bar's first half plus a crash: every kit piece once
    std::vector<PartNote> s;
    for (const PartNote& n : v) if (n.startSec < startSec + 4 * e8 - 1e-9) s.push_back(n);
    s.push_back({startSec + 3 * e8, e8 / 2, 48, 80.0});
    s.push_back({startSec, e8, 49, 110.0});
    std::stable_sort(s.begin(), s.end(), [](const PartNote& a, const PartNote& b) { return a.startSec < b.startSec; });
    return s;
  }
  return v;
}
inline bool renderKit(const PlayerRig& r, const std::vector<PartNote>& w, std::vector<double>& mix,
                      const std::atomic<bool>* cancel, std::atomic<float>* prog) {
  const DrumKitPlayerProfile& prof = r.kit;
  std::vector<KitNote> wk; for (const PartNote& n : expressed(w, r)) wk.push_back({n, limbFor(n.midi)});
  const std::vector<KitNote> pk = performDrumKit(wk, prof);
  static const auto kit = yamahaTourCustomKit().value();
  static const auto cym = sabianXsrCompleteSet().value();
  static const auto crashPlate = [] { VonKarmanPlateSpec sp; sp.radiusM = sabianXsrCompleteSet().value().crash16.radius.value(); return buildVonKarmanPlate(sp); }();
  const CymbalMaterial bronze{0.001, 1.0e11, 7800.0, 0.33};  // test_contact_voice.cpp plate numbers, UNSOURCED
  std::vector<int> nd, rd;
  for (int n = 2; n <= 14; ++n) { nd.push_back(n); rd.push_back(1); }
  for (int n = 0; n <= 8; ++n) { nd.push_back(n); rd.push_back(2); }
  for (int n = 0; n <= 4; ++n) { nd.push_back(n); rd.push_back(3); }
  constexpr double kVkCrashMixGain = 1.52e-3;  // band3, TUNED to the old crash level, not physics
  return renderNotes(pk.size(), [&](std::size_t k, NoteOut& o) {
    const PartNote& e = pk[k].note;
    o.at = static_cast<std::size_t>(e.startSec * kRate); if (o.at >= mix.size()) { o.skip = true; return; }
    const double speed = r.extras.stickSpeedAt127 * (e.velocity / 127.0) * (e.velocity / 127.0);
    o.gain = e.midi == 36 ? 0.25 : e.midi == 42 ? 3.0e4 : 1.0;  // band3 mix gains, TUNED for balance
    auto run = [&](auto& voice, double sec) {
      o.x.assign(std::min(static_cast<std::size_t>(sec * kRate), mix.size() - o.at), 0.0);
      return voice.renderBlock(o.x.data(), FrameCount{o.x.size()}).ok();
    };
    bool ok = false;
    if (e.midi == 38) {
      auto v = snareDrumHit(speed, true, SampleRateHz{kRate}); if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 0.6); }
    } else if (e.midi == 36 || e.midi == 43 || e.midi == 45 || e.midi == 48) {
      const MembraneShell& sh = e.midi == 36 ? kit.bassDrum : e.midi == 43 ? kit.floorTom : e.midi == 45 ? kit.rackTom12 : kit.rackTom10;
      const double hz = e.midi == 36 ? 60.0 : e.midi == 43 ? 80.0 : e.midi == 45 ? 110.0 : 140.0;
      const MembraneHead head = kitHead(sh, hz);
      const MalletStrike st = e.midi == 36 ? MalletStrike{0.0, 0.0, 0.03, speed, 8.0e4, 1.0, 0.0, 40.0}
                                           : MalletStrike{0.3, 0.0, kSnareStickMassKg, speed, kSnareStickStiffness, kSnareStickExponent, 0.0, 40.0};
      auto v = CavityDrumVoice::create(head, &head, sh, st, 0, 1, SampleRateHz{kRate}, 101325.0, 293.15, 1.0);
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, e.midi == 36 ? 0.8 : 1.0); }
    } else if (e.midi == 49) {
      const double cs = kitCrashStickSpeed(prof, e.velocity);
      const MalletStrike st{0.85, 0.0, kSnareStickMassKg, cs > 0.0 ? cs : speed, kSnareStickStiffness, kSnareStickExponent, 0.0, 600.0};
      if (crashPlate.ok()) {
        auto v = VonKarmanCymbalVoice::create(crashPlate.value(), st, SampleRateHz{kRate});
        if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 3.0); }
      }
      if (ok) { const std::size_t fo = std::min<std::size_t>(o.x.size(), 24000); for (std::size_t i = o.x.size() - fo; i < o.x.size(); ++i) o.x[i] *= double(o.x.size() - i) / double(fo); }
      o.gain = kVkCrashMixGain;
    } else if (e.midi == 42) {
      const MalletStrike st{0.6, 0.0, kSnareStickMassKg, speed, kSnareStickStiffness, kSnareStickExponent, 0.0, 30.0};
      auto v = PlateMalletVoice::create(cym.hiHatTop, bronze, st, nd.data(), rd.data(), static_cast<int>(nd.size()), SampleRateHz{kRate}, 0.45, 0.3);
      if (v.ok()) { auto vv = std::move(v).value(); ok = run(vv, 0.12); }
      if (ok) { const std::size_t fo = std::min<std::size_t>(o.x.size(), 960); for (std::size_t i = o.x.size() - fo; i < o.x.size(); ++i) o.x[i] *= double(o.x.size() - i) / double(fo); }
    }
    o.ok = ok;
  }, mix, cancel, prog);
}

// --- sleigh bells (sbpat) ---
inline std::vector<SleighBellEvent> sleighTimeline(Phrase p, double startSec = 0.25) {
  std::vector<SleighBellEvent> tl;
  const auto w = writtenPhrase(PlayerInstrument::SleighBells, p == Phrase::Short ? Phrase::Short : Phrase::Tune, startSec);
  double end = 0; for (const auto& n : w) end = std::max(end, n.startSec + n.durSec);
  SleighBellEvent sh; sh.kind = SleighBellEvent::Kind::Shake; sh.timeSec = 0.0; sh.durationSec = end; sh.strength = 0.017; sh.endStrength = 0.017;
  tl.push_back(sh);  // ref_timeline's steady 0.017 m shake
  SleighBellEvent fade = sh; fade.timeSec = end; fade.durationSec = 0.6; fade.endStrength = 0.0; tl.push_back(fade);
  if (p != Phrase::Scale) {
    for (const auto& n : w) {
      SleighBellEvent s; s.kind = SleighBellEvent::Kind::Strike; s.timeSec = n.startSec; s.strength = strikeSpeedForDb(n.velocity > 85 ? -3.0 : -9.0);
      s.lateral = tl.size() % 2 ? 0.3 : -0.3; tl.push_back(s);
    }
  } else {
    for (int k = 0; k < 16; ++k) {
      SleighBellEvent s; s.kind = SleighBellEvent::Kind::Strike; s.timeSec = startSec + k * kBeatSec; s.strength = strikeSpeedForDb(-15.0 + k);
      s.lateral = k % 2 ? 0.3 : -0.3; tl.push_back(s);
    }
  }
  return tl;
}
inline bool renderSleigh(const PlayerRig& r, Phrase p, std::vector<double>& mix) {
  const auto tl = performSleighBells(sleighTimeline(p), r.shaken);
  SleighBellStrap strap(kSleighBellCount, 7, kRate);
  ShakeGesture g; g.rateHz = r.extras.shakeHz; g.minimumJerkStrokes = r.extras.smoothStrokes;
  strap.render(mix.data(), mix.size(), tl, g);
  return true;
}

// --- winds (winds_pro1/wp1.cpp, its default settings) ---
inline ValveBlow sustained(double pressure, double cutoffHz, std::uint64_t attack) {
  ValveBlow b; b.pressurePa = pressure; b.frames = 96000; b.reflectionMagnitude = 0.995;
  b.initialDisplacementMeters = 2.0e-5; b.attackFrames = attack; b.listenerDistanceMeters = 1.0;
  b.reflectionCutoffHz = cutoffHz; return b;
}
inline double conePartial(double length, double apex, int mode) {
  constexpr double kAir = 101325.0, kTemp = 293.15;
  SaxophoneSpec spec{length, apex, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0, 2.0e-6, 1.4e-4, 7.0e-4, 0.09};
  auto s = makeSaxophone(spec); if (!s.ok()) return 0.0;
  auto m = saxophoneConeMode(s.value(), mode); return m.ok() ? m.value().value() : 0.0;
}
inline double impedancePeakHz(double length, double apex, int mode, double targetHz) {
  if (mode >= 1) return conePartial(length, apex, mode);
  double best = conePartial(length, apex, 1), gap = std::fabs(best - targetHz);
  for (int n = 2; n <= 8; ++n) { const double f = conePartial(length, apex, n); if (std::fabs(f - targetHz) < gap) { best = f; gap = std::fabs(f - targetHz); } }
  return best;
}
inline void push(std::vector<PhraseNote>& notes, const ScoreNote& src, double start, double length, double lip, double pressure, double jet, double releaseSec = kReedBreathReleaseSec) {
  PhraseNote n; n.name = src.name; n.startSec = start; n.targetHz = src.hertz; n.lengthM = length; n.lipHz = lip;
  n.pressure = src.hertz > 0.0 ? pressure : 0.0; n.jetSamples = jet; n.tongue = src.tongued; n.vibrato = src.vibrato;
  n.dynamic = src.dynamic; n.rest = !(src.hertz > 0.0); n.releaseSec = releaseSec;
  notes.push_back(n);
}
inline std::vector<ScoreNote> windScore(const std::vector<PartNote>& w, bool slurredScale) {
  std::vector<ScoreNote> s;
  for (std::size_t k = 0; k < w.size(); ++k) {
    // Notes are tongued, except: the scale is slurred in groups of four,
    // and in the tune the short note after a dotted one (C. D) is slurred
    // into. Notes of a beat or more take vibrato.
    const bool newPitch = k > 0 && w[k].midi != w[k - 1].midi;
    const bool slur = newPitch && (slurredScale ? k % 4 != 0
                                                : w[k - 1].durSec > 1.2 * kBeatSec && w[k].durSec < 0.9 * kBeatSec);
    s.push_back(ScoreNote{"n", midiHz(w[k].midi), w[k].durSec, !slur, w[k].durSec >= kBeatSec * 0.99, "mf"});
  }
  s.push_back(ScoreNote{"rest", 0.0, 0.8, false, false, "mf"});
  return ensureLeadIn(s);
}
// onsetCents (tests, step 24.2a): a forced onset error in cents for each
// written note, added to the player's random one (PhraseNote::onsetErrorCents).
// scoopCents (tests, step 24.2b): a forced scoop depth per written note
// (PhraseNote::scoopCents).
inline bool renderWind(const PlayerRig& r, const std::vector<PartNote>& w, bool slurredScale, std::vector<double>& mix,
                       const std::vector<double>* onsetCents = nullptr, const std::vector<double>* scoopCents = nullptr) {
  using I = PlayerInstrument;
  constexpr double kAir = 101325.0, kTemp = 293.15;
  const SampleRateHz rate{kRate};
  const auto sp = speedOfSound(Air{TemperatureKelvin{kTemp}}); if (!sp.ok()) return false;
  const double speed = sp.value().value();
  const WindPlayerProfile& prof = r.wind;
  PlayerAirway airway(prof.airwaySpec());
  const std::vector<ScoreNote> song = windScore(w, slurredScale);
  double T = 0; for (const auto& n : song) T += n.seconds;
  std::vector<PhraseNote> notes; std::vector<double> out;
  auto applyErrors = [&] {
    if (notes.size() < w.size() + 1) return;
    const std::size_t off = notes.size() - w.size() - 1;  // the lead-in rest; the closing rest is last
    if (onsetCents != nullptr)
      for (std::size_t k = 0; k < w.size() && k < onsetCents->size(); ++k) notes[k + off].onsetErrorCents = (*onsetCents)[k];
    if (scoopCents != nullptr)
      for (std::size_t k = 0; k < w.size() && k < scoopCents->size(); ++k) notes[k + off].scoopCents = (*scoopCents)[k];
  };
  // A flute scoop lengthens the air column (the lip covers the hole) and
  // the bore's delay line cannot grow, so a scooping flute is built
  // longer by the deepest scoop. Without scoops the build is unchanged.
  double scoopMax = prof.scoopCents > 0.0 && prof.scoopSec >= 0.0 ? prof.scoopCents : 0.0;
  if (scoopCents != nullptr) for (double c : *scoopCents) scoopMax = std::max(scoopMax, c);
  const double scoopRoom = scoopMax > 0.0 ? std::pow(2.0, (kFluteScoopCoverGain * scoopMax + 25.0) / 1200.0) : 1.0;
  const double reedRoom = scoopMax > 0.0 ? std::pow(2.0, (kReedScoopLengthGain * scoopMax + 25.0) / 1200.0) : 1.0;
  const double brassRoom = scoopMax > 0.0 ? std::pow(2.0, (kBrassScoopLengthGain * scoopMax + 25.0) / 1200.0) : 1.0;
  const double rel = r.extras.reedReleaseSec >= 0.0 ? r.extras.reedReleaseSec : kReedBreathReleaseSec;
  const double noise = r.extras.breathNoiseScale;
  if (r.instrument == I::Clarinet) {
    double longest = 0; for (auto& s : song) if (s.hertz > 0) longest = std::max(longest, clarinetSoundingLengthMeters(s.hertz));
    ClarinetSpec spec{longest * 1.04 * reedRoom, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0, 1.0e-6, 1.0e-4, 1.0e-3, 0.11};
    auto clR = makeClarinet(spec); if (!clR.ok()) return false; auto cl = std::move(clR).value();
    ValveBlow blow = clarinetBoreLosses(sustained(4000.0, 1500.0, 19200)); blow.frames = static_cast<std::uint64_t>(std::llround((T + 0.5) * kRate));
    blow.reedTurbulence = kClarinetReedTurbulence * noise; blow.reflectionCutoffHz = kClarinetReflectionCutoffHz; blow.wallFitFundamental = true;
    blow.lockedBreathFraction = kClarinetLockedBreathFraction;
    auto voiceR = BoreValveVoice::fromClarinet(cl, blow, rate); if (!voiceR.ok()) return false; auto voice = std::move(voiceR).value();
    double start = 0, held = longest;
    for (auto& s : song) { const double L = s.hertz > 0 ? clarinetSoundingLengthMeters(s.hertz) : held; push(notes, s, start, L, 0, 4000.0, 0, rel); if (s.hertz > 0) held = L; start += s.seconds; }
    applyErrors();
    out = playPhrase(voice, notes, T, false, 0.50, &airway, kRate, prof);
  } else if (r.instrument == I::AltoSaxophone) {
    double longest = 0; for (auto& s : song) if (s.hertz > 0) longest = std::max(longest, saxophoneSoundingLengthMeters(s.hertz));
    SaxophoneSpec spec{longest * 1.08 * reedRoom, 0.20, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0, 2.0e-6, 1.4e-4, 7.0e-4, 0.09};
    auto saxR = makeSaxophone(spec); if (!saxR.ok()) return false; auto sax = std::move(saxR).value();
    ValveBlow blow = sustained(4200.0, 1500.0, 19200); blow.frames = static_cast<std::uint64_t>(std::llround((T + 0.5) * kRate));
    blow.initialDisplacementMeters = 1.0e-5; blow.reedTurbulence = kSaxophoneReedTurbulence * noise; blow.reflectionCutoffHz = 1500.0;
    blow.lockedBreathFraction = kSaxophoneLockedBreathFraction; blow.spectrumBreakHz = kSaxophoneSpectrumBreakHz; blow.radiationHz = kSaxophoneRadiationHz;
    auto voiceR = ConeValveVoice::fromSaxophone(sax, blow, rate); if (!voiceR.ok()) return false; auto voice = std::move(voiceR).value();
    double start = 0, held = longest;
    for (auto& s : song) { const double L = s.hertz > 0 ? saxophoneSoundingLengthMeters(s.hertz) : held; push(notes, s, start, L, 0, 4200.0, 0, rel); if (s.hertz > 0) held = L; start += s.seconds; }
    applyErrors();
    out = playPhrase(voice, notes, T, false, 0.50, &airway, kRate, prof);
  } else if (r.instrument == I::Flute) {
    const double span = 7.0e-3, sigma = 0.45, thickness = 1.0e-3, end = fluteEndCorrectionMeters();
    auto bore = [&](double f) { return fluteSoundingLengthMeters(speed, f, end); };
    double longest = 0, lowest = 1e9; for (auto& s : song) if (s.hertz > 0) { longest = std::max(longest, bore(s.hertz)); lowest = std::min(lowest, s.hertz); }
    FluteSpec spec{longest * 1.04 * scoopRoom, 2.8e-4, kAir, kTemp, 1.2e-5, thickness, span, sigma};
    auto flR = makeFlute(spec); if (!flR.ok()) return false; auto fl = std::move(flR).value();
    auto pressureFor = [&](double f) { const double v = 2.0 * span * f / sigma; return 0.5 * fl.airDensityKgPerM3 * v * v; };
    ValveBlow jet = sustained(pressureFor(lowest) * 0.5, kFluteReflectionCutoffHz, 960); jet.frames = static_cast<std::uint64_t>(std::llround(T * kRate));
    const FluteVoicing voicing = fluteVoicingFor("");
    jet.reflectionMagnitude = 0.985; jet = fluteBoreLosses(jet); jet.wallFitFundamental = kFluteWallFitFundamental; jet.initialDisplacementMeters = 0.0;
    jet.jetAimMeters = voicing.aimFraction * thickness; jet.reflectionCutoffHz = voicing.reflectionCutoffHz;
    jet.jetNoiseMeters = 0.035 * thickness * noise; jet.reedTurbulence = kFluteJetTurbulence * noise;
    jet.lockedBreathFraction = voicing.lockedBreathFraction; jet.endCorrectionMeters = end;
    auto voiceR = BoreValveVoice::fromFlute(fl, jet, rate); if (!voiceR.ok()) return false; auto voice = std::move(voiceR).value();
    double start = 0, hL = longest, hJ = 0, hP = pressureFor(lowest);
    for (auto& s : song) {
      const double p = s.hertz > 0 ? pressureFor(s.hertz) : 0.0, L = s.hertz > 0 ? bore(s.hertz) : hL;
      double j = hJ;
      if (s.hertz > 0) { auto ct = fluteConvectionTime(fl, p); if (!ct.ok()) return false; j = kRate * ct.value(); }
      push(notes, s, start, L, 0, p > 0 ? p : hP, j);
      if (s.hertz > 0) { hL = L; hJ = j; hP = p; }
      start += s.seconds;
    }
    applyErrors();
    out = playPhrase(voice, notes, T, true, 0.50, &airway, kRate, prof);
  } else if (r.instrument == I::Trumpet) {
    TrumpetSpec tpSpec = phraseTrumpetSpec(speed, kAir, kTemp, kPhraseTrumpetLengthMeters * brassRoom);
    tpSpec.lipMassKg *= r.extras.lipMassScale; tpSpec.lipStiffnessNewtonsPerMeter *= r.extras.lipMassScale;  // wp1 WP_MASS
    auto tpR = makeTrumpet(tpSpec); if (!tpR.ok()) return false; auto tp = std::move(tpR).value();
    ValveBlow lip = sustained(2800.0, 10000.0, 1920); lip.frames = static_cast<std::uint64_t>(std::llround(T * kRate));
    lip.reflectionMagnitude = 0.999; lip.initialDisplacementMeters = 0.0; lip.tongueReleaseFrames = 0;
    lip.brassiness = 1.0; lip.reedTurbulence = kTrumpetReedTurbulence * noise; applyBach37Bell(lip); lip.bellTransmission = true; lip.radiationHz = kTrumpetRadiationHz;
    auto voiceR = ConeValveVoice::fromTrumpet(tp, 0.26, lip, rate); if (!voiceR.ok()) return false; auto voice = std::move(voiceR).value();
    double start = 0; BrassFinger held = kTrumpetFingers[0];
    for (auto& s : song) {
      const BrassFinger f = s.hertz > 0 ? trumpetFinger(s.hertz) : held;
      const double lipHz = f.mode == 0 ? 0.75 * speed / f.length : trumpetLipRatio(f) * conePartial(f.length, 0.26, f.mode);
      push(notes, s, start, f.length, lipHz, s.hertz > 0 ? trumpetMouthPressurePa(f) : 0.0, 0);
      notes.back().peakHz = impedancePeakHz(f.length, 0.26, f.mode, f.hertz);
      if (s.hertz > 0) held = f;
      start += s.seconds;
    }
    applyErrors();
    out = playPhrase(voice, notes, T, false, 0.50, &airway, kRate, prof);
  } else {  // trombone (wp1's default path)
    double longest = kPhraseTromboneLengthMeters;
    for (auto& s : song) if (s.hertz > 0) longest = std::max(longest, tromboneFinger(s.hertz).length * 1.04);
    auto boneSpec = phraseTromboneSpec(speed, kAir, kTemp, longest * brassRoom);
    boneSpec.lipMassKg *= r.extras.lipMassScale; boneSpec.lipStiffnessNewtonsPerMeter *= r.extras.lipMassScale;
    auto hornR = makeTrombone(boneSpec); if (!hornR.ok()) return false; auto horn = std::move(hornR).value();
    ValveBlow bone = sustained(1600.0, 5000.0, 6144); bone.frames = static_cast<std::uint64_t>(std::llround(T * kRate));
    bone.reflectionMagnitude = 0.999; bone.initialDisplacementMeters = 0.0; bone.tongueReleaseFrames = 0;
    bone.brassiness = 1.0; bone.reedTurbulence = kTromboneReedTurbulence * noise; applyScottBell(bone); bone.bellTransmission = false;
    bone.wallRadiusMeters = kBach42BoreRadiusMeters;
    auto voiceR = ConeValveVoice::fromTrombone(horn, 0.60, bone, rate); if (!voiceR.ok()) return false; auto voice = std::move(voiceR).value();
    double start = 0; BrassFinger held = kTromboneFingers[0];
    for (auto& s : song) {
      const BrassFinger f = s.hertz > 0 ? tromboneFinger(s.hertz) : held;
      push(notes, s, start, f.length, 0.80 * conePartial(f.length, 0.60, f.mode), 1600.0, 0);
      notes.back().peakHz = impedancePeakHz(f.length, 0.60, f.mode, f.hertz);
      if (s.hertz > 0) held = f;
      start += s.seconds;
    }
    applyErrors();
    out = playPhrase(voice, notes, T, false, 0.50, &airway, kRate, prof);
  }
  if (out.empty()) return false;
  // The phrase player starts with the lead-in rest; drop it so every
  // instrument starts at the same moment.
  const std::size_t lead = static_cast<std::size_t>(std::max(0.0, song.front().seconds - 0.25) * kRate);
  for (std::size_t i = 0; i < mix.size() && i + lead < out.size(); ++i) mix[i] += out[i + lead];
  return true;
}
}  // namespace detail

struct Audition {
  std::vector<double> mono;  // kRate, the voice's own units
  bool ok = false;
  std::string error;
};

// Plays the phrase with this player. Runs for a few seconds of CPU: call
// it from a worker thread. `cancel` (optional) stops it between notes and
// `progress` (optional) is set to 0..1 as notes finish.
[[nodiscard]] inline Audition renderAudition(const PlayerRig& r, Phrase p, const std::atomic<bool>* cancel = nullptr,
                                             std::atomic<float>* progress = nullptr) {
  using I = PlayerInstrument;
  Audition a;
  const double total = phraseSeconds(r.instrument, p);
  a.mono.assign(static_cast<std::size_t>(total * kRate), 0.0);
  const std::vector<PartNote> w = writtenPhrase(r.instrument, p);
  bool ok = false;
  switch (r.family()) {
    case PlayerFamily::Bowed: case PlayerFamily::Cello: ok = detail::renderBowed(r, w, a.mono, cancel, progress); break;
    case PlayerFamily::Plucked:
      ok = r.instrument == I::ClassicalGuitar ? detail::renderGuitar(r, w, a.mono, total, cancel, progress)
           : r.instrument == I::DoubleBassPizz ? detail::renderBassPizz(r, w, a.mono, cancel, progress)
                                               : detail::renderPlucked(r, w, a.mono, cancel, progress);
      break;
    case PlayerFamily::Struck:
      ok = r.instrument == I::Piano ? detail::renderPiano(r, w, a.mono, cancel, progress) : detail::renderStruck(r, w, a.mono, cancel, progress);
      break;
    case PlayerFamily::Kit: ok = detail::renderKit(r, detail::kitPart(p), a.mono, cancel, progress); break;
    case PlayerFamily::Shaken: ok = detail::renderSleigh(r, p, a.mono); break;
    case PlayerFamily::Wind: ok = detail::renderWind(r, w, p == Phrase::Scale && !isBrass(r.instrument), a.mono); break;  // brass scales stay tongued
  }
  if (progress) progress->store(1.0f);
  for (double x : a.mono) if (!std::isfinite(x)) { ok = false; a.error = "the model went unstable"; break; }
  if (!ok && a.error.empty()) a.error = detail::cancelled(cancel) ? "cancelled" : "this setting could not be played";
  a.ok = ok;
  return a;
}

[[nodiscard]] inline double peakOf(const std::vector<double>& x) {
  double p = 0; for (double v : x) p = std::max(p, std::fabs(v)); return p;
}
[[nodiscard]] inline double rmsOf(const std::vector<double>& x) {
  double s = 0; for (double v : x) s += v * v; return x.empty() ? 0.0 : std::sqrt(s / double(x.size()));
}

}  // namespace app::audition
