// Player profiles for the instruments added in step 13 (player_profile.h,
// bowed_note.h): violin, viola, arco double bass, harp, harpsichord,
// electric guitar and the drum kit. The default changes nothing, each
// named profile carries its cited values, and each habit is heard.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <vector>

#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/player_profile.h>
#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/violin.h>

using namespace mmathen;
using Catch::Approx;

namespace {
constexpr double kFs = 48000.0;
std::vector<double> renderOf(Result<OversampledBowedStringVoice, QuantityError> made, double seconds) {
  REQUIRE(made.ok());
  auto v = std::move(made).value();
  std::vector<double> x(std::size_t(seconds * kFs));
  REQUIRE(v.renderBlock(x.data(), FrameCount{x.size()}).ok());
  return x;
}
bool bitSame(const std::vector<double>& a, const std::vector<double>& b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
}
// Zero-crossing rate of the pitch track is too fragile on a bowed tone;
// the vibrato is read as the spread of the period between upward zero
// crossings of the low-passed force over the note's sustained part.
double periodSpreadCents(const std::vector<double>& x, double f0) {
  std::vector<double> y(x.size());
  const double a = std::exp(-2.0 * 3.141592653589793 * 2.0 * f0 / kFs);
  double s = 0, s2 = 0;
  for (std::size_t i = 0; i < x.size(); ++i) { s = a * s + (1 - a) * x[i]; s2 = a * s2 + (1 - a) * s; y[i] = s2; }
  std::vector<double> ups;
  for (std::size_t i = std::size_t(0.5 * kFs); i + 1 < y.size() - std::size_t(0.2 * kFs); ++i)
    if (y[i] <= 0.0 && y[i + 1] > 0.0) ups.push_back(i + y[i] / (y[i] - y[i + 1]));
  double lo = 1e9, hi = 0;
  for (std::size_t i = 1; i < ups.size(); ++i) { const double p = ups[i] - ups[i - 1]; lo = std::min(lo, p); hi = std::max(hi, p); }
  return 1200.0 * std::log2(hi / lo);
}
}  // namespace

TEST_CASE("makeBowedNote on the cello voicing is makeCelloNote bit for bit", "[player][profile][bowed]") {
  auto vc = cello();
  REQUIRE(vc.ok());
  const auto ref = renderOf(makeCelloNote(vc.value(), 130.81, 0.4, 0.5, SampleRateHz{kFs}), 0.6);
  CHECK(bitSame(ref, renderOf(makeBowedNote(vc.value(), 130.81, 0.4, 0.5, SampleRateHz{kFs}, celloBowedVoicing()), 0.6)));
  CHECK(bitSame(ref, renderOf(makeBowedNote(vc.value(), 130.81, 0.4, 0.5, SampleRateHz{kFs}, celloBowedVoicing(), true,
                                            BowedPlayerHabits{}), 0.6)));
  // The default habits are the voicing's own values: an explicit copy of them changes nothing.
  auto vn = violin();
  REQUIRE(vn.ok());
  const BowedVoicing violinV = violinBowedVoicing();
  BowedPlayerHabits same;
  same.vibratoHz = violinV.vibratoHz;
  same.vibratoCents = violinV.vibratoCents;
  same.vibratoOnsetSeconds = violinV.vibratoOnsetSeconds;
  CHECK(bitSame(renderOf(makeBowedNote(vn.value(), 440.0, 0.4, 1.0, SampleRateHz{kFs}, violinV), 0.6),
                renderOf(makeBowedNote(vn.value(), 440.0, 0.4, 1.0, SampleRateHz{kFs}, violinV, true, same), 0.6)));
}

TEST_CASE("the bowed profiles carry MacLeod's and the bass study's vibrato", "[player][profile][bowed]") {
  const BowedPlayerProfile def;
  CHECK(def.feel.isDefault());
  CHECK(def.strings.vibratoHz < 0.0);
  CHECK(def.strings.bowForceScale == 1.0);
  const auto vn = violinistPlayerProfile(), va = violistPlayerProfile(), vb = arcoBassistPlayerProfile();
  CHECK(vn.strings.vibratoHz == 5.71);
  CHECK(vn.strings.vibratoCents == 25.5);   // 51 c peak to peak
  CHECK(va.strings.vibratoHz == 5.49);
  CHECK(va.strings.vibratoCents == 20.5);   // 41 c peak to peak
  CHECK(vb.strings.vibratoHz == 5.17);
  CHECK(vb.strings.vibratoCents == 9.5);    // 19 c peak to peak
  // MacLeod: violists vibrate slower and narrower than violinists.
  CHECK(va.strings.vibratoHz < vn.strings.vibratoHz);
  CHECK(va.strings.vibratoCents < vn.strings.vibratoCents);
  for (const auto* p : {&vn, &va, &vb}) {
    CHECK(p->feel.timingSdMaxSec == player_detail::kEnsembleTimingSdMaxSec);
    CHECK(p->strings.bowForceScale == 1.0);
    CHECK(p->strings.contactPointScale == 1.0);
  }
}

TEST_CASE("the violist's habits are heard on a viola note", "[player][profile][bowed]") {
  auto va = viola();
  REQUIRE(va.ok());
  const BowedVoicing v = violaBowedVoicing();
  const double f = 293.66;  // D4, open D string
  const auto plain = renderOf(makeBowedNote(va.value(), f, 1.4, 1.0, SampleRateHz{kFs}, v), 1.6);
  const auto played = renderOf(makeBowedNote(va.value(), f, 1.4, 1.0, SampleRateHz{kFs}, v, true, violistPlayerProfile().strings), 1.6);
  CHECK_FALSE(bitSame(plain, played));
  // Wider vibrato (+/-20.5 c against the voicing's +/-20 c) is close; check
  // both carry vibrato of the right order rather than a ratio.
  const double sp = periodSpreadCents(plain, f), sq = periodSpreadCents(played, f);
  CHECK(sp > 10.0);
  CHECK(sq > 10.0);
  CHECK(sp < 150.0);
  CHECK(sq < 150.0);
}

TEST_CASE("the arco bass profile renders on the double bass", "[player][profile][bowed]") {
  auto vb = doubleBass();
  REQUIRE(vb.ok());
  const auto x = renderOf(makeBowedNote(vb.value(), 55.0, 0.8, 1.0, SampleRateHz{kFs}, doubleBassBowedVoicing(), true,
                                        arcoBassistPlayerProfile().strings), 1.0);
  double peak = 0;
  for (double s : x) { REQUIRE(std::isfinite(s)); peak = std::max(peak, std::fabs(s)); }
  CHECK(peak > 0.0);
}

TEST_CASE("the plucked profiles carry their cited pluck points", "[player][profile][plucked]") {
  const PluckedPlayerProfile def;
  CHECK(def.pluckFraction == 0.0);
  const auto hp = harpistPlayerProfile();
  CHECK(hp.pluckFraction > 1.0 / 3.0);   // Chadefaux 2012: between 1/3 and 2/5
  CHECK(hp.pluckFraction < 2.0 / 5.0);
  CHECK(hp.pluckFromBridgeM == 0.0);
  const auto eg = electricGuitaristPlayerProfile();
  CHECK(eg.pluckFromBridgeM >= 0.06);    // Zollner: 6-10 cm from the bridge
  CHECK(eg.pluckFromBridgeM <= 0.10);
  // The harpsichordist's plectrum fixes the loudness: timing only.
  const auto hc = harpsichordistPlayerProfile();
  CHECK(hc.feel.velocityCv == 0.0);
  CHECK(hc.feel.velocityScale == 1.0);
  CHECK(hc.feel.timingCv > 0.0);
  CHECK(hc.pluckFromBridgeM == 0.0);
  CHECK(hc.pluckFraction == 0.0);
  std::vector<PartNote> w;
  for (int i = 0; i < 32; ++i) w.push_back({0.5 + 0.25 * i, 0.2, 60 + i % 12, 80.0});
  const auto p = performNotes(w, hc.feel);
  for (std::size_t i = 0; i < w.size(); ++i) CHECK(p[i].velocity == 80.0);
  double moved = 0;
  for (std::size_t i = 0; i < w.size(); ++i) moved += std::fabs(p[i].startSec - w[i].startSec);
  CHECK(moved > 0.0);
}

TEST_CASE("the kit drummer leads with the snare and kick against the hi-hat", "[player][profile][kit]") {
  std::vector<KitNote> w;
  for (int i = 0; i < 64; ++i) {
    const double t = 0.5 + 0.3 * i;
    w.push_back({{t, 0.1, 42, 80.0}, DrumLimb::RightHand});
    if (i % 4 == 0) w.push_back({{t, 0.1, 36, 100.0}, DrumLimb::RightFoot});
    if (i % 4 == 2) w.push_back({{t, 0.1, 38, 100.0}, DrumLimb::LeftHand});
  }
  // Default: unchanged, bit for bit.
  const auto d = performDrumKit(w, DrumKitPlayerProfile{});
  for (std::size_t i = 0; i < w.size(); ++i) {
    CHECK(std::memcmp(&d[i].note.startSec, &w[i].note.startSec, sizeof(double)) == 0);
    CHECK(d[i].note.velocity == w[i].note.velocity);
  }
  const auto prof = drumKitPlayerProfile();
  CHECK(prof.limbOffsetSec[0] == 0.0);
  CHECK(prof.limbOffsetSec[1] == -0.010);
  CHECK(prof.limbOffsetSec[2] == -0.010);
  const auto p = performDrumKit(w, prof);
  // Notes struck together share the feel's deviation, so a kick or snare
  // note sits exactly its limb's lead before the hi-hat on the same beat.
  int pairs = 0;
  for (std::size_t i = 0; i + 1 < w.size(); ++i) {
    if (w[i].limb != DrumLimb::RightHand || w[i + 1].note.startSec != w[i].note.startSec) continue;
    const double lead = p[i + 1].note.startSec - p[i].note.startSec;
    CHECK(lead == Approx(-0.010).margin(1e-9));
    ++pairs;
  }
  CHECK(pairs == 32);
  // The hi-hat keeps the beat on average (Fujii: about 2 ms, taken as 0).
  double mean = 0; int n = 0;
  for (std::size_t i = 0; i < w.size(); ++i)
    if (w[i].limb == DrumLimb::RightHand) { mean += p[i].note.startSec - w[i].note.startSec; ++n; }
  CHECK(std::fabs(mean / n) < 0.006);
}
