// Step 24.2e: the four wind faults together (cracks 24.2a, scoops 24.2b,
// fall-offs 24.2c, fatigue 24.2d). These tests check that every fault
// control is a builder slider on each of the five winds, in its group,
// off by default; that a player with every fault control moved survives
// save, load, export (.mmplayer) and import unchanged and renders
// bit-identically; that each fault control audibly changes the render on
// each wind (the gated controls with their gate on and the how-often
// sliders at 100%); and that the faults together, at moderate settings
// and at the slider ends, render finite, not silent, within the -1 dBTP
// ceiling after the limiter, and in bounded time.
#include <catch2/catch_test_macros.hpp>

#include "../app/player_audition.h"
#include "../app/player_library.h"

#include <mmathen/dsp/loudness_normalize.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace mmathen;
using app::builder::PlayerLibrary;
namespace au = app::audition;
namespace fs = std::filesystem;

namespace {
constexpr const char* kWinds[] = {"trumpet", "trombone", "clarinet", "alto_sax", "flute"};

PlayerInstrument instrumentOf(const char* key) {
  PlayerInstrument i{};
  REQUIRE(playerInstrumentFromKey(key, i));
  return i;
}
const PlayerField* fieldOf(PlayerInstrument i, const std::string& key) {
  static std::map<PlayerInstrument, std::vector<PlayerField>> cache;
  auto& fields = cache[i];
  if (fields.empty()) fields = playerFields(i);
  for (const auto& f : fields)
    if (key == f.key) return &f;
  return nullptr;
}
double diffRms(const std::vector<double>& a, const std::vector<double>& b) {
  const std::size_t n = std::max(a.size(), b.size());
  double s = 0.0;
  for (std::size_t k = 0; k < n; ++k) {
    const double x = (k < a.size() ? a[k] : 0.0) - (k < b.size() ? b[k] : 0.0);
    s += x * x;
  }
  return n ? std::sqrt(s / static_cast<double>(n)) : 0.0;
}
std::vector<double> render(PlayerInstrument i, const PlayerValues& v) {
  const auto a = au::renderAudition(makePlayer(i, v), au::Phrase::Tune);
  REQUIRE(a.ok);
  return a.mono;
}

// Every fault control, its group, and its off value.
struct FaultControl {
  const char* key;
  const char* group;
  double off;  // the default: the depth / rate controls are 0
};
constexpr FaultControl kFaults[] = {
    {"crackSdCents", "Mistakes", 0.0},          {"scoopCents", "Note starts & ends", 0.0},
    {"scoopSec", "Note starts & ends", 0.0},    {"scoopChance", "Note starts & ends", 1.0},
    {"fallCents", "Note starts & ends", 0.0},   {"fallSec", "Note starts & ends", 0.0},
    {"fallChance", "Note starts & ends", 1.0},  {"fallPhraseEndsOnly", "Note starts & ends", 1.0},
    {"fatigueRate", "Stamina", 0.0},            {"fatigueRecovery", "Stamina", 1.0}};

// A non-default value of each control that the slider shows exactly.
const std::map<std::string, double> kMoved{{"crackSdCents", 333.0}, {"scoopCents", 77.0},  {"scoopSec", 0.123},
                                           {"scoopChance", 0.6},    {"fallCents", 444.0},  {"fallSec", 0.567},
                                           {"fallChance", 0.7},     {"fallPhraseEndsOnly", 0.0},
                                           {"fatigueRate", 2.5},    {"fatigueRecovery", 1.75}};
}  // namespace

TEST_CASE("wind faults: every fault control is a slider on each wind, in its group, off by default", "[wind_faults]") {
  for (const char* key : kWinds) {
    const PlayerInstrument i = instrumentOf(key);
    for (const FaultControl& c : kFaults) {
      INFO(key << " " << c.key);
      const PlayerField* f = fieldOf(i, c.key);
      REQUIRE(f != nullptr);
      CHECK(std::string(f->group) == c.group);
      CHECK(f->defaultValue == c.off);
      CHECK(std::string(f->tooltip).size() > 20);
      CHECK(f->get(builtInPlayer(i)) == c.off);
    }
    // With every depth and rate at 0, the gated controls change nothing.
    PlayerValues v = defaultValues(i);
    for (const char* k : {"scoopSec", "scoopChance", "fallSec", "fallChance", "fallPhraseEndsOnly", "fatigueRecovery"})
      v[k] = kMoved.at(k);
    INFO(key);
    CHECK(render(i, v) == render(i, defaultValues(i)));
  }
}

TEST_CASE("wind faults: a player with every fault moved survives save, load, export and import", "[wind_faults]") {
  const fs::path dir = fs::temp_directory_path() / "mmathen_wind_faults_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  for (const char* key : kWinds) {
    INFO(key);
    const PlayerInstrument i = instrumentOf(key);
    PlayerLibrary mine(dir / (std::string(key) + "-mine.txt"));
    mine.ensureLoaded();
    mine.select(i);
    for (const auto& [k, x] : kMoved) mine.setValue(*fieldOf(i, k), x);
    for (const auto& [k, x] : kMoved) CHECK(mine.values()[k] == x);
    const PlayerValues edited = mine.values();
    REQUIRE(mine.save("Tired jazz").ok);
    const auto want = render(i, edited);
    CHECK(want != render(i, defaultValues(i)));

    // Load from the players file (version 2) in a fresh library.
    PlayerLibrary again(dir / (std::string(key) + "-mine.txt"));
    again.ensureLoaded();
    REQUIRE(again.players().size() == 1);
    REQUIRE(again.load(0).ok);
    CHECK(again.values(i) == edited);
    CHECK(render(i, again.values(i)) == want);
    const std::string text = playersToText(again.players());
    CHECK(text.find(std::string("format ") + kPlayersFormatTag + " 2") != std::string::npos);

    // Export to .mmplayer and import into another library.
    const fs::path file = dir / app::builder::exportFileName(again.players()[0]);
    CHECK(file.extension() == ".mmplayer");
    REQUIRE(again.exportPlayer(0, file).ok);
    PlayerLibrary theirs(dir / (std::string(key) + "-theirs.txt"));
    theirs.ensureLoaded();
    const auto in = theirs.readImport(file);
    REQUIRE(in.ok);
    CHECK(in.warnings == 0);
    REQUIRE(theirs.importPlayers(in.players, PlayerLibrary::Clash::KeepBoth).ok);
    REQUIRE(theirs.players().size() == 1);
    REQUIRE(theirs.load(0).ok);
    CHECK(theirs.values(i) == edited);
    CHECK(render(i, theirs.values(i)) == want);
  }
}

TEST_CASE("wind faults: each fault control audibly changes each wind's render", "[wind_faults]") {
  // The control's value, and the gate it needs on (the depth or the
  // tiredness) in both renders; the how-often sliders stay at 100%.
  struct Probe {
    const char* key;
    double x;
    const char* gate;
    double gateValue;
  };
  constexpr Probe probes[] = {{"crackSdCents", 150.0, "", 0.0},   {"scoopCents", 60.0, "", 0.0},
                              {"scoopSec", 0.4, "scoopCents", 100.0}, {"scoopChance", 0.5, "scoopCents", 100.0},
                              {"fallCents", 200.0, "", 0.0},      {"fallSec", 1.0, "fallCents", 300.0},
                              {"fallChance", 0.0, "fallCents", 300.0}, {"fallPhraseEndsOnly", 0.0, "fallCents", 300.0},
                              {"fatigueRate", 1.0, "", 0.0},      {"fatigueRecovery", 4.0, "fatigueRate", 5.0}};
  for (const char* key : kWinds) {
    const PlayerInstrument i = instrumentOf(key);
    for (const Probe& p : probes) {
      INFO(key << " " << p.key);
      PlayerValues base = defaultValues(i);
      if (*p.gate) base[p.gate] = p.gateValue;
      const auto ref = render(i, base);
      PlayerValues v = base;
      v[p.key] = p.x;
      const auto a = render(i, v);
      const double effect = diffRms(a, ref) / au::rmsOf(ref);
      INFO("effect " << effect);
      CHECK(effect > 1e-3);
    }
  }
}

TEST_CASE("wind faults: the faults together stay finite, sounding and under the ceiling", "[wind_faults]") {
  // Moderate: a tired jazz player. Max: every fault control at its most.
  for (const char* key : kWinds) {
    const PlayerInstrument i = instrumentOf(key);
    PlayerValues moderate = defaultValues(i);
    moderate["crackSdCents"] = 150.0;
    moderate["scoopCents"] = 60.0;
    moderate["scoopChance"] = 0.5;
    moderate["fallCents"] = 300.0;
    moderate["fatigueRate"] = 2.0;
    PlayerValues most = defaultValues(i);
    for (const char* k : {"crackSdCents", "scoopCents", "scoopSec", "fallCents", "fallSec", "fatigueRate"})
      most[k] = fieldOf(i, k)->maxValue;
    most["fatigueRecovery"] = fieldOf(i, "fatigueRecovery")->minValue;
    most["fallPhraseEndsOnly"] = 0.0;
    const double cleanRms = au::rmsOf(render(i, defaultValues(i)));
    for (const PlayerValues* v : {&moderate, &most}) {
      for (au::Phrase phrase : {au::Phrase::Tune, au::Phrase::Scale}) {
        INFO(key << (v == &moderate ? " moderate" : " max") << (phrase == au::Phrase::Tune ? " tune" : " scale"));
        const auto t0 = std::chrono::steady_clock::now();
        auto a = au::renderAudition(makePlayer(i, *v), phrase);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        REQUIRE(a.ok);
        CHECK(sec < 60.0);  // no stall (a clean tune renders in a few seconds)
        CHECK(std::all_of(a.mono.begin(), a.mono.end(), [](double x) { return std::isfinite(x); }));
        // Sounding: within 20 dB of the clean player's level.
        CHECK(au::rmsOf(a.mono) > 0.1 * cleanRms);
        // Through the limiter at a loud -14 LUFS: no true peak over -1 dBTP.
        NormalizeTarget target;
        target.lufs = -14.0;
        target.truePeakDb = -1.0;
        auto rep = normalizeLoudness(a.mono, 1, SampleRateHz{static_cast<double>(au::kRate)}, target);
        REQUIRE(rep.ok());
        INFO("true peak " << rep.value().outputTruePeakDb);
        CHECK(rep.value().outputTruePeakDb <= -1.0 + 0.05);
      }
    }
  }
}
