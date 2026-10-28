// Step 24.2a: cracked / split notes. A crack is the lip, reed or jet
// captured for a moment by a neighbouring resonance of the bore, from an
// onset embouchure error (phrase_player.h). These tests check that the
// error is off for every named player, that the Mistakes control and soft
// attacks raise it, and that a forced error shows in the render as a brief
// jump to another partial on a pitch track, then a return.
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

// One written note's pitch track, 10 ms hop: per frame, the pitch over the
// written pitch (0 = no pitch).
struct Track {
  std::vector<double> ratio;
  // Longest run of frames more than 300 cents off (a neighbour partial,
  // not a pitch bend) that starts in the first 150 ms, in frames.
  int crackFrames() const {
    int best = 0, run = 0, start = 0;
    for (std::size_t i = 0; i < ratio.size(); ++i) {
      const bool wrong = ratio[i] > 0.0 && std::fabs(1200.0 * std::log2(ratio[i])) > 300.0;
      if (wrong) {
        if (run == 0) start = static_cast<int>(i);
        ++run;
        if (start <= 15) best = std::max(best, run);
      } else {
        run = 0;
      }
    }
    return best;
  }
  // Median ratio of the off frames (the regime it jumped to).
  double jumpRatio() const {
    std::vector<double> off;
    for (double r : ratio)
      if (r > 0.0 && std::fabs(1200.0 * std::log2(r)) > 300.0) off.push_back(r);
    if (off.empty()) return 1.0;
    std::nth_element(off.begin(), off.begin() + static_cast<std::ptrdiff_t>(off.size() / 2), off.end());
    return off[off.size() / 2];
  }
  // Of the pitched frames in the last third of the note, the fraction
  // back on the written partial (within the same 300 cents; the low
  // trombone has unpitched frames, and its clean track wanders up to
  // +270 cents on this tracker).
  double backOnPitch() const {
    const std::size_t from = ratio.size() * 2 / 3;
    int ok = 0, all = 0;
    for (std::size_t i = from; i < ratio.size(); ++i) {
      if (!(ratio[i] > 0.0)) continue;
      ++all;
      if (std::fabs(1200.0 * std::log2(ratio[i])) <= 300.0) ++ok;
    }
    return all >= 5 ? static_cast<double>(ok) / all : 0.0;
  }
};

// Plays notes (midi, 0.6 s each, tongued) through the audition's wind
// path with a forced onset error per note; returns each note's track.
std::vector<Track> playForced(PlayerInstrument inst, const std::vector<int>& midi, const std::vector<double>& errCents) {
  std::vector<PartNote> w;
  double t = 0.25;
  for (int m : midi) {
    PartNote n;
    n.startSec = t;
    n.durSec = 0.6;
    n.midi = m;
    w.push_back(n);
    t += 0.6;
  }
  const PlayerRig r = builtInPlayer(inst);
  std::vector<double> mix(static_cast<std::size_t>((t + 1.5) * au::kRate), 0.0);
  REQUIRE(au::detail::renderWind(r, w, false, mix, &errCents));
  std::vector<Track> out;
  for (const PartNote& n : w) {
    const double f = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0);
    Track tr;
    for (double s = n.startSec; s < n.startSec + n.durSec - 0.04; s += 0.01) {
      const double hz = yinHz(mix, static_cast<std::size_t>(s * au::kRate), au::kRate, f / 2.2, std::min(4000.0, f * 3.3));
      tr.ratio.push_back(hz > 0.0 ? hz / f : 0.0);
    }
    out.push_back(tr);
  }
  return out;
}

PlayerInstrument instrumentOf(const char* key) {
  PlayerInstrument i{};
  REQUIRE(playerInstrumentFromKey(key, i));
  return i;
}
}  // namespace

namespace {
// Cracks in an audition render: notes whose track jumps >= 5 frames.
int auditionCracks(PlayerInstrument inst, double sdCents, std::uint64_t seed) {
  PlayerRig r = builtInPlayer(inst);
  r.wind.crackSdCents = sdCents;
  r.wind.crackSeed = seed;
  const auto a = au::renderAudition(r, au::Phrase::Tune);
  REQUIRE(a.ok);
  int cracks = 0;
  for (const PartNote& n : au::writtenPhrase(inst, au::Phrase::Tune)) {
    const double f = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0);
    Track tr;
    for (double s = n.startSec; s < n.startSec + n.durSec - 0.04; s += 0.01) {
      const double hz = yinHz(a.mono, static_cast<std::size_t>(s * au::kRate), au::kRate, f / 2.2, std::min(4000.0, f * 3.3));
      tr.ratio.push_back(hz > 0.0 ? hz / f : 0.0);
    }
    if (tr.crackFrames() >= 5) ++cracks;
  }
  return cracks;
}
std::vector<PhraseNote> manyNotes(std::size_t n, const char* dynamic) {
  std::vector<PhraseNote> v(n);
  for (auto& x : v) x.dynamic = dynamic;
  return v;
}
double fractionOver(const std::vector<double>& e, double cents) {
  int k = 0;
  for (double x : e) if (std::fabs(x) > cents) ++k;
  return static_cast<double>(k) / static_cast<double>(e.size());
}
}  // namespace

TEST_CASE("wind cracks: off for every named player, and an off player renders as before", "[wind_cracks]") {
  for (const auto& info : playerInstruments()) {
    if (info.family != PlayerFamily::Wind) continue;
    const PlayerRig r = builtInPlayer(info.id);
    CHECK(r.wind.crackSdCents == 0.0);
  }
  const std::vector<PhraseNote> notes = manyNotes(50, "pp");
  for (double e : phraseOnsetErrors(notes, WindPlayerProfile{})) CHECK(e == 0.0);
  // Zero forced errors take the same path as none: the render is identical.
  for (const char* key : {"trumpet", "alto_sax", "flute"}) {
    const PlayerInstrument inst = instrumentOf(key);
    std::vector<PartNote> w(2);
    w[0].startSec = 0.25; w[0].durSec = 0.6; w[0].midi = key[0] == 'f' ? 72 : 65;
    w[1] = w[0]; w[1].startSec = 0.85;
    const PlayerRig r = builtInPlayer(inst);
    std::vector<double> a(static_cast<std::size_t>(3.0 * au::kRate), 0.0), b = a;
    const std::vector<double> zeros(2, 0.0);
    REQUIRE(au::detail::renderWind(r, w, false, a));
    REQUIRE(au::detail::renderWind(r, w, false, b, &zeros));
    CHECK(a == b);
  }
}

TEST_CASE("wind cracks: the Mistakes control and soft attacks raise the error", "[wind_cracks]") {
  const std::vector<PhraseNote> mf = manyNotes(4000, "mf"), pp = manyNotes(4000, "pp");
  WindPlayerProfile p;
  p.crackSdCents = 0.0;
  CHECK(fractionOver(phraseOnsetErrors(mf, p), 0.0) == 0.0);
  double last = 0.0;
  for (double sd : {100.0, 200.0, 400.0}) {
    p.crackSdCents = sd;
    const double f = fractionOver(phraseOnsetErrors(mf, p), 300.0);
    CHECK(f > last);
    last = f;
  }
  p.crackSdCents = 200.0;
  CHECK(fractionOver(phraseOnsetErrors(pp, p), 300.0) > 2.0 * fractionOver(phraseOnsetErrors(mf, p), 300.0));
  // Same seed, same draws: pp errors are (1 + kCrackSoftAttackGain) x mf.
  const auto em = phraseOnsetErrors(mf, p), ep = phraseOnsetErrors(pp, p);
  CHECK(std::fabs(ep[7] - (1.0 + kCrackSoftAttackGain) * em[7]) < 1e-9);
  // In the render: a trumpet playing the tune cracks more as the slider rises.
  const PlayerInstrument tp = instrumentOf("trumpet");
  const int off = auditionCracks(tp, 0.0, 1), mid = auditionCracks(tp, 150.0, 1), high = auditionCracks(tp, 400.0, 1);
  INFO("cracks at 0 / 150 / 400 c: " << off << " / " << mid << " / " << high);
  CHECK(off == 0);
  CHECK(high > mid);
}

TEST_CASE("wind cracks: high trumpet notes crack at errors the low notes ride out", "[wind_cracks]") {
  // The same onset errors on D4 and on D5 / F5. Partials crowd together
  // up high, so more of the errors reach a neighbour there.
  const PlayerInstrument tp = instrumentOf("trumpet");
  int low = 0, high = 0;
  for (double e : {250.0, -250.0, 350.0, -350.0, 450.0, -450.0}) {
    const auto tr = playForced(tp, {62, 62, 65, 65, 74, 74, 77, 77}, {0, e, 0, e, 0, e, 0, e});
    if (tr[1].crackFrames() >= 5) ++low;
    if (tr[5].crackFrames() >= 5) ++high;
    if (tr[7].crackFrames() >= 5) ++high;
  }
  INFO("cracks: D4 " << low << " of 6, D5 + F5 " << high << " of 12");
  CHECK(2 * low < high);
}

namespace {
// A forced crack: a jump of >= 50 ms to a regime near `ratio` that starts
// in the note's first 150 ms, with the note back on pitch for most of its
// last third; the unforced note of the same pitch does not jump.
void checkCrack(const Track& cracked, const Track& clean, double ratio, double tol) {
  INFO("crack frames " << cracked.crackFrames() << ", jumped to x" << cracked.jumpRatio() << ", back on pitch "
                       << cracked.backOnPitch() << ", clean frames " << clean.crackFrames());
  CHECK(cracked.crackFrames() >= 5);
  CHECK(cracked.crackFrames() <= 30);  // brief: 0.3 s at most
  CHECK(std::fabs(cracked.jumpRatio() - ratio) < tol);
  CHECK(cracked.backOnPitch() > 0.8);
  CHECK(clean.crackFrames() < 5);
}
}  // namespace

TEST_CASE("wind cracks: a forced error is a brief jump to another partial", "[wind_cracks]") {
  SECTION("trumpet: C5 drops to the partial below (3/4)") {
    const auto tr = playForced(instrumentOf("trumpet"), {65, 65, 72, 72}, {0, 0, 0, -400});
    checkCrack(tr[3], tr[2], 0.75, 0.05);
  }
  SECTION("trombone: G3 jumps to the partial above (5/4)") {
    const auto tr = playForced(instrumentOf("trombone"), {55, 55, 65, 65}, {0, 500, 0, 500});
    checkCrack(tr[1], tr[0], 1.25, 0.06);
  }
  SECTION("alto saxophone: a first note squawks up a twelfth (3x)") {
    const auto tr = playForced(instrumentOf("alto_sax"), {64, 64}, {1000, 0});
    checkCrack(tr[0], tr[1], 3.0, 0.1);
  }
  SECTION("clarinet: a first note squeaks high, the reed's own regime") {
    const auto tr = playForced(instrumentOf("clarinet"), {64, 64, 72, 72}, {1000, 0, 1000, 0});
    checkCrack(tr[0], tr[1], 2.6, 0.2);
  }
  SECTION("flute: an overblown jet jumps an octave") {
    const auto tr = playForced(instrumentOf("flute"), {72, 72, 79, 79}, {0, 1500, 0, 1500});
    checkCrack(tr[1], tr[0], 2.0, 0.05);
    checkCrack(tr[3], tr[2], 2.0, 0.05);
  }
}
