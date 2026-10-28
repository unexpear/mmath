// Step 24.2c: fall-offs. A fall-off drops the end of a note below its
// pitch while the breath fades, made by the embouchure on the instrument
// (phrase_player.h, the scoop mechanisms in reverse). These tests check
// that fall-offs are off for every named player and leave an off render
// bit-identical, that slurs, rests, the chance and the phrase-end rule pick
// the falling notes, and that a YIN pitch track of the render holds the
// note, then falls about the requested depth, without climbing back, by the
// end of the requested time, as the level drops.
#include <catch2/catch_test_macros.hpp>

#include "../app/player_audition.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace mmathen;
namespace au = app::audition;

namespace {
// Frame pitch by YIN (A. de Cheveigne & H. Kawahara, JASA 111, 1917-1930,
// 2002): cumulative-mean-normalised difference, first dip under 0.15,
// searched between fLo and fHi. 0 = no pitch.
double yinHz(const std::vector<double>& x, std::size_t a, double fs, double fLo, double fHi) {
  constexpr std::size_t n = 1024;
  const auto tMin = static_cast<std::size_t>(fs / fHi), tMax = static_cast<std::size_t>(fs / fLo);
  if (tMin < 2 || a + n + tMax + 1 > x.size()) return 0.0;
  std::vector<double> dn(tMax + 2, 1.0);
  double run = 0.0;
  for (std::size_t t = 1; t <= tMax + 1; ++t) {
    double d = 0.0;
    for (std::size_t k = 0; k < n; ++k) {
      const double e = x[a + k] - x[a + k + t];
      d += e * e;
    }
    run += d;
    dn[t] = run > 0.0 ? d * static_cast<double>(t) / run : 1.0;
  }
  std::size_t best = 0;
  for (std::size_t t = tMin; t <= tMax; ++t)
    if (dn[t] < 0.15) {
      while (t + 1 <= tMax && dn[t + 1] < dn[t]) ++t;
      best = t;
      break;
    }
  if (best == 0) {
    double m = 1e300;
    for (std::size_t t = tMin; t <= tMax; ++t)
      if (dn[t] < m) { m = dn[t]; best = t; }
    if (m > 0.5) return 0.0;
  }
  return fs / static_cast<double>(best);
}

std::vector<PartNote> pairs(const std::vector<int>& midi, double dur) {
  std::vector<PartNote> w;
  double t = 0.25;
  for (int m : midi)
    for (int k = 0; k < 2; ++k) {
      PartNote n;
      n.startSec = t;
      n.durSec = dur;
      n.midi = m;
      w.push_back(n);
      t += dur;
    }
  return w;
}

std::vector<double> render(const PlayerRig& r, const std::vector<PartNote>& w, const std::vector<double>* fall) {
  const double end = w.back().startSec + w.back().durSec;
  std::vector<double> mix(static_cast<std::size_t>((end + 1.5) * au::kRate), 0.0);
  REQUIRE(au::detail::renderWind(r, w, false, mix, nullptr, nullptr, fall));
  return mix;
}

PlayerInstrument instrumentOf(const char* key) {
  PlayerInstrument i{};
  REQUIRE(playerInstrumentFromKey(key, i));
  return i;
}

// A note's pitch and level track: 5 ms hop, 1024-sample window, time =
// the window's centre after the note's start. Pitch in cents from the
// note's own pitch before the fall (the median over 0.1 s .. ref), NAN =
// no pitch; level in dB.
struct Track {
  std::vector<double> t, c, db;
  std::vector<double> pitched(double lo, double hi) const {
    std::vector<double> v;
    for (std::size_t k = 0; k < t.size(); ++k)
      if (t[k] >= lo && t[k] <= hi && !std::isnan(c[k])) v.push_back(c[k]);
    return v;
  }
  double level(double lo, double hi) const {
    double s = 0.0;
    int n = 0;
    for (std::size_t k = 0; k < t.size(); ++k)
      if (t[k] >= lo && t[k] <= hi) { s += db[k]; ++n; }
    return n ? s / n : -300.0;
  }
  // Largest rise between consecutive pitched frames in lo..hi.
  double maxRise(double lo, double hi) const {
    double m = 0.0, prev = NAN;
    for (std::size_t k = 0; k < t.size(); ++k) {
      if (t[k] < lo || t[k] > hi || std::isnan(c[k])) continue;
      if (!std::isnan(prev)) m = std::max(m, c[k] - prev);
      prev = c[k];
    }
    return m;
  }
  // First time at or below `cents`, or 1e9.
  double firstBelow(double cents, double from) const {
    for (std::size_t k = 0; k < t.size(); ++k)
      if (t[k] >= from && !std::isnan(c[k]) && c[k] <= cents) return t[k];
    return 1e9;
  }
};

Track trackOf(const std::vector<double>& mix, const PartNote& n, double ref) {
  const double fs = au::kRate, f = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0), half = 512.0 / fs;
  Track tr;
  for (double tc = 0.0; tc < n.durSec; tc += 0.005) {
    const double s = n.startSec + tc - half;
    const auto a = static_cast<std::size_t>(std::max(0.0, s) * fs);
    const double hz = yinHz(mix, a, fs, f / 4.5, std::min(4000.0, f * 3.3));
    double e = 0.0;
    for (std::size_t k = 0; k < 1024 && a + k < mix.size(); ++k) e += mix[a + k] * mix[a + k];
    tr.t.push_back(tc);
    tr.c.push_back(hz > 0.0 ? 1200.0 * std::log2(hz / f) : NAN);
    tr.db.push_back(10.0 * std::log10(e / 1024.0 + 1e-30));
  }
  auto pre = tr.pitched(0.1, ref);
  REQUIRE(pre.size() >= 10);
  std::sort(pre.begin(), pre.end());
  const double steady = pre[pre.size() / 2];
  for (double& x : tr.c) x -= steady;
  return tr;
}
}  // namespace

TEST_CASE("wind fall-offs: off for every named player, and an off render is bit-identical", "[wind_falloffs]") {
  for (const auto& info : playerInstruments()) {
    const PlayerRig r = builtInPlayer(info.id);
    INFO(info.label);
    CHECK(r.wind.fallCents == 0.0);
    CHECK(r.wind.fallSec == 0.0);
  }
  for (const char* key : {"trumpet", "trombone", "clarinet", "alto_sax", "flute"}) {
    INFO(key);
    const auto w = pairs({65}, 0.4);
    PlayerRig r = builtInPlayer(instrumentOf(key));
    const auto plain = render(r, w, nullptr);
    const std::vector<double> zeros(w.size(), 0.0);
    CHECK(render(r, w, &zeros) == plain);
    // Length, chance and the phrase rule alone do nothing without a depth.
    r.wind.fallSec = 0.5;
    r.wind.fallChance = 0.5;
    r.wind.fallPhraseEndsOnly = false;
    r.wind.fallSeed = 7;
    CHECK(render(r, w, nullptr) == plain);
  }
}

TEST_CASE("wind fall-offs: slurs, rests, the chance and phrase ends pick the falling notes", "[wind_falloffs]") {
  // Bars of four: two 1 s notes, a 2 s note, a 1 s note slurred into the
  // next bar's first note, then a rest every fourth bar.
  std::vector<PhraseNote> notes;
  double t = 0.0;
  for (int bar = 0; bar < 400; ++bar) {
    for (int k = 0; k < 4; ++k) {
      PhraseNote n{};
      n.startSec = t;
      n.tongue = !(k == 0 && bar > 0 && (bar - 1) % 4 != 3);  // slurred in from the last bar's note 3
      notes.push_back(n);
      t += k == 2 ? 2.0 : 1.0;
    }
    if (bar % 4 == 3) {
      PhraseNote r{};
      r.startSec = t;
      r.rest = true;
      notes.push_back(r);
      t += 1.0;
    }
  }
  WindPlayerProfile p;
  for (double d : phraseFallDepths(notes, p)) CHECK(d == 0.0);  // off: no depth
  p.fallCents = 300.0;
  p.fallPhraseEndsOnly = false;
  const auto all = phraseFallDepths(notes, p);
  int eligible = 0;
  for (std::size_t i = 0; i < notes.size(); ++i) {
    const bool slurOn = i + 1 < notes.size() && !notes[i + 1].rest && !notes[i + 1].tongue;
    if (notes[i].rest || slurOn) CHECK(all[i] == 0.0);
    else { CHECK(all[i] == 300.0); ++eligible; }
  }
  // Phrase ends only: the held 2 s notes and the notes before a rest.
  p.fallPhraseEndsOnly = true;
  const auto ends = phraseFallDepths(notes, p);
  for (std::size_t i = 0; i < notes.size(); ++i) {
    if (notes[i].rest) continue;
    const double len = (i + 1 < notes.size() ? notes[i + 1].startSec : t) - notes[i].startSec;
    const bool beforeRest = i + 1 >= notes.size() || notes[i + 1].rest;
    const bool slurOn = i + 1 < notes.size() && !notes[i + 1].rest && !notes[i + 1].tongue;
    INFO("note " << i);
    CHECK((ends[i] > 0.0) == (!slurOn && (len > 1.5 || beforeRest)));
  }
  // The chance: about that share of the eligible notes.
  p.fallPhraseEndsOnly = false;
  p.fallChance = 0.3;
  int fell = 0;
  for (double d : phraseFallDepths(notes, p)) fell += d > 0.0;
  INFO(fell << " of " << eligible);
  CHECK(std::fabs(static_cast<double>(fell) / eligible - 0.3) < 0.05);
  // Same seed, same notes; another seed, others.
  const auto a = phraseFallDepths(notes, p);
  CHECK(phraseFallDepths(notes, p) == a);
  p.fallSeed = 99;
  CHECK(phraseFallDepths(notes, p) != a);
  // A forced depth falls whatever the chance, but never into a slur.
  p.fallCents = 0.0;
  notes[0].fallCents = 120.0;
  notes[3].fallCents = 120.0;  // slurred into bar 1's first note
  const auto forced = phraseFallDepths(notes, p);
  CHECK(forced[0] == 120.0);
  CHECK(forced[3] == 0.0);
}

TEST_CASE("wind fall-offs: the pitch holds, falls about the depth by the end, and fades", "[wind_falloffs]") {
  // 300 cents over 0.3 s on the second note of each pair (0.8 s notes; the
  // fall ends at the next note's 40 ms tongue closure). The reference is
  // the same note rendered without the fall.
  constexpr double kDepth = 300.0, kTime = 0.3, kDur = 0.8;
  const double fallEnd = kDur - 0.040, fallStart = fallEnd - kTime;
  struct Case { const char* key; std::vector<int> midi; };
  const std::vector<Case> cases{{"trumpet", {65, 72}}, {"trombone", {60, 65}}, {"clarinet", {65, 72}},
                                {"alto_sax", {64, 72}}, {"flute", {72, 79}}};
  for (const Case& k : cases) {
    const auto w = pairs(k.midi, kDur);
    std::vector<double> fall(w.size(), 0.0);
    for (std::size_t i = 1; i < w.size(); i += 2) fall[i] = kDepth;
    PlayerRig r = builtInPlayer(instrumentOf(k.key));
    r.wind.fallSec = kTime;
    const auto mix = render(r, w, &fall);
    const auto plain = render(r, w, nullptr);
    for (std::size_t i = 1; i < w.size(); i += 2) {
      const Track tr = trackOf(mix, w[i], fallStart - 0.03), clean = trackOf(plain, w[i], fallStart - 0.03);
      auto before = tr.pitched(0.15, fallStart - 0.03);
      for (double& x : before) x = std::fabs(x);
      std::sort(before.begin(), before.end());
      // The window ends half a YIN window before the tongue closure: a
      // brass bore rings low once the lips seal.
      const auto tail = tr.pitched(fallEnd - 0.06, fallEnd - 0.011);
      const double deepest = tail.empty() ? 0.0 : *std::min_element(tail.begin(), tail.end());
      const auto cleanTail = clean.pitched(fallEnd - 0.06, fallEnd - 0.011);
      const double cleanDeepest = cleanTail.empty() ? 0.0 : *std::min_element(cleanTail.begin(), cleanTail.end());
      const double leaves = tr.firstBelow(-25.0, 0.15), reached = tr.firstBelow(0.8 * deepest, 0.15);
      const double fade = tr.level(fallEnd - 0.04, fallEnd) - tr.level(0.15, fallStart - 0.03);
      INFO(k.key << " m" << w[i].midi << ": before (90th pct) " << before[before.size() * 9 / 10] << " c, deepest "
                 << deepest << " c (clean " << cleanDeepest << "), leaves at " << leaves * 1000.0 << " ms, 80% at "
                 << reached * 1000.0 << " ms (fall " << fallStart * 1000.0 << ".." << fallEnd * 1000.0 << "), max rise "
                 << tr.maxRise(fallStart, fallEnd) << " c, tail frames " << tail.size() << ", level " << fade << " dB");
      CHECK(before[before.size() * 9 / 10] <= 30.0);   // on the note before the fall
      CHECK(tail.size() >= 5);                         // still sounding at the end
      CHECK(deepest <= -0.6 * kDepth);                 // about the depth
      CHECK(deepest >= -1.6 * kDepth);
      CHECK(leaves >= fallStart - 0.03);               // not early
      CHECK(reached <= fallEnd + 0.015);               // in time
      CHECK(tr.maxRise(fallStart, fallEnd) <= 25.0);   // no climbing back
      CHECK(fade <= -1.0);                             // fades with the breath
      CHECK(cleanDeepest >= -30.0);                    // a clean note does not fall
    }
  }
}
