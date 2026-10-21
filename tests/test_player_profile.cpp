// Player profiles for the carol-band instruments (player_profile.h): the
// default profile changes nothing, each named profile carries its cited
// values, and the feel scatters onsets and velocities by the stated amounts.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <vector>

#include <mmathen/instruments/player_profile.h>

using namespace mmathen;
using Catch::Approx;

namespace {
constexpr double kFs = 48000.0;

std::vector<PartNote> isochronous(int n, double ioi, double vel) {
  std::vector<PartNote> out;
  for (int i = 0; i < n; ++i) out.push_back({1.0 + i * ioi, 0.8 * ioi, 60 + (i % 12), vel});
  return out;
}
struct Spread { double meanShift, sdShift, velMean, velCv; };
Spread spreadOf(const std::vector<PartNote>& a, const std::vector<PartNote>& b) {
  double m = 0, m2 = 0, v = 0, v2 = 0; const double n = double(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    const double d = b[i].startSec - a[i].startSec; m += d; m2 += d * d;
    v += b[i].velocity; v2 += b[i].velocity * b[i].velocity;
  }
  m /= n; v /= n;
  return {m, std::sqrt(std::max(0.0, m2 / n - m * m)), v, std::sqrt(std::max(0.0, v2 / n - v * v)) / v};
}
bool sameNotes(const std::vector<PartNote>& a, const std::vector<PartNote>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (std::memcmp(&a[i].startSec, &b[i].startSec, sizeof(double)) || std::memcmp(&a[i].velocity, &b[i].velocity, sizeof(double)) ||
        a[i].durSec != b[i].durSec || a[i].midi != b[i].midi) return false;
  return true;
}
std::vector<double> renderCello(const CelloPlayerHabits* h) {
  auto vc = cello();
  REQUIRE(vc.ok());
  auto note = h ? makeCelloNote(vc.value(), 130.81, 0.4, 0.5, SampleRateHz{kFs}, true, *h)
                : makeCelloNote(vc.value(), 130.81, 0.4, 0.5, SampleRateHz{kFs});
  REQUIRE(note.ok());
  auto v = std::move(note).value();
  std::vector<double> x(std::size_t(0.6 * kFs));
  REQUIRE(v.renderBlock(x.data(), FrameCount{x.size()}).ok());
  return x;
}
}  // namespace

TEST_CASE("a default player plays the written part bit for bit", "[player][profile]") {
  const auto w = isochronous(64, 0.3, 70.0);
  CHECK(sameNotes(w, performNotes(w, PlayerFeel{})));
  CHECK(sameNotes(w, performPiano(w, StruckPlayerProfile{})));
  CHECK(PlayerFeel{}.isDefault());
  // The default piano mapping is pianoHammerSpeed's rounded step.
  for (double v : {1.0, 38.0, 52.0, 58.0, 100.0, 127.0})
    CHECK(pianoSpeedFor(StruckPlayerProfile{}, v) == pianoHammerSpeed(int(std::lround(v * 5.0 / 127.0))));
  // Sleigh bells: the timeline comes back unchanged.
  const auto tl = parseSleighBellTimeline("shake 0.3 5.0 0.01 0.01 0.3\nstrike 0.86 1.6 0.3\nstrike 1.46 0.9 -0.3\n");
  const auto p = performSleighBells(tl, ShakenPlayerProfile{});
  REQUIRE(p.size() == tl.size());
  for (std::size_t i = 0; i < tl.size(); ++i) {
    CHECK(p[i].timeSec == tl[i].timeSec);
    CHECK(p[i].strength == tl[i].strength);
  }
}

TEST_CASE("default habits leave the cello and the bass voices byte-identical", "[player][profile][cello]") {
  const CelloPlayerHabits d{};
  const auto a = renderCello(nullptr), b = renderCello(&d);
  REQUIRE(a.size() == b.size());
  CHECK(std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0);
  // The cellist profile's vibrato changes the note.
  const CelloPlayerHabits cellist = cellistPlayerProfile().cello;
  const auto c = renderCello(&cellist);
  CHECK(std::memcmp(a.data(), c.data(), a.size() * sizeof(double)) != 0);

  auto bass = doubleBass();
  REQUIRE(bass.ok());
  auto note = doubleBassNote(bass.value(), 110.0);
  REQUIRE(note.ok());
  auto render = [&](bool explicitPoint) {
    auto vo = explicitPoint ? doubleBassPizzVoice(note.value(), 2e-3, SampleRateHz{kFs}, 1.0, kDoubleBassPizzStoppedT60At6F0,
                                                  bassistPlayerProfile().pluckFromBridgeM)
                            : doubleBassPizzVoice(note.value(), 2e-3, SampleRateHz{kFs});
    REQUIRE(vo.ok());
    auto v = std::move(vo).value();
    std::vector<double> x(4800);
    REQUIRE(v.renderBlock(x.data(), FrameCount{x.size()}).ok());
    return x;
  };
  const auto x0 = render(false), x1 = render(true);
  CHECK(std::memcmp(x0.data(), x1.data(), x0.size() * sizeof(double)) == 0);
}

TEST_CASE("each named profile carries its cited values", "[player][profile]") {
  const BowedPlayerProfile vc = cellistPlayerProfile();
  CHECK(vc.cello.vibratoHz == 5.5);       // Geringer & Allen 2004
  CHECK(vc.cello.vibratoCents == 11.0);   // ~22 c peak to peak
  CHECK(vc.cello.bowForceScale == 1.0);
  CHECK(vc.cello.contactPointScale == 1.0);
  CHECK(CelloPlayerHabits{}.vibratoHz == kCelloVibratoRateHz);
  CHECK(CelloPlayerHabits{}.vibratoCents == kCelloVibratoCents);

  const PluckedPlayerProfile gt = guitaristPlayerProfile(), bs = bassistPlayerProfile();
  CHECK(gt.pluckFromBridgeM == kClassicalGuitarRoundPluckFromBridgeM);  // Traube, 12-14 cm
  CHECK(gt.pluckFromBridgeM >= 0.12);
  CHECK(gt.pluckFromBridgeM <= 0.14);
  CHECK(bs.pluckFromBridgeM == kDoubleBassPizzFromBridgeM);

  const StruckPlayerProfile pn = pianistPlayerProfile(), sd = snareDrummerPlayerProfile(), hb = handbellRingerPlayerProfile();
  CHECK(pn.pianoKeyAction);
  CHECK(pn.feel.timingCv == 0.03);         // Goebl & Palmer 2008 lower bound
  CHECK(sd.feel.timingOffsetSec == -0.010); // Fujii et al. 2011
  CHECK(hb.feel.timingCv == 0.074);        // Hofmann et al. 2017
  for (const StruckPlayerProfile& p : {glockenspielPlayerProfile(), celestaPlayerProfile(), tubularBellsPlayerProfile()}) {
    CHECK(p.feel.timingCv == 0.03);
    CHECK_FALSE(p.pianoKeyAction);
  }
  CHECK(sleighBellPlayerProfile().shakeAmplitudeScale == 1.0);
  // Every named profile differs from the default.
  CHECK_FALSE(vc.feel.isDefault());
  CHECK_FALSE(gt.feel.isDefault());
  CHECK_FALSE(bs.feel.isDefault());
  CHECK_FALSE(pn.feel.isDefault());
  CHECK_FALSE(sd.feel.isDefault());
  CHECK_FALSE(sleighBellPlayerProfile().feel.isDefault());
}

TEST_CASE("the feel scatters onsets and velocities by the profile's amounts", "[player][profile]") {
  const auto w = isochronous(2000, 0.25, 64.0);
  for (const PlayerFeel& f : {snareDrummerPlayerProfile().feel, guitaristPlayerProfile().feel, handbellRingerPlayerProfile().feel}) {
    const Spread s = spreadOf(w, performNotes(w, f));
    INFO("timingCv " << f.timingCv << " offset " << f.timingOffsetSec);
    CHECK(s.meanShift == Approx(f.timingOffsetSec).margin(0.0015));
    CHECK(s.sdShift == Approx(f.timingCv * 0.25).epsilon(0.08));
    CHECK(s.velMean == Approx(64.0).epsilon(0.01));
    CHECK(s.velCv == Approx(f.velocityCv).epsilon(0.08));
  }
  // Same seed, same performance.
  const PlayerFeel f = snareDrummerPlayerProfile().feel;
  CHECK(sameNotes(performNotes(w, f), performNotes(w, f)));
}

TEST_CASE("a chord moves as one gesture", "[player][profile]") {
  std::vector<PartNote> w;
  for (int i = 0; i < 50; ++i)
    for (int m : {48, 55, 64}) w.push_back({0.5 + 0.6 * i, 0.5, m, 60.0});
  PlayerFeel f; f.timingCv = 0.05; f.seed = 9;
  const auto p = performNotes(w, f);
  for (std::size_t i = 0; i < p.size(); i += 3) {
    CHECK(p[i].startSec == p[i + 1].startSec);
    CHECK(p[i].startSec == p[i + 2].startSec);
  }
}

TEST_CASE("the piano action makes the louder voice lead", "[player][profile][piano]") {
  // Askenfelt & Jansson 1991 / Askenfelt 1990 via Goebl 2001: 148 ms at 1 m/s, 28 ms at 5 m/s.
  CHECK(pianoActionDelaySec(1.0) == Approx(0.148).epsilon(1e-9));
  CHECK(pianoActionDelaySec(5.0) == Approx(0.028).epsilon(1e-9));
  CHECK(pianoActionDelaySec(2.5) < pianoActionDelaySec(1.5));
  // A chord with an f top voice over p accompaniment: the top note sounds
  // first, by a few tens of ms (Goebl's ~30 ms melody lead).
  std::vector<PartNote> w{{1.0, 0.5, 72, 110.0}, {1.0, 0.5, 64, 50.0}, {1.0, 0.5, 60, 50.0}};
  StruckPlayerProfile p = pianistPlayerProfile();
  p.feel = PlayerFeel{};  // action only
  const auto out = performPiano(w, p);
  const double lead = out[1].startSec - out[0].startSec;
  CHECK(lead > 0.015);
  CHECK(lead < 0.060);
  // A part at one dynamic stays on the grid (the player times the mean stroke).
  std::vector<PartNote> even{{1.0, 0.5, 60, 50.0}, {1.5, 0.5, 62, 50.0}};
  CHECK(performPiano(even, p)[0].startSec == Approx(1.0).margin(1e-12));
  CHECK(performPiano(even, p)[1].startSec == Approx(1.5).margin(1e-12));
  // Continuous hammer speed agrees with the table at the steps.
  for (int k = 0; k <= 5; ++k) CHECK(pianoHammerSpeedContinuous(k * 127.0 / 5.0) == Approx(pianoHammerSpeed(k)));
}

TEST_CASE("the sleigh-bell player moves strikes and leaves the shake", "[player][profile][sleigh]") {
  std::string text = "shake 0.3 30.0 0.01 0.01 0.3\n";
  for (int i = 0; i < 48; ++i) text += "strike " + std::to_string(0.86 + 0.6 * i) + " 1.6 0.3\n";
  const auto tl = parseSleighBellTimeline(text);
  const auto p = performSleighBells(tl, sleighBellPlayerProfile());
  REQUIRE(p.size() == tl.size());
  CHECK(p[0].strength == tl[0].strength);
  int moved = 0;
  for (std::size_t i = 1; i < p.size(); ++i) {
    if (p[i].timeSec != tl[i].timeSec) ++moved;
    CHECK(std::abs(p[i].timeSec - tl[i].timeSec) < 0.1);
    CHECK(p[i].strength > 0.0);
  }
  CHECK(moved == 48);
}
