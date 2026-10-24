// Step 23: the player builder (player_builder.h) and its audition renderer
// (app/player_audition.h).
#include <catch2/catch_test_macros.hpp>

#include <mmathen/instruments/player_builder.h>
#include "../app/player_audition.h"

#include <cmath>
#include <filesystem>
#include <set>
#include <string>

using namespace mmathen;
namespace au = app::audition;

namespace {
// Instruments whose short phrase renders in well under a second: every
// control of these is checked by ear-free render comparison. The others
// (snare and kit: oversampled membranes; tubular bells, piano: long modal
// rings) are checked by plumbing below and by one render in the hidden
// [.player_builder_slow] case.
bool quick(PlayerInstrument i) {
  using I = PlayerInstrument;
  return !(i == I::SnareDrum || i == I::DrumKit || i == I::TubularBells || i == I::Piano);
}
double diffRms(const std::vector<double>& a, const std::vector<double>& b) {
  const std::size_t n = std::max(a.size(), b.size());
  double s = 0;
  for (std::size_t k = 0; k < n; ++k) {
    const double d = (k < a.size() ? a[k] : 0.0) - (k < b.size() ? b[k] : 0.0);
    s += d * d;
  }
  return n ? std::sqrt(s / double(n)) : 0.0;
}
// A non-default value for the field: its far end from the default.
double farValue(const PlayerField& f) {
  if (f.toggle) return f.defaultValue >= 0.5 ? 0.0 : 1.0;
  return (f.maxValue - f.defaultValue) >= (f.defaultValue - f.minValue) ? f.maxValue : f.minValue;
}
}  // namespace

TEST_CASE("player builder: every instrument with a profile has adjustable fields", "[player_builder]") {
  REQUIRE(playerInstruments().size() == static_cast<std::size_t>(kPlayerInstrumentCount));
  std::set<std::string> keys;
  for (const auto& info : playerInstruments()) {
    INFO(info.label);
    CHECK(static_cast<int>(info.id) == static_cast<int>(&info - &playerInstruments()[0]));
    CHECK(keys.insert(info.key).second);
    PlayerInstrument back{};
    CHECK(playerInstrumentFromKey(info.key, back));
    CHECK(back == info.id);
    const auto fields = playerFields(info.id);
    CHECK(fields.size() >= 1);
    std::set<std::string> fk;
    for (const PlayerField& f : fields) {
      INFO(f.key);
      CHECK(fk.insert(f.key).second);
      CHECK(std::string(f.label).size() > 0);
      CHECK(std::string(f.tooltip).size() > 0);
      CHECK(std::string(f.group).size() > 0);
      CHECK(std::string(f.citation).size() > 0);
      CHECK(f.minValue < f.maxValue);
      CHECK(f.defaultValue >= f.minValue);
      CHECK(f.defaultValue <= f.maxValue);
    }
    CHECK(std::string(builtInPlayerName(builtInPlayer(info.id))) != "default");
  }
}

TEST_CASE("player builder: default values rebuild the built-in profile", "[player_builder]") {
  for (const auto& info : playerInstruments()) {
    INFO(info.label);
    const PlayerRig a = builtInPlayer(info.id), b = makePlayer(info.id, defaultValues(info.id));
    CHECK(valuesOf(a) == valuesOf(b));
    CHECK(valuesOf(b) == defaultValues(info.id));
  }
  // Bit-identical render for a few families.
  for (PlayerInstrument i : {PlayerInstrument::Harp, PlayerInstrument::Violin, PlayerInstrument::Clarinet, PlayerInstrument::SleighBells}) {
    const auto a = au::renderAudition(builtInPlayer(i), au::Phrase::Short);
    const auto b = au::renderAudition(makePlayer(i, defaultValues(i)), au::Phrase::Short);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    CHECK(a.mono == b.mono);
  }
}

TEST_CASE("player builder: out-of-range values are clamped to the safe range", "[player_builder]") {
  for (const auto& info : playerInstruments()) {
    for (const PlayerField& f : playerFields(info.id)) {
      INFO(info.label << " " << f.key);
      PlayerValues v = defaultValues(info.id);
      v[f.key] = f.maxValue + 1000.0;
      const PlayerRig hi = makePlayer(info.id, v);
      CHECK(f.get(hi) == (f.toggle ? 1.0 : f.maxValue));
      v[f.key] = f.minValue - 1000.0;
      const PlayerRig lo = makePlayer(info.id, v);
      CHECK(f.get(lo) == (f.toggle ? 0.0 : f.minValue));
      v[f.key] = std::nan("");
      CHECK(clampValues(info.id, v)[f.key] == f.defaultValue);
    }
  }
  CHECK(clampValues(PlayerInstrument::Violin, {{"noSuchField", 3.0}}) == defaultValues(PlayerInstrument::Violin));
}

TEST_CASE("player builder: every control is plumbed into the player", "[player_builder]") {
  // Setting a field to a non-default value changes what its getter reads
  // back, and changes no other field.
  for (const auto& info : playerInstruments()) {
    const auto fields = playerFields(info.id);
    for (const PlayerField& f : fields) {
      INFO(info.label << " " << f.key);
      PlayerValues v = defaultValues(info.id);
      v[f.key] = farValue(f);
      const PlayerRig r = makePlayer(info.id, v);
      CHECK(f.get(r) == clampFieldValue(f, farValue(f)));
      CHECK(f.get(r) != f.defaultValue);
      for (const PlayerField& g : fields)
        if (std::string(g.key) != f.key) CHECK(g.get(r) == g.defaultValue);
    }
  }
}

// How much moving one control (to either end, or just off its low end)
// changes the render, relative to the render's level. Some controls only
// act together with another one (vibrato speed needs some vibrato depth;
// the drift limit only bites when the timing is loose; slurs need the full
// tune), so a control that is silent on the short phrase is retried on the
// full tune, then with every other control of the instrument moved too.
static double controlEffect(PlayerInstrument i, const PlayerField& f) {
  const auto fields = playerFields(i);
  std::vector<double> xs{farValue(f)};
  if (!f.toggle) {
    xs.push_back(farValue(f) == f.maxValue ? f.minValue : f.maxValue);
    xs.push_back(f.minValue + 0.05 * (f.maxValue - f.minValue));
  }
  double best = 0.0;
  for (int attempt = 0; attempt < 3 && best <= 1e-3; ++attempt) {
    const au::Phrase phrase = attempt == 0 ? au::Phrase::Short : au::Phrase::Tune;
    PlayerValues base = defaultValues(i);
    if (attempt == 2)
      for (const PlayerField& g : fields)
        if (std::string(g.key) != f.key && std::string(g.key) != "noteLength") base[g.key] = farValue(g);
    const auto ref = au::renderAudition(makePlayer(i, base), phrase);
    if (!ref.ok) return -1.0;
    const double level = au::rmsOf(ref.mono);
    if (!(level > 0.0)) return -1.0;
    for (double x : xs) {
      if (clampFieldValue(f, x) == base[f.key]) continue;
      PlayerValues v = base;
      v[f.key] = x;
      const auto a = au::renderAudition(makePlayer(i, v), phrase);
      if (a.ok) best = std::max(best, diffRms(a.mono, ref.mono) / level);
    }
  }
  return best;
}

TEST_CASE("player builder: every control changes the sound (quick instruments)", "[player_builder]") {
  for (const auto& info : playerInstruments()) {
    if (!quick(info.id)) continue;
    for (const PlayerField& f : playerFields(info.id)) {
      INFO(info.label << " " << f.key);
      CHECK(controlEffect(info.id, f) > 1e-3);
    }
  }
}

TEST_CASE("player builder: a custom player renders measurably differently", "[player_builder]") {
  // A harder, looser harpist: louder by the loudness factor, onsets moved.
  PlayerValues v = defaultValues(PlayerInstrument::Harp);
  v["pluckForceScale"] = 2.0;
  v["timingOffsetSec"] = 0.040;
  const auto a = au::renderAudition(builtInPlayer(PlayerInstrument::Harp), au::Phrase::Tune);
  const auto b = au::renderAudition(makePlayer(PlayerInstrument::Harp, v), au::Phrase::Tune);
  REQUIRE(a.ok);
  REQUIRE(b.ok);
  CHECK(au::rmsOf(b.mono) / au::rmsOf(a.mono) > 1.8);
  auto onset = [](const std::vector<double>& x) {
    const double p = au::peakOf(x);
    for (std::size_t k = 0; k < x.size(); ++k) if (std::fabs(x[k]) > 0.05 * p) return double(k) / au::kRate;
    return 0.0;
  };
  CHECK(onset(b.mono) - onset(a.mono) > 0.030);
  // A violinist with a much wider, slower vibrato differs from the built-in one.
  PlayerValues w = defaultValues(PlayerInstrument::Violin);
  w["vibratoCents"] = 40.0;
  w["vibratoHz"] = 4.0;
  const auto c = au::renderAudition(builtInPlayer(PlayerInstrument::Violin), au::Phrase::Tune);
  const auto d = au::renderAudition(makePlayer(PlayerInstrument::Violin, w), au::Phrase::Tune);
  REQUIRE(c.ok);
  REQUIRE(d.ok);
  CHECK(diffRms(c.mono, d.mono) / au::rmsOf(c.mono) > 0.05);
}

TEST_CASE("player builder: named players save and load exactly", "[player_builder]") {
  std::vector<CustomPlayer> list;
  for (const auto& info : playerInstruments()) {
    CustomPlayer p;
    p.instrument = info.id;
    p.name = std::string("Mike's ") + info.label + " #1";
    p.values = defaultValues(info.id);
    // Awkward values: thirds, tiny and edge values.
    for (auto& [k, x] : p.values) x = x + (x == 0.0 ? 1.0e-3 : x / 3.0);
    p.values = clampValues(info.id, p.values);
    upsertPlayer(list, p);
  }
  const auto path = std::filesystem::temp_directory_path() / "mmathen_pb_test" / "players.txt";
  std::error_code ec;
  std::filesystem::remove_all(path.parent_path(), ec);
  REQUIRE(savePlayers(path, list));
  const auto back = loadPlayers(path);
  CHECK(back == list);
  // Text round trip too, and replacement by name.
  CHECK(playersFromText(playersToText(list)) == list);
  CustomPlayer again = list.front();
  again.values.begin()->second = clampValues(again.instrument, {{again.values.begin()->first, 1.0e9}})[again.values.begin()->first];
  upsertPlayer(list, again);
  CHECK(list.size() == back.size());
  CHECK(removePlayer(list, again.instrument, again.name));
  CHECK_FALSE(removePlayer(list, again.instrument, again.name));
  CHECK(cleanPlayerName("  a\nb\t ") == "a b");
  CHECK(loadPlayers(path.parent_path() / "missing.txt").empty());
  // Values out of range in a file are clamped on load.
  const auto clamped = playersFromText("player violin Wild\nvibratoCents 900\nend\n");
  REQUIRE(clamped.size() == 1);
  CHECK(clamped[0].values.at("vibratoCents") == 40.0);
  CHECK(defaultPlayersPath().filename() == "players.txt");
  std::filesystem::remove_all(path.parent_path(), ec);
}

TEST_CASE("player builder: every control changes the sound (slow instruments)", "[.player_builder_slow]") {
  // Hidden (minutes): the snare, kit, tubular bells and piano controls.
  for (const auto& info : playerInstruments()) {
    if (quick(info.id)) continue;
    for (const PlayerField& f : playerFields(info.id)) {
      INFO(info.label << " " << f.key);
      CHECK(controlEffect(info.id, f) > 1e-3);
    }
  }
}
