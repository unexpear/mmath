// Step 24.1: the players file, version 2 (player_builder.h): version
// header, v1 migration, nothing lost (kept blocks and lines, unreadable
// file, two writers, .bak), BOM / CRLF, UTF-8 names, instrument:name
// lookup and line-numbered warnings.
#include <catch2/catch_test_macros.hpp>

#include <mmathen/instruments/player_builder.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace mmathen;
namespace fs = std::filesystem;

namespace {
fs::path freshDir(const char* name) {
  const fs::path d = fs::temp_directory_path() / "mmathen_players_file_test" / name;
  std::error_code ec;
  fs::remove_all(d, ec);
  fs::create_directories(d, ec);
  return d;
}
std::string readAll(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}
void writeAll(const fs::path& p, const std::string& text) {
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f << text;
}
bool hasWarning(const PlayersFile& f, int line, const std::string& part) {
  for (const auto& w : f.warnings)
    if (w.line == line && w.message.find(part) != std::string::npos) return true;
  return false;
}
// Every player of every instrument with awkward values, as step 23 wrote it.
std::vector<CustomPlayer> awkwardPlayers() {
  std::vector<CustomPlayer> list;
  for (const auto& info : playerInstruments()) {
    CustomPlayer p;
    p.instrument = info.id;
    p.name = std::string("Mike's ") + info.label;
    p.values = defaultValues(info.id);
    for (auto& [k, x] : p.values) x = x + (x == 0.0 ? 1.0e-3 : x / 3.0);
    p.values = clampValues(info.id, p.values);
    list.push_back(p);
  }
  return list;
}
std::string v1Text(const std::vector<CustomPlayer>& list) {
  std::string out = "# mmathEn custom players v1\n";
  for (const auto& p : list) {
    out += std::string("player ") + playerInstrumentInfo(p.instrument).key + " " + p.name + "\n";
    for (const auto& [k, x] : p.values) out += k + " " + player_builder_detail::formatExact(x) + "\n";
    out += "end\n";
  }
  return out;
}
}  // namespace

TEST_CASE("players file: the version header is written and checked", "[players_file]") {
  const std::string text = playersToText(awkwardPlayers());
  CHECK(text.find("\nformat mmathEn-players 2\n") != std::string::npos);
  const PlayersFile back = playersFileFromText(text);
  CHECK(back.version == 2);
  CHECK(back.warnings.empty());
  CHECK(back.players == awkwardPlayers());

  // A newer version: read what it can, but read-only; nothing is written.
  const fs::path dir = freshDir("newer");
  const fs::path path = dir / "players.txt";
  const std::string newer = "format mmathEn-players 3\nplayer violin A\nvibratoCents 20\nfutureThing 1\nend\n";
  writeAll(path, newer);
  const PlayersLoad l = loadPlayersFile(path);
  CHECK(l.state == PlayersFileState::Newer);
  CHECK_FALSE(l.canSave());
  CHECK(l.file.players.size() == 1);
  CHECK(hasWarning(l.file, 1, "newer version"));
  const PlayersEdit e = editPlayersFile(path, [](std::vector<CustomPlayer>& v) { v.clear(); });
  CHECK_FALSE(e.ok);
  CHECK(readAll(path) == newer);
  CHECK_FALSE(fs::exists(dir / "players.txt.bak"));

  // A garbled format line is kept and reported, not trusted.
  const PlayersFile g = playersFileFromText("format something-else 9\nplayer violin A\nend\n");
  CHECK(g.version == 1);
  CHECK(hasWarning(g, 1, "format"));
  CHECK(g.keptText == std::vector<std::string>{"format something-else 9"});
}

TEST_CASE("players file: a step-23 (v1) file migrates with every value unchanged", "[players_file]") {
  const auto list = awkwardPlayers();
  const std::string v1 = v1Text(list);
  const PlayersFile f = playersFileFromText(v1);
  CHECK(f.version == 1);
  CHECK(f.warnings.empty());
  CHECK(f.players == list);

  const fs::path dir = freshDir("v1");
  const fs::path path = dir / "players.txt";
  writeAll(path, v1);
  CustomPlayer extra;
  extra.instrument = PlayerInstrument::Cello;
  extra.name = "New";
  extra.values = defaultValues(PlayerInstrument::Cello);
  const PlayersEdit e = editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, extra); });
  REQUIRE(e.ok);
  CHECK(e.after.state == PlayersFileState::Ok);
  CHECK(e.after.file.version == 2);
  REQUIRE(e.after.file.players.size() == list.size() + 1);
  for (std::size_t k = 0; k < list.size(); ++k) CHECK(e.after.file.players[k] == list[k]);
  CHECK(readAll(dir / "players.txt.bak") == v1);  // the v1 original is kept
}

TEST_CASE("players file: lines and blocks it can't use are kept exactly and written back", "[players_file]") {
  const std::string text =
      "# my own notes about these players\n"          // 1
      "player violin Warm\n"                          // 2
      "vibratoCents 30\n"                             // 3
      "laterVersionSetting 0.5\n"                     // 4
      "bowForceScale lots\n"                          // 5
      "# a comment inside a player\n"                 // 6
      "end\n"                                         // 7
      "player banjo Bob\n"                            // 8
      "twang 7\n"                                     // 9
      "end\n"                                         // 10
      "player cello\n"                                // 11
      "vibratoHz 6\n"                                 // 12
      "end\n"                                         // 13
      "stray words\n";                                // 14
  const PlayersFile f = playersFileFromText(text);
  REQUIRE(f.players.size() == 1);
  CHECK(f.players[0].name == "Warm");
  CHECK(f.players[0].values.at("vibratoCents") == 30.0);
  CHECK(f.players[0].keptLines ==
        std::vector<std::string>{"laterVersionSetting 0.5", "bowForceScale lots", "# a comment inside a player"});
  CHECK(hasWarning(f, 4, "laterVersionSetting"));
  CHECK(hasWarning(f, 5, "bowForceScale"));
  CHECK(hasWarning(f, 8, "unknown instrument \"banjo\""));
  CHECK(hasWarning(f, 11, "no name"));
  CHECK(hasWarning(f, 14, "outside any player"));
  CHECK(f.warnings.size() == 5);

  const fs::path dir = freshDir("kept");
  const fs::path path = dir / "players.txt";
  writeAll(path, text);
  CustomPlayer p;
  p.instrument = PlayerInstrument::Flute;
  p.name = "Airy";
  p.values = defaultValues(PlayerInstrument::Flute);
  const PlayersEdit e = editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, p); });
  REQUIRE(e.ok);
  const std::string after = readAll(path);
  CHECK(after.find("# my own notes about these players\n") != std::string::npos);
  CHECK(after.find("player banjo Bob\ntwang 7\nend\n") != std::string::npos);
  CHECK(after.find("player cello\nvibratoHz 6\nend\n") != std::string::npos);
  CHECK(after.find("stray words\n") != std::string::npos);
  CHECK(after.find("laterVersionSetting 0.5\nbowForceScale lots\n# a comment inside a player\nend\n") != std::string::npos);
  CHECK(e.after.file.players.size() == 2);
  // Reading the written file again keeps the same things (a fixed point).
  CHECK(playersToText(playersFileFromText(after)) == after);

  // Saving the player again from the builder replaces the unreadable value
  // of a known setting, and keeps the unknown setting and the comment.
  CustomPlayer warm = e.after.file.players[0];
  warm.keptLines.clear();
  const PlayersEdit e2 = editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, warm); });
  REQUIRE(e2.ok);
  CHECK(e2.after.file.players[0].keptLines ==
        std::vector<std::string>{"laterVersionSetting 0.5", "# a comment inside a player"});
}

TEST_CASE("players file: a file that can't be read is not treated as empty", "[players_file]") {
  const fs::path dir = freshDir("unreadable");
  // Missing: an empty list, and saving creates it (no backup to keep).
  const fs::path missing = dir / "new" / "players.txt";
  const PlayersLoad m = loadPlayersFile(missing);
  CHECK(m.state == PlayersFileState::Missing);
  CHECK(m.canSave());
  CustomPlayer p;
  p.instrument = PlayerInstrument::Harp;
  p.name = "Bright";
  p.values = defaultValues(PlayerInstrument::Harp);
  const PlayersEdit made = editPlayersFile(missing, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, p); });
  REQUIRE(made.ok);
  CHECK(made.after.file.players.size() == 1);
  CHECK_FALSE(fs::exists(dir / "new" / "players.txt.bak"));

  // Exists but can't be read (here: a folder where the file should be):
  // reported, and every save or delete is refused with nothing written.
  const fs::path blocked = dir / "blocked.txt";
  fs::create_directories(blocked / "inside");
  const PlayersLoad u = loadPlayersFile(blocked);
  CHECK(u.state == PlayersFileState::Unreadable);
  CHECK_FALSE(u.error.empty());
  CHECK_FALSE(u.canSave());
  CHECK(loadPlayers(blocked).empty());
  const PlayersEdit e = editPlayersFile(blocked, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, p); });
  CHECK_FALSE(e.ok);
  CHECK(e.error.find("Nothing was changed") != std::string::npos);
  CHECK(fs::is_directory(blocked / "inside"));
  CHECK_FALSE(fs::exists(dir / "blocked.txt.bak"));
  CHECK_FALSE(fs::exists(dir / "blocked.txt.tmp"));
}

TEST_CASE("players file: each save re-reads the file and keeps a .bak", "[players_file]") {
  const fs::path dir = freshDir("merge");
  const fs::path path = dir / "players.txt";
  auto player = [](PlayerInstrument i, const char* name) {
    CustomPlayer p;
    p.instrument = i;
    p.name = name;
    p.values = defaultValues(i);
    return p;
  };
  REQUIRE(editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, player(PlayerInstrument::Violin, "A")); }).ok);
  // Window 1 has read the file (A). Window 2 then saves B.
  const PlayersLoad window1 = loadPlayersFile(path);
  REQUIRE(window1.file.players.size() == 1);
  REQUIRE(editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, player(PlayerInstrument::Piano, "B")); }).ok);
  const std::string beforeC = readAll(path);
  // Window 1 saves C: B must survive.
  const PlayersEdit e = editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { upsertPlayer(v, player(PlayerInstrument::Trumpet, "C")); });
  REQUIRE(e.ok);
  REQUIRE(e.after.file.players.size() == 3);
  CHECK(e.after.file.players[0].name == "A");
  CHECK(e.after.file.players[1].name == "B");
  CHECK(e.after.file.players[2].name == "C");
  CHECK(readAll(dir / "players.txt.bak") == beforeC);
  // A delete is applied to the file as it is now.
  const PlayersEdit d = editPlayersFile(path, [&](std::vector<CustomPlayer>& v) { (void)removePlayer(v, PlayerInstrument::Piano, "B"); });
  REQUIRE(d.ok);
  REQUIRE(d.after.file.players.size() == 2);
  CHECK(d.after.file.players[1].name == "C");
  CHECK_FALSE(fs::exists(dir / "players.txt.tmp"));
}

TEST_CASE("players file: byte-order mark and Windows line endings are read", "[players_file]") {
  const PlayersFile f = playersFileFromText("\xEF\xBB\xBFplayer violin A\r\nvibratoCents 20\r\nend\r\nplayer cello B\r\nend\r\n");
  CHECK(f.warnings.empty());
  REQUIRE(f.players.size() == 2);
  CHECK(f.players[0].name == "A");
  CHECK(f.players[0].values.at("vibratoCents") == 20.0);
  CHECK(f.players[1].name == "B");
}

TEST_CASE("players file: names are cut by character, never inside one", "[players_file]") {
  std::string accented;
  for (int k = 0; k < 70; ++k) accented += "\xC3\xA9";  // e-acute, 2 bytes
  const std::string a = cleanPlayerName(accented);
  CHECK(a.size() == 2 * kPlayerNameMaxChars);
  std::string cjk;
  for (int k = 0; k < 61; ++k) cjk += "\xE9\x9F\xB3";  // a 3-byte character
  CHECK(cleanPlayerName(cjk).size() == 3 * kPlayerNameMaxChars);
  CHECK(cleanPlayerName("Ab\xF0\x9F\x8E\xBB") == "Ab\xF0\x9F\x8E\xBB");  // a 4-byte emoji survives
  // Bad bytes become U+FFFD; control characters and DEL become spaces.
  CHECK(cleanPlayerName("a\xFF" "b") == "a\xEF\xBF\xBD" "b");
  CHECK(cleanPlayerName("a\xC3") == "a\xEF\xBF\xBD");        // cut-off sequence
  CHECK(cleanPlayerName("a\xC0\xAF") == "a\xEF\xBF\xBD\xEF\xBF\xBD");  // overlong form
  CHECK(cleanPlayerName("  a\x7F" "b\t\n ") == "a b");
  CHECK(cleanPlayerName(std::string(59, 'x') + "  yz") == std::string(59, 'x'));  // no trailing space after the cut
  // Round trip through the file.
  CustomPlayer p;
  p.instrument = PlayerInstrument::Viola;
  p.name = accented;
  p.values = defaultValues(PlayerInstrument::Viola);
  const PlayersFile f = playersFileFromText(playersToText(std::vector<CustomPlayer>{p}));
  REQUIRE(f.players.size() == 1);
  CHECK(f.players[0].name == a);
}

TEST_CASE("players file: instrument:name lookup for --builder-load", "[players_file]") {
  std::vector<CustomPlayer> list(3);
  list[0].instrument = PlayerInstrument::Violin;
  list[0].name = "Warm";
  list[1].instrument = PlayerInstrument::Cello;
  list[1].name = "Warm";
  list[2].instrument = PlayerInstrument::Piano;
  list[2].name = "Solo: night";
  std::string err;
  CHECK(findPlayer(list, "cello:Warm", err) == 1);
  CHECK(err.empty());
  CHECK(findPlayer(list, "violin:Warm", err) == 0);
  CHECK(findPlayer(list, "Warm", err) == -1);  // ambiguous
  CHECK(err.find("instrument:name") != std::string::npos);
  CHECK(findPlayer(list, "Solo: night", err) == 2);  // a colon in a name alone still works
  CHECK(findPlayer(list, "piano:Solo: night", err) == 2);
  CHECK(findPlayer(list, "Nobody", err) == -1);
  CHECK(err.find("No saved player") != std::string::npos);
}

TEST_CASE("players file: problems are reported with their line numbers", "[players_file]") {
  const PlayersFile f = playersFileFromText(
      "format mmathEn-players 2\n"   // 1
      "player violin A\n"            // 2
      "vibratoCents 10\n"            // 3
      "vibratoCents 12\n"            // 4
      "vibratoHz 900\n"              // 5
      "player violin B\n"            // 6
      "end\n"                        // 7
      "player violin A\n"            // 8
      "end\n"                        // 9
      "player cello C\n");           // 10
  REQUIRE(f.players.size() == 3);
  CHECK(f.players[0].values.at("vibratoCents") == 12.0);
  CHECK(f.players[0].values.at("vibratoHz") == 8.0);
  CHECK(hasWarning(f, 4, "set twice (line 3)"));
  CHECK(hasWarning(f, 5, "outside the safe range"));
  CHECK(hasWarning(f, 2, "no \"end\""));
  CHECK(hasWarning(f, 8, "same instrument and name"));
  CHECK(hasWarning(f, 10, "ends before"));
  CHECK(f.keptText == std::vector<std::string>{"player violin A", "end"});
}
