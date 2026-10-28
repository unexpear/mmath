// Step 24.2b: scoops. A scoop starts a note below its pitch and slides up
// into it, made by the embouchure on the instrument (phrase_player.h). These
// tests check that scoops are off for every named player and leave an off
// render bit-identical, that the chance and slurs pick the scooped notes,
// and that a YIN pitch track of the render starts near the requested depth,
// reaches the note within the requested time and slides without steps.
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

std::vector<double> render(const PlayerRig& r, const std::vector<PartNote>& w, const std::vector<double>* scoop) {
  const double end = w.back().startSec + w.back().durSec;
  std::vector<double> mix(static_cast<std::size_t>((end + 1.5) * au::kRate), 0.0);
  REQUIRE(au::detail::renderWind(r, w, false, mix, nullptr, scoop));
  return mix;
}

PlayerInstrument instrumentOf(const char* key) {
  PlayerInstrument i{};
  REQUIRE(playerInstrumentFromKey(key, i));
  return i;
}

// A note's pitch track: 5 ms hop, 1024-sample window, time = the window's
// centre after the note's start, pitch in cents from the note's own steady
// pitch (the median of its last 40%). NAN = no pitch.
struct Track {
  std::vector<double> t, c;
  double at(double lo, double hi, bool deepest) const {
    double v = deepest ? 1e9 : 0.0;
    int n = 0;
    for (std::size_t k = 0; k < t.size(); ++k)
      if (t[k] >= lo && t[k] <= hi && !std::isnan(c[k])) {
        v = deepest ? std::min(v, c[k]) : v + c[k];
        ++n;
      }
    return n == 0 ? NAN : deepest ? v : v / n;
  }
  // First time from which 4 pitched frames in a row are within 15 cents.
  double reach() const {
    for (std::size_t k = 0; k + 3 < t.size(); ++k) {
      bool ok = true;
      for (std::size_t j = k; j < k + 4; ++j) ok = ok && !std::isnan(c[j]) && std::fabs(c[j]) <= 15.0;
      if (ok) return t[k];
    }
    return 1e9;
  }
  // Largest frame-to-frame step between lo and hi.
  double maxStep(double lo, double hi) const {
    double m = 0.0, prev = NAN;
    for (std::size_t k = 0; k < t.size(); ++k) {
      if (t[k] < lo || t[k] > hi || std::isnan(c[k])) continue;
      if (!std::isnan(prev)) m = std::max(m, std::fabs(c[k] - prev));
      prev = c[k];
    }
    return m;
  }
};

Track trackOf(const std::vector<double>& mix, const PartNote& n) {
  const double fs = au::kRate, f = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0), half = 512.0 / fs;
  Track tr;
  for (double tc = 0.0; tc < n.durSec - 0.04; tc += 0.005) {
    const double s = n.startSec + tc - half;
    const double hz = s < 0.0 ? 0.0 : yinHz(mix, static_cast<std::size_t>(s * fs), fs, f / 2.2, std::min(4000.0, f * 3.3));
    tr.t.push_back(tc);
    tr.c.push_back(hz > 0.0 ? 1200.0 * std::log2(hz / f) : NAN);
  }
  std::vector<double> tail;
  for (std::size_t k = tr.c.size() * 6 / 10; k < tr.c.size(); ++k)
    if (!std::isnan(tr.c[k])) tail.push_back(tr.c[k]);
  REQUIRE(!tail.empty());
  std::sort(tail.begin(), tail.end());
  const double steady = tail[tail.size() / 2];
  for (double& x : tr.c) x -= steady;
  return tr;
}
}  // namespace

TEST_CASE("wind scoops: off for every named player, and an off render is bit-identical", "[wind_scoops]") {
  for (const auto& info : playerInstruments()) {
    const PlayerRig r = builtInPlayer(info.id);
    INFO(info.label);
    CHECK(r.wind.scoopCents == 0.0);
    CHECK(r.wind.scoopSec == 0.0);
  }
  for (const char* key : {"trumpet", "trombone", "clarinet", "alto_sax", "flute"}) {
    INFO(key);
    const auto w = pairs({65}, 0.4);
    PlayerRig r = builtInPlayer(instrumentOf(key));
    const auto plain = render(r, w, nullptr);
    const std::vector<double> zeros(w.size(), 0.0);
    CHECK(render(r, w, &zeros) == plain);
    // Time and chance alone do nothing without a depth.
    r.wind.scoopSec = 0.3;
    r.wind.scoopChance = 0.5;
    r.wind.scoopSeed = 7;
    CHECK(render(r, w, nullptr) == plain);
  }
}

TEST_CASE("wind scoops: the chance picks tongued notes, slurs and rests never scoop", "[wind_scoops]") {
  WindPlayerProfile p;
  std::vector<PhraseNote> notes(4000);
  for (std::size_t i = 0; i < notes.size(); ++i) {
    notes[i].tongue = i % 4 != 1;  // every 4th note slurred
    notes[i].rest = i % 4 == 3;
  }
  // Off: no depth.
  for (double d : phraseScoopDepths(notes, p)) CHECK(d == 0.0);
  p.scoopCents = 80.0;
  p.scoopChance = 0.3;
  const auto d = phraseScoopDepths(notes, p);
  int tongued = 0, scooped = 0;
  for (std::size_t i = 0; i < notes.size(); ++i) {
    if (notes[i].rest || !notes[i].tongue) {
      CHECK(d[i] == 0.0);
      continue;
    }
    ++tongued;
    if (d[i] > 0.0) {
      CHECK(d[i] == 80.0);
      ++scooped;
    }
  }
  INFO(scooped << " of " << tongued);
  CHECK(std::fabs(static_cast<double>(scooped) / tongued - 0.3) < 0.04);
  p.scoopChance = 1.0;
  const auto all = phraseScoopDepths(notes, p);
  for (std::size_t i = 0; i < notes.size(); ++i)
    if (!notes[i].rest && notes[i].tongue) CHECK(all[i] == 80.0);
  // A note's own forced depth wins, even on a slur.
  p.scoopCents = 0.0;
  notes[1].scoopCents = 40.0;
  CHECK(phraseScoopDepths(notes, p)[1] == 40.0);
  // The same seed draws the same notes; another seed draws others.
  p.scoopCents = 80.0;
  p.scoopChance = 0.5;
  notes[1].scoopCents = 0.0;
  const auto a = phraseScoopDepths(notes, p);
  CHECK(phraseScoopDepths(notes, p) == a);
  p.scoopSeed = 99;
  CHECK(phraseScoopDepths(notes, p) != a);
}

TEST_CASE("wind scoops: the pitch starts low, reaches the note in time and slides without steps", "[wind_scoops]") {
  // 100 cents over 0.15 s on the second note of each pair; the reference
  // is the same note rendered without the scoop. TUNED gains (phrase_player.h) put the deepest pitch in
  // the first 40 ms at about 70-140 cents.
  constexpr double kDepth = 100.0, kTime = 0.15;
  struct Case { const char* key; std::vector<int> midi; };
  const std::vector<Case> cases{{"trumpet", {65, 72}}, {"trombone", {65}}, {"clarinet", {64, 72}},
                                {"alto_sax", {64, 72}}, {"flute", {72, 79}}};
  for (const Case& k : cases) {
    const auto w = pairs(k.midi, 0.6);
    std::vector<double> scoop(w.size(), 0.0);
    for (std::size_t i = 1; i < w.size(); i += 2) scoop[i] = kDepth;
    PlayerRig r = builtInPlayer(instrumentOf(k.key));
    r.wind.scoopSec = kTime;
    const auto mix = render(r, w, &scoop);
    const auto plain = render(r, w, nullptr);  // the same notes unscooped
    for (std::size_t i = 0; i < w.size(); i += 2) {
      const Track clean = trackOf(plain, w[i + 1]), scooped = trackOf(mix, w[i + 1]);
      const double deepest = scooped.at(0.0, 0.040, true);
      const double early = scooped.at(0.030, 0.100, false), earlyClean = clean.at(0.030, 0.100, false);
      INFO(k.key << " m" << w[i].midi << ": deepest " << deepest << " c, reach " << scooped.reach() * 1000.0
                 << " ms, 30-100 ms mean " << early << " c (clean " << earlyClean << " c), max step "
                 << scooped.maxStep(0.025, kTime));
      CHECK(deepest <= -0.5 * kDepth);
      CHECK(deepest >= -1.8 * kDepth);
      CHECK(scooped.reach() <= kTime + 0.015);
      CHECK(early < earlyClean - 25.0);
      CHECK(scooped.maxStep(0.025, kTime) <= 40.0);
    }
  }
}
