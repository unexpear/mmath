// Step 24.2d: player fatigue. The embouchure muscles tire on long, loud,
// high playing and recover in rests (phrase_player.h: the three-
// compartment fatigue model of Xia & Frey-Law 2008 with the rest
// multiplier of Looft et al. 2018). The tired share u moves physical
// controls only: brass lip resonance sags and its damping and buzz noise
// rise; a reed is pressed less (a longer equivalent reed, the pitch sags)
// and the breath support weakens; the flute's lip cover and jet aim drift
// and its edge noise rises. These tests check that fatigue is off for
// every named player and an off render is bit-identical; that the state
// rises under load and falls by about 1 - exp(-20 / 52) in a 20 s rest;
// and that on a three-minute high passage each wind's YIN pitch drifts
// flat steadily, its level sags and its tone changes, and that a 20 s
// rest brings part of the pitch back.
#include <catch2/catch_test_macros.hpp>

#include "../app/player_audition.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

using namespace mmathen;
namespace au = app::audition;

namespace {
// Frame pitch by YIN (A. de Cheveigne & H. Kawahara, JASA 111, 1917-1930,
// 2002): cumulative-mean-normalised difference, first dip under 0.15,
// refined by parabolic interpolation (their step 5). `ap` = the dip's
// depth, YIN's aperiodicity (0 = perfectly periodic). 0 Hz = no pitch.
double yinHz(const std::vector<double>& x, std::size_t a, double fs, double fLo, double fHi, double* ap) {
  constexpr std::size_t n = 1024;
  const auto tMin = static_cast<std::size_t>(fs / fHi), tMax = static_cast<std::size_t>(fs / fLo);
  *ap = 1.0;
  if (tMin < 2 || a + n + tMax + 2 > x.size()) return 0.0;
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
    if (m > 0.5) { *ap = m; return 0.0; }
  }
  *ap = dn[best];
  double tau = static_cast<double>(best);
  const double a0 = dn[best - 1], a1 = dn[best], a2 = dn[best + 1], den = a0 - 2.0 * a1 + a2;
  if (den > 0.0) tau += 0.5 * (a0 - a2) / den;
  return fs / tau;
}

PlayerInstrument instrumentOf(const char* key) {
  PlayerInstrument i{};
  REQUIRE(playerInstrumentFromKey(key, i));
  return i;
}

// One pass of the tune: median pitch (cents from the written notes),
// level (dB of the mean square), median aperiodicity and brightness
// (rms of the first difference over rms) over the notes of 0.3 s and up.
struct Pass {
  double cents = 0.0, db = 0.0, ap = 0.0, bright = 0.0;
};

// The tune line, a fourth up (shift +5: the high register), played `reps`
// times 0.5 s apart, a rest of restSec, then 2 more passes (about three
// minutes at 28 passes). keepGaps plays the gaps as rests.
std::vector<Pass> longPassage(PlayerRig r, int reps, double restSec, int shift = 5) {
  const auto line = au::writtenPhrase(r.instrument, au::Phrase::Tune, 0.0);
  const double lineSec = line.back().startSec + line.back().durSec;
  std::vector<PartNote> w;
  std::vector<int> passOf;
  double t = 0.25;
  for (int k = 0; k < reps + 2; ++k) {
    if (k == reps) t += restSec;
    for (PartNote n : line) {
      n.startSec += t;
      n.midi += shift;
      w.push_back(n);
      passOf.push_back(k);
    }
    t += lineSec + 0.5;
  }
  std::vector<double> mix(static_cast<std::size_t>((t + 1.5) * au::kRate), 0.0);
  REQUIRE(au::detail::renderWind(r, w, false, mix, nullptr, nullptr, nullptr, true));
  const double fs = au::kRate;
  std::vector<Pass> out;
  for (int k = 0; k < reps + 2; ++k) {
    std::vector<double> cents, aps;
    double pw = 0.0, dpw = 0.0;
    for (std::size_t i = 0; i < w.size(); ++i) {
      if (passOf[i] != k || w[i].durSec < 0.3) continue;
      const double f = 440.0 * std::pow(2.0, (w[i].midi - 69) / 12.0);
      for (double s = w[i].startSec + 0.08; s < w[i].startSec + w[i].durSec - 0.08; s += 0.02) {
        const auto a = static_cast<std::size_t>(s * fs);
        double ap = 1.0;
        const double hz = yinHz(mix, a, fs, f / 2.2, std::min(4000.0, f * 3.3), &ap);
        if (hz > 0.0) cents.push_back(1200.0 * std::log2(hz / f));
        aps.push_back(ap);
        for (std::size_t j = 0; j < 1024; ++j) {
          pw += mix[a + j] * mix[a + j];
          const double d = mix[a + j + 1] - mix[a + j];
          dpw += d * d;
        }
      }
    }
    REQUIRE(cents.size() > 20);
    std::sort(cents.begin(), cents.end());
    std::sort(aps.begin(), aps.end());
    Pass p;
    p.cents = cents[cents.size() / 2];
    p.ap = aps[aps.size() / 2];
    p.db = 10.0 * std::log10(pw / static_cast<double>(aps.size() * 1024));
    p.bright = std::sqrt(dpw / pw);
    out.push_back(p);
  }
  return out;
}

template <class F>
double meanOf(const std::vector<Pass>& p, std::size_t a, std::size_t b, F get) {
  double s = 0.0;
  for (std::size_t k = a; k <= b; ++k) s += get(p[k]);
  return s / static_cast<double>(b - a + 1);
}

std::vector<double> render(const PlayerRig& r, const std::vector<PartNote>& w, bool keepGaps) {
  const double end = w.back().startSec + w.back().durSec;
  std::vector<double> mix(static_cast<std::size_t>((end + 1.5) * au::kRate), 0.0);
  REQUIRE(au::detail::renderWind(r, w, false, mix, nullptr, nullptr, nullptr, keepGaps));
  return mix;
}

PhraseNote held(double start, double hz, const char* dynamic, bool rest = false) {
  PhraseNote n{"n"};
  n.startSec = start;
  n.targetHz = rest ? 0.0 : hz;
  n.rest = rest;
  n.dynamic = dynamic;
  return n;
}
}  // namespace

TEST_CASE("player fatigue: off for every named player, and an off render is bit-identical", "[player_fatigue]") {
  for (const auto& info : playerInstruments()) {
    const PlayerRig r = builtInPlayer(info.id);
    INFO(info.label);
    CHECK(r.wind.fatigueRate == 0.0);
    CHECK(r.wind.fatigueRecovery == 1.0);
  }
  for (const char* key : {"trumpet", "trombone", "clarinet", "alto_sax", "flute"}) {
    INFO(key);
    PlayerRig r = builtInPlayer(instrumentOf(key));
    const auto w = au::writtenPhrase(r.instrument, au::Phrase::Tune, 0.0);
    const auto plain = render(r, w, false);
    // Recovery speed, the seed and the register reference alone do
    // nothing without tiredness.
    r.wind.fatigueRecovery = 3.0;
    r.wind.fatigueSeed = 99;
    r.wind.fatigueRefHz = 300.0;
    CHECK(render(r, w, false) == plain);
  }
}

TEST_CASE("player fatigue: the tired share rises under load and recovers in rests", "[player_fatigue]") {
  WindPlayerProfile p;
  CHECK(phraseFatigue({held(0.0, 440.0, "f")}, p, 48000, 48000.0, 64).empty());
  p.fatigueRate = 1.0;
  p.fatigueRefHz = 440.0;
  constexpr double fs = 48000.0;
  constexpr std::uint64_t hop = 64;
  // 120 s of forte at the reference pitch, a 20 s rest, 10 s more.
  const std::vector<PhraseNote> notes{held(0.0, 440.0, "f"), held(120.0, 0.0, "", true), held(140.0, 440.0, "f")};
  const auto total = static_cast<std::uint64_t>(150.0 * fs);
  const auto u = phraseFatigue(notes, p, total, fs, hop);
  REQUIRE(u.size() == (total + hop - 1) / hop);
  auto at = [&](double sec) { return u[static_cast<std::size_t>(sec * fs / hop)]; };
  CHECK(at(0.0) == 0.0);
  // Rises steadily while playing.
  for (double s = 10.0; s <= 120.0; s += 10.0) CHECK(at(s) > at(s - 10.0));
  CHECK(at(119.9) > 0.3);
  CHECK(at(119.9) < 0.6);
  // A 20 s rest recovers about 1 - exp(-R r 20 s), R r = 0.00064 x 30 / s.
  const double expected = 1.0 - std::exp(-kRecoveryRatePerSec * kRestRecoveryMultiplier * 20.0);
  const double recovered = 1.0 - at(139.9) / at(119.9);
  CHECK(std::fabs(recovered - expected) < 0.02);
  CHECK(at(149.9) > at(140.1));
  // Higher, louder playing tires faster; a quicker recoverer recovers more.
  WindPlayerProfile q = p;
  const auto soft = phraseFatigue({held(0.0, 220.0, "p")}, q, static_cast<std::uint64_t>(60.0 * fs), fs, hop);
  const auto loud = phraseFatigue({held(0.0, 880.0, "ff")}, q, static_cast<std::uint64_t>(60.0 * fs), fs, hop);
  CHECK(loud.back() > 2.0 * soft.back());
  q.fatigueRecovery = 3.0;
  const auto quick = phraseFatigue(notes, q, total, fs, hop);
  CHECK(quick[static_cast<std::size_t>(139.9 * fs / hop)] < at(139.9));
  // Twice the tiredness tires faster.
  q = p;
  q.fatigueRate = 2.0;
  CHECK(phraseFatigue(notes, q, total, fs, hop)[static_cast<std::size_t>(119.9 * fs / hop)] > at(119.9));
}

TEST_CASE("player fatigue: a tired wind drifts flat, sags and changes tone; a rest brings part back",
          "[player_fatigue]") {
  // Thresholds from the step 24.2d sweep at tiredness 1, 28 passes, 20 s
  // rest (runs/pb24): fresh = passes 0-1, tired = 25-27, rested = 28-29.
  struct Want {
    const char* key;
    double minDropCents, minSagDb, minApRatio, minBrightChange;
  };
  for (const Want& k : {Want{"trumpet", 15.0, 0.75, 1.8, 0.10}, Want{"trombone", 12.0, 0.0, 0.0, 0.0},
                        Want{"clarinet", 20.0, 2.0, 3.0, 0.0}, Want{"alto_sax", 15.0, 1.5, 2.0, 0.0},
                        Want{"flute", 12.0, 2.0, 0.0, 0.0}}) {
    INFO(k.key);
    PlayerRig r = builtInPlayer(instrumentOf(k.key));
    r.wind.fatigueRate = 1.0;
    const auto p = longPassage(r, 28, 20.0);
    REQUIRE(p.size() == 30);
    auto cents = [](const Pass& x) { return x.cents; };
    auto db = [](const Pass& x) { return x.db; };
    auto ap = [](const Pass& x) { return x.ap; };
    auto bright = [](const Pass& x) { return x.bright; };
    const double fresh = meanOf(p, 0, 1, cents), tired = meanOf(p, 25, 27, cents), rested = meanOf(p, 28, 29, cents);
    INFO("cents fresh " << fresh << " tired " << tired << " rested " << rested);
    INFO("dB fresh " << meanOf(p, 0, 1, db) << " tired " << meanOf(p, 25, 27, db));
    INFO("ap fresh " << meanOf(p, 0, 1, ap) << " tired " << meanOf(p, 25, 27, ap));
    INFO("bright fresh " << meanOf(p, 0, 1, bright) << " tired " << meanOf(p, 25, 27, bright));
    // Flat, and steadily: each block of three passes no sharper than the
    // one before, within 3 cents.
    CHECK(fresh - tired >= k.minDropCents);
    for (std::size_t b = 3; b + 2 < 27; b += 3) CHECK(meanOf(p, b, b + 2, cents) <= meanOf(p, b - 3, b - 1, cents) + 3.0);
    // Weaker.
    if (k.minSagDb > 0.0) CHECK(meanOf(p, 0, 1, db) - meanOf(p, 25, 27, db) >= k.minSagDb);
    // Noisier or a changed tone (the trombone and flute measured no
    // change at tiredness 1: see the 24.2d report).
    if (k.minApRatio > 0.0) CHECK(meanOf(p, 25, 27, ap) >= k.minApRatio * meanOf(p, 0, 1, ap));
    if (k.minBrightChange > 0.0)
      CHECK(std::fabs(meanOf(p, 25, 27, bright) / meanOf(p, 0, 1, bright) - 1.0) >= k.minBrightChange);
    // The rest brings part of the drift back, not more than all of it.
    CHECK(rested - tired >= 0.10 * (fresh - tired));
    CHECK(rested <= fresh + 3.0);
  }
}

TEST_CASE("player fatigue: a fresh player holds the long passage steady", "[player_fatigue]") {
  // The alto sax once stopped 142 s into this passage: a shut reed's
  // roundoff opening defeated the pressure search's bracket (cone_valve.h,
  // bracketBelow).
  for (const char* key : {"trumpet", "alto_sax"}) {
    INFO(key);
    PlayerRig r = builtInPlayer(instrumentOf(key));
    const auto p = longPassage(r, 28, 20.0);
    REQUIRE(p.size() == 30);
    for (const Pass& x : p) {
      CHECK(std::fabs(x.cents - p[0].cents) < 3.0);
      CHECK(std::fabs(x.db - p[0].db) < 1.0);
    }
  }
}
