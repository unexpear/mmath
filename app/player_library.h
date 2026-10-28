#pragma once
// Step 24.2: the Player builder's saved-players logic, without any UI, so
// it can be unit-tested (tests/test_player_library.cpp). The panel
// (player_builder_panel.h) only draws and asks for confirmations.
//
//   - the edits of each instrument, and which saved player they came from
//     (so "Revert to saved" and the unsaved-changes prompts know the
//     baseline);
//   - save (overwrite needs the caller's confirmation: nameTaken), load,
//     delete, all through editPlayersFile (step 24.1: nothing lost, .bak);
//   - export one player as a .mmplayer file (a version-2 players file with
//     one player) and import one or more players with name-clash handling;
//   - slider values snapped to the default within the display precision.

#include <mmathen/instruments/player_builder.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace app::builder {

using mmathen::CustomPlayer;
using mmathen::PlayerField;
using mmathen::PlayerInstrument;
using mmathen::PlayerValues;

// Decimal places a field is shown with (the slider's own format): finer
// for narrower spans, whole numbers for integer and toggle fields.
[[nodiscard]] inline int displayDecimals(const PlayerField& f) {
  if (f.integer || f.toggle) return 0;
  const double span = (f.maxValue - f.minValue) * f.displayScale;
  return span >= 100 ? 0 : span >= 10 ? 1 : span >= 1 ? 2 : 3;
}
// A value that shows the same as the default (within half the last shown
// digit) is the default, so a slider dragged back reads as unchanged.
[[nodiscard]] inline double snapToDefault(const PlayerField& f, double stored) {
  const double halfStep = 0.5 * std::pow(10.0, -displayDecimals(f));
  return std::fabs((stored - f.defaultValue) * f.displayScale) < halfStep ? f.defaultValue : stored;
}
// A slider value kept to the digits the slider shows, so the saved file reads
// "31.3", not the float slider's 31.299999237060547; a value that shows the
// same as the default becomes the default.
[[nodiscard]] inline double toShownDigits(const PlayerField& f, double stored) {
  if (f.toggle || f.displayScale == 0.0) return snapToDefault(f, stored);
  const double unit = std::pow(10.0, displayDecimals(f));
  const double shown = std::round(stored * f.displayScale * unit) / unit;
  return snapToDefault(f, mmathen::clampFieldValue(f, shown / f.displayScale));
}

inline constexpr const char* kPlayerExportExtension = "mmplayer";

// A file name for an exported player: "<instrument>-<name>.mmplayer" with
// characters Windows does not allow in file names replaced by '_'.
[[nodiscard]] inline std::string exportFileName(const CustomPlayer& p) {
  std::string s = std::string(mmathen::playerInstrumentInfo(p.instrument).key) + "-" + mmathen::cleanPlayerName(p.name);
  for (char& c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < 32 || std::string("<>:\"/\\|?*").find(c) != std::string::npos) c = '_';
  }
  while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
  return s + "." + kPlayerExportExtension;
}
// A file:// URL for a folder (for "Open folder"), bytes outside the URL
// unreserved set percent-encoded (RFC 3986).
[[nodiscard]] inline std::string fileUrl(const std::filesystem::path& p) {
  const std::u8string u8 = p.generic_u8string();
  const std::string s(reinterpret_cast<const char*>(u8.data()), u8.size());
  std::string out = s.rfind('/', 0) == 0 ? "file://" : "file:///";
  static const char* hex = "0123456789ABCDEF";
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || std::string("-._~/:").find(c) != std::string::npos) {
      out += c;
    } else {
      out += '%';
      out += hex[u >> 4];
      out += hex[u & 15u];
    }
  }
  return out;
}

struct Outcome {
  bool ok = false;
  std::string message;  // plain English, for the status line
};

class PlayerLibrary {
 public:
  explicit PlayerLibrary(std::filesystem::path path = mmathen::defaultPlayersPath()) : path_(std::move(path)) {}

  // --- the players file ------------------------------------------------------
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }
  void setPath(std::filesystem::path p) {
    path_ = std::move(p);
    loaded_ = false;
  }
  [[nodiscard]] std::filesystem::path folder() const { return path_.has_parent_path() ? path_.parent_path() : std::filesystem::path("."); }
  void ensureLoaded() {
    if (!loaded_) reload();
  }
  void reload() {
    loaded_ = true;
    apply(mmathen::loadPlayersFile(path_));
  }
  [[nodiscard]] const std::vector<CustomPlayer>& players() const { return players_; }
  [[nodiscard]] mmathen::PlayersFileState state() const { return state_; }
  [[nodiscard]] const std::string& fileError() const { return fileError_; }
  [[nodiscard]] const std::vector<mmathen::PlayersFileWarning>& warnings() const { return warnings_; }
  [[nodiscard]] bool canSave() const { return state_ == mmathen::PlayersFileState::Missing || state_ == mmathen::PlayersFileState::Ok; }

  // --- what is being edited ------------------------------------------------------
  [[nodiscard]] PlayerInstrument instrument() const { return instrument_; }
  void select(PlayerInstrument i) {
    instrument_ = i;
    (void)values(i);
  }
  PlayerValues& values() { return values(instrument_); }
  PlayerValues& values(PlayerInstrument i) {
    auto it = values_.find(i);
    if (it == values_.end()) it = values_.emplace(i, mmathen::defaultValues(i)).first;
    return it->second;
  }
  // The saved player the instrument's edits started from (load or save),
  // if any.
  [[nodiscard]] std::optional<std::string> savedName(PlayerInstrument i) const {
    const auto it = origin_.find(i);
    if (it == origin_.end()) return std::nullopt;
    return it->second.name;
  }
  // What "unsaved" is measured against: the saved player the edits came
  // from, else the built-in player.
  [[nodiscard]] PlayerValues baseline(PlayerInstrument i) const {
    const auto it = origin_.find(i);
    return it == origin_.end() ? mmathen::defaultValues(i) : it->second.values;
  }
  [[nodiscard]] bool unsaved(PlayerInstrument i) const {
    const auto it = values_.find(i);
    return it != values_.end() && it->second != baseline(i);
  }
  [[nodiscard]] std::vector<PlayerInstrument> unsavedInstruments() const {
    std::vector<PlayerInstrument> out;
    for (const auto& [i, v] : values_)
      if (unsaved(i)) out.push_back(i);
    return out;
  }
  [[nodiscard]] bool anyUnsaved() const { return !unsavedInstruments().empty(); }
  // Settings that differ from the built-in player.
  [[nodiscard]] int changedFromBuiltIn(PlayerInstrument i) const {
    const auto it = values_.find(i);
    if (it == values_.end()) return 0;
    const PlayerValues d = mmathen::defaultValues(i);
    int n = 0;
    for (const auto& [k, x] : it->second) {
      const auto dk = d.find(k);
      if (dk != d.end() && dk->second != x) ++n;
    }
    return n;
  }
  // Sets one setting, clamped to its safe range and snapped to the default.
  void setValue(const PlayerField& f, double x) { values()[f.key] = toShownDigits(f, mmathen::clampFieldValue(f, x)); }
  void resetField(const PlayerField& f) { values()[f.key] = f.defaultValue; }
  void resetToBuiltIn() { values() = mmathen::defaultValues(instrument_); }
  [[nodiscard]] bool canRevert() const { return origin_.count(instrument_) != 0 && unsaved(instrument_); }
  void revertToSaved() {
    const auto it = origin_.find(instrument_);
    if (it != origin_.end()) values() = it->second.values;
  }

  // --- save, load, delete ------------------------------------------------------------
  // True when saving under this name would replace a saved player (ask first).
  [[nodiscard]] bool nameTaken(const std::string& name) const { return indexOf(instrument_, mmathen::cleanPlayerName(name)) >= 0; }
  Outcome save(const std::string& name) {
    CustomPlayer p;
    p.name = mmathen::cleanPlayerName(name);
    p.instrument = instrument_;
    p.values = mmathen::clampValues(instrument_, values());
    if (p.name.empty()) return {false, "Give the player a name first."};
    auto r = mmathen::editPlayersFile(path_, [&](std::vector<CustomPlayer>& list) { mmathen::upsertPlayer(list, p); });
    apply(std::move(r.after));
    if (!r.ok) return {false, r.error};
    origin_[instrument_] = p;
    values() = p.values;
    return {true, "Saved \"" + p.name + "\"."};
  }
  // True when loading player k would throw away unsaved edits.
  [[nodiscard]] bool loadWouldDiscard(int k) const {
    return valid(k) && unsaved(players_[static_cast<std::size_t>(k)].instrument);
  }
  Outcome load(int k) {
    if (!valid(k)) return {false, "Pick a saved player first."};
    const CustomPlayer& p = players_[static_cast<std::size_t>(k)];
    instrument_ = p.instrument;
    values_[p.instrument] = mmathen::clampValues(p.instrument, p.values);
    origin_[p.instrument] = p;
    origin_[p.instrument].values = values_[p.instrument];
    return {true, "Loaded \"" + p.name + "\" (" + mmathen::playerInstrumentInfo(p.instrument).label + ")."};
  }
  Outcome remove(int k) {
    if (!valid(k)) return {false, "Pick a saved player first."};
    const CustomPlayer gone = players_[static_cast<std::size_t>(k)];
    auto r = mmathen::editPlayersFile(path_, [&](std::vector<CustomPlayer>& list) { (void)mmathen::removePlayer(list, gone.instrument, gone.name); });
    apply(std::move(r.after));
    if (!r.ok) return {false, r.error};
    const auto it = origin_.find(gone.instrument);
    if (it != origin_.end() && it->second.name == gone.name) origin_.erase(it);  // its edits are no longer saved anywhere
    return {true, "Deleted \"" + gone.name + "\"."};
  }

  // --- export / import ---------------------------------------------------------------------
  Outcome exportPlayer(int k, const std::filesystem::path& file) const {
    if (!valid(k)) return {false, "Pick a saved player to export."};
    mmathen::PlayersFile one;
    one.players.push_back(players_[static_cast<std::size_t>(k)]);
    std::string why;
    if (!mmathen::player_builder_detail::writeFileAtomic(file, mmathen::playersToText(one), &why)) return {false, "Not exported: " + why + "."};
    return {true, "Exported \"" + one.players[0].name + "\" to " + mmathen::pathText(file) + "."};
  }
  struct ImportFile {
    bool ok = false;
    std::string message;
    std::vector<CustomPlayer> players;
    int clashes = 0;  // players with the same instrument and name as a saved one
    int warnings = 0;
  };
  [[nodiscard]] ImportFile readImport(const std::filesystem::path& file) const {
    ImportFile r;
    const mmathen::PlayersLoad l = mmathen::loadPlayersFile(file);
    if (l.state == mmathen::PlayersFileState::Missing) {
      r.message = "Not imported: " + mmathen::pathText(file) + " does not exist.";
      return r;
    }
    if (l.state == mmathen::PlayersFileState::Unreadable) {
      r.message = "Not imported: " + mmathen::pathText(file) + " can't be read (" + l.error + ").";
      return r;
    }
    if (l.file.players.empty()) {
      r.message = "Not imported: no players found in " + mmathen::pathText(file) + ".";
      return r;
    }
    r.ok = true;
    r.players = l.file.players;
    r.warnings = static_cast<int>(l.file.warnings.size());
    for (const CustomPlayer& p : r.players)
      if (indexOf(p.instrument, p.name) >= 0) ++r.clashes;
    return r;
  }
  enum class Clash { Replace, KeepBoth, Skip };
  Outcome importPlayers(const std::vector<CustomPlayer>& incoming, Clash clash) {
    int added = 0, replaced = 0, skipped = 0;
    auto r = mmathen::editPlayersFile(path_, [&](std::vector<CustomPlayer>& list) {
      for (CustomPlayer p : incoming) {
        const bool taken = std::any_of(list.begin(), list.end(), [&](const CustomPlayer& q) { return q.instrument == p.instrument && q.name == p.name; });
        if (taken && clash == Clash::Skip) { ++skipped; continue; }
        if (taken && clash == Clash::KeepBoth) p.name = freeName(list, p);
        if (taken && clash == Clash::Replace) ++replaced;
        else ++added;
        mmathen::upsertPlayer(list, p);
      }
    });
    apply(std::move(r.after));
    if (!r.ok) return {false, r.error};
    std::string msg = "Imported " + std::to_string(added) + " player" + (added == 1 ? "" : "s");
    if (replaced) msg += ", replaced " + std::to_string(replaced);
    if (skipped) msg += ", skipped " + std::to_string(skipped);
    return {true, msg + "."};
  }

 private:
  std::filesystem::path path_;
  bool loaded_ = false;
  std::vector<CustomPlayer> players_;
  mmathen::PlayersFileState state_ = mmathen::PlayersFileState::Missing;
  std::string fileError_;
  std::vector<mmathen::PlayersFileWarning> warnings_;
  PlayerInstrument instrument_ = PlayerInstrument::Violin;
  std::map<PlayerInstrument, PlayerValues> values_;
  std::map<PlayerInstrument, CustomPlayer> origin_;

  void apply(mmathen::PlayersLoad l) {
    players_ = std::move(l.file.players);
    state_ = l.state;
    fileError_ = std::move(l.error);
    warnings_ = std::move(l.file.warnings);
  }
  [[nodiscard]] bool valid(int k) const { return k >= 0 && k < static_cast<int>(players_.size()); }
  [[nodiscard]] int indexOf(PlayerInstrument i, const std::string& name) const {
    for (std::size_t k = 0; k < players_.size(); ++k)
      if (players_[k].instrument == i && players_[k].name == name) return static_cast<int>(k);
    return -1;
  }
  // "Name (2)", "Name (3)", ...: the first not taken, cut so it stays
  // within kPlayerNameMaxChars.
  static std::string freeName(const std::vector<CustomPlayer>& list, const CustomPlayer& p) {
    for (int n = 2;; ++n) {
      const std::string suffix = " (" + std::to_string(n) + ")";
      std::string base = p.name;
      while (!base.empty() && mmathen::cleanPlayerName(base + suffix) != base + suffix) {
        base.pop_back();
        while (!base.empty() && (static_cast<unsigned char>(base.back()) & 0xC0u) == 0x80u) base.pop_back();
        if (!base.empty() && static_cast<unsigned char>(base.back()) >= 0xC0u) base.pop_back();
      }
      const std::string candidate = mmathen::cleanPlayerName(base + suffix);
      if (std::none_of(list.begin(), list.end(), [&](const CustomPlayer& q) { return q.instrument == p.instrument && q.name == candidate; }))
        return candidate;
    }
  }
};

}  // namespace app::builder
