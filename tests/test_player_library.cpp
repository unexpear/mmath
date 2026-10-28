// Step 24.2: the Player builder's saved-players logic (app/player_library.h),
// UI-free: unsaved changes and their baseline, Revert to saved, overwrite
// detection, load / delete, export / import with name clashes, slider
// snapping and real system errors on a failed save.
#include <catch2/catch_test_macros.hpp>

#include "../app/player_library.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace mmathen;
using app::builder::PlayerLibrary;
namespace fs = std::filesystem;

namespace {
fs::path freshDir(const char* name) {
  const fs::path d = fs::temp_directory_path() / "mmathen_player_library_test" / name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
PlayerField fieldOf(PlayerInstrument i, const std::string& key) {
  for (const auto& f : playerFields(i))
    if (key == f.key) return f;
  FAIL("no field " << key);
  return {};
}
}  // namespace

TEST_CASE("player library: a slider dragged back to the default reads as unchanged", "[player_library]") {
  for (const auto& info : playerInstruments()) {
    for (const PlayerField& f : playerFields(info.id)) {
      // What the float slider hands back for the default value.
      const double viaFloat = static_cast<double>(static_cast<float>(f.defaultValue * f.displayScale)) / f.displayScale;
      CHECK(app::builder::snapToDefault(f, viaFloat) == f.defaultValue);
      // One shown step away is a real change.
      if (!f.toggle && !f.integer) {
        const double step = std::pow(10.0, -app::builder::displayDecimals(f)) / f.displayScale;
        const double moved = f.defaultValue + step <= f.maxValue ? f.defaultValue + step : f.defaultValue - step;
        CHECK(app::builder::snapToDefault(f, moved) == moved);
      }
    }
  }
  PlayerLibrary lib(freshDir("snap") / "players.txt");
  lib.select(PlayerInstrument::Flute);
  const PlayerField f = fieldOf(PlayerInstrument::Flute, "tongueReleaseSec");
  lib.setValue(f, static_cast<double>(static_cast<float>(f.defaultValue * f.displayScale)) / f.displayScale);
  CHECK_FALSE(lib.unsaved(PlayerInstrument::Flute));
  CHECK(lib.changedFromBuiltIn(PlayerInstrument::Flute) == 0);
}

TEST_CASE("player library: unsaved changes, Revert to saved and Reset to built-in", "[player_library]") {
  const fs::path path = freshDir("revert") / "players.txt";
  PlayerLibrary lib(path);
  lib.ensureLoaded();
  lib.select(PlayerInstrument::Violin);
  const PlayerField depth = fieldOf(PlayerInstrument::Violin, "vibratoCents");
  CHECK_FALSE(lib.anyUnsaved());
  lib.setValue(depth, 35.0);
  CHECK(lib.unsaved(PlayerInstrument::Violin));
  CHECK_FALSE(lib.canRevert());  // nothing saved to go back to
  REQUIRE(lib.save("Wide").ok);
  CHECK_FALSE(lib.unsaved(PlayerInstrument::Violin));
  CHECK(lib.savedName(PlayerInstrument::Violin) == std::optional<std::string>("Wide"));
  lib.setValue(depth, 10.0);
  CHECK(lib.unsaved(PlayerInstrument::Violin));
  CHECK(lib.canRevert());
  lib.revertToSaved();
  CHECK(lib.values().at("vibratoCents") == 35.0);
  CHECK_FALSE(lib.unsaved(PlayerInstrument::Violin));
  lib.resetToBuiltIn();  // back to the built-in: now differs from the saved player
  CHECK(lib.values() == defaultValues(PlayerInstrument::Violin));
  CHECK(lib.unsaved(PlayerInstrument::Violin));
  lib.revertToSaved();
  // Switching instruments keeps each instrument's edits.
  lib.select(PlayerInstrument::Cello);
  lib.setValue(fieldOf(PlayerInstrument::Cello, "vibratoHz"), 7.0);
  CHECK(lib.unsavedInstruments() == std::vector<PlayerInstrument>{PlayerInstrument::Cello});
  lib.select(PlayerInstrument::Violin);
  CHECK(lib.values().at("vibratoCents") == 35.0);
  CHECK(lib.values(PlayerInstrument::Cello).at("vibratoHz") == 7.0);
}

TEST_CASE("player library: load asks about unsaved changes; save over a name is detected", "[player_library]") {
  const fs::path path = freshDir("load") / "players.txt";
  PlayerLibrary lib(path);
  lib.ensureLoaded();
  lib.select(PlayerInstrument::Cello);
  lib.setValue(fieldOf(PlayerInstrument::Cello, "vibratoHz"), 6.5);
  REQUIRE(lib.save("Mine").ok);
  lib.select(PlayerInstrument::Violin);
  lib.setValue(fieldOf(PlayerInstrument::Violin, "vibratoHz"), 7.5);
  REQUIRE(lib.save("Fast").ok);
  CHECK(lib.nameTaken("Fast"));
  CHECK(lib.nameTaken("  Fast "));  // the same name once cleaned
  CHECK_FALSE(lib.nameTaken("Slow"));
  lib.select(PlayerInstrument::Cello);
  CHECK(lib.nameTaken("Mine"));
  CHECK_FALSE(lib.nameTaken("Fast"));  // a violin player: no clash for the cello
  // Edit the violin, then load: the violin player would discard, the cello one not.
  lib.select(PlayerInstrument::Violin);
  lib.setValue(fieldOf(PlayerInstrument::Violin, "vibratoHz"), 4.0);
  const int violinK = 1, celloK = 0;
  REQUIRE(lib.players()[static_cast<std::size_t>(violinK)].name == "Fast");
  CHECK(lib.loadWouldDiscard(violinK));
  CHECK_FALSE(lib.loadWouldDiscard(celloK));
  REQUIRE(lib.load(celloK).ok);
  CHECK(lib.instrument() == PlayerInstrument::Cello);
  CHECK(lib.values().at("vibratoHz") == 6.5);
  REQUIRE(lib.load(violinK).ok);  // the user said "load and lose changes"
  CHECK(lib.values().at("vibratoHz") == 7.5);
  CHECK_FALSE(lib.anyUnsaved());
  // Deleting the player the edits came from: they are no longer saved anywhere.
  REQUIRE(lib.remove(violinK).ok);
  CHECK(lib.unsaved(PlayerInstrument::Violin));
  CHECK_FALSE(lib.savedName(PlayerInstrument::Violin).has_value());
  CHECK(lib.players().size() == 1);
}

TEST_CASE("player library: export one player and import it with name clashes", "[player_library]") {
  const fs::path dir = freshDir("share");
  PlayerLibrary mine(dir / "mine.txt");
  mine.ensureLoaded();
  mine.select(PlayerInstrument::Trumpet);
  mine.setValue(fieldOf(PlayerInstrument::Trumpet, "pressureScale"), 1.2);
  REQUIRE(mine.save("Lead: \"loud\"").ok);
  const CustomPlayer lead = mine.players()[0];
  CHECK(app::builder::exportFileName(lead) == "trumpet-Lead_ _loud_.mmplayer");
  const fs::path file = dir / app::builder::exportFileName(lead);
  REQUIRE(mine.exportPlayer(0, file).ok);
  CHECK_FALSE(mine.exportPlayer(5, file).ok);
  const PlayersFile one = playersFileFromText([&] { std::ifstream f(file, std::ios::binary); return std::string((std::istreambuf_iterator<char>(f)), {}); }());
  REQUIRE(one.players.size() == 1);
  CHECK(one.players[0] == lead);

  PlayerLibrary theirs(dir / "theirs.txt");
  theirs.ensureLoaded();
  auto in = theirs.readImport(file);
  REQUIRE(in.ok);
  CHECK(in.clashes == 0);
  REQUIRE(theirs.importPlayers(in.players, PlayerLibrary::Clash::KeepBoth).ok);
  CHECK(theirs.players().size() == 1);
  in = theirs.readImport(file);
  CHECK(in.clashes == 1);
  REQUIRE(theirs.importPlayers(in.players, PlayerLibrary::Clash::Skip).ok);
  CHECK(theirs.players().size() == 1);
  REQUIRE(theirs.importPlayers(in.players, PlayerLibrary::Clash::Replace).ok);
  CHECK(theirs.players().size() == 1);
  REQUIRE(theirs.importPlayers(in.players, PlayerLibrary::Clash::KeepBoth).ok);
  REQUIRE(theirs.players().size() == 2);
  CHECK(theirs.players()[1].name == "Lead: \"loud\" (2)");
  CHECK(theirs.players()[1].values == lead.values);
  // A long name stays within the limit when renamed.
  CustomPlayer longName = lead;
  longName.name = std::string(kPlayerNameMaxChars, 'x');
  REQUIRE(theirs.importPlayers({longName, longName}, PlayerLibrary::Clash::KeepBoth).ok);
  CHECK(theirs.players().back().name == std::string(kPlayerNameMaxChars - 4, 'x') + " (2)");
  // Bad import files.
  CHECK_FALSE(theirs.readImport(dir / "nope.mmplayer").ok);
  fs::create_directories(dir / "folder.mmplayer");
  const auto bad = theirs.readImport(dir / "folder.mmplayer");
  CHECK_FALSE(bad.ok);
  CHECK(bad.message.find("can't be read") != std::string::npos);
  std::ofstream(dir / "empty.mmplayer") << "nothing here\n";
  CHECK_FALSE(theirs.readImport(dir / "empty.mmplayer").ok);
}

TEST_CASE("player library: a failed save shows the system's reason", "[player_library]") {
  const fs::path dir = freshDir("fail");
  std::ofstream(dir / "afile") << "x";
  PlayerLibrary lib(dir / "afile" / "players.txt");  // its folder is a file
  lib.ensureLoaded();
  lib.select(PlayerInstrument::Harp);
  const auto r = lib.save("Bright");
  CHECK_FALSE(r.ok);
  CHECK(r.message.find(pathText(dir / "afile")) != std::string::npos);
  const auto colon = r.message.rfind(": ");
  REQUIRE(colon != std::string::npos);
  CHECK(r.message.size() > colon + 3);  // the system's text follows
  CHECK(r.message.find("the system gave no reason") == std::string::npos);
  CHECK(lib.unsaved(PlayerInstrument::Harp) == false);  // nothing changed, nothing claimed saved
  CHECK_FALSE(lib.savedName(PlayerInstrument::Harp).has_value());
}

TEST_CASE("player library: Open folder URL", "[player_library]") {
  CHECK(app::builder::fileUrl(fs::path(u8"a b/\u00e9")) == "file:///a%20b/%C3%A9");
  CHECK(app::builder::fileUrl(fs::path("/tmp/x")) == "file:///tmp/x");
}

TEST_CASE("player library: non-ASCII folders work and messages show them as UTF-8", "[player_library]") {
  const fs::path dir = freshDir("utf8") / fs::path(u8"M\u00fcller \u97f3\u697d");
  CHECK(pathText(fs::path(u8"M\u00fcller")) == "M\xc3\xbcller");
  PlayerLibrary lib(dir / "players.txt");
  lib.ensureLoaded();
  lib.select(PlayerInstrument::Flute);
  lib.setValue(fieldOf(PlayerInstrument::Flute, "pressureScale"), 1.1);
  REQUIRE(lib.save("Airy").ok);
  const fs::path file = dir / app::builder::exportFileName(lib.players()[0]);
  const auto out = lib.exportPlayer(0, file);
  REQUIRE(out.ok);
  CHECK(out.message.find("M\xc3\xbcller \xe9\x9f\xb3\xe6\xa5\xbd") != std::string::npos);
  PlayerLibrary other(dir / "other.txt");
  other.ensureLoaded();
  const auto in = other.readImport(file);
  REQUIRE(in.ok);
  REQUIRE(in.players.size() == 1);
  CHECK(in.players[0].name == "Airy");
}

TEST_CASE("player library: slider values are kept to the shown digits", "[player_library]") {
  const PlayerField depth = fieldOf(PlayerInstrument::Violin, "vibratoCents");
  REQUIRE(app::builder::displayDecimals(depth) == 1);
  PlayerLibrary lib(freshDir("digits") / "players.txt");
  lib.ensureLoaded();
  lib.select(PlayerInstrument::Violin);
  lib.setValue(depth, static_cast<double>(31.3f));  // what a float slider hands back
  CHECK(lib.values(PlayerInstrument::Violin).at("vibratoCents") == 31.3);
  REQUIRE(lib.save("Warm").ok);
  std::ifstream in(lib.path(), std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), {});
  CHECK(text.find("vibratoCents 31.3\n") != std::string::npos);
  lib.setValue(depth, depth.maxValue + 100.0);  // still clamped
  CHECK(lib.values(PlayerInstrument::Violin).at("vibratoCents") == depth.maxValue);
}
