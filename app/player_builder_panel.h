#pragma once
// Step 23: the Player Builder window. Pick an instrument, shape how the
// player plays it with plain-English sliders (grouped; each with a tooltip,
// where its default comes from, and a Reset), listen to a short phrase
// rendered in the background, and keep named players in
// %APPDATA%\mmathEn\players.txt (player_builder.h). Step 24.1: a file that
// exists but can't be read blocks Save and Delete (nothing is overwritten),
// each save re-reads the file and keeps players.txt.bak, and lines the app
// can't use are kept and listed with their line numbers.
// Step 24.2: the saved-players logic lives in player_library.h (tested);
// this panel adds the confirmations: unsaved changes before a load or
// quitting, replacing a saved player, import name clashes; Revert to saved;
// Export / Import (.mmplayer) and Open folder; help on keyboard focus.
//
// The panel owns its render thread. The app takes finished audio from
// takeAudio() and plays it through its normal transport; wantStop() asks it
// to stop playback.

#include "desktop_style.h"
#include "player_audition.h"
#include "player_library.h"

#include <mmathen/instruments/player_builder.h>

#include <SDL3/SDL.h>
#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace app::builder {

struct PlayRequest {
  std::vector<double> mono;  // 48 kHz, already levelled for playback
  std::string name;
};

class Panel {
 public:
  Panel() = default;
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;
  ~Panel() { stopWorker(); }

  // The window file dialogs belong to (set by the app).
  SDL_Window* window = nullptr;

  // Another saved-players file (set before the first draw).
  void setPlayersPath(const std::filesystem::path& p) { lib_.setPath(p); }
  void select(PlayerInstrument i) { lib_.select(i); }
  [[nodiscard]] bool busy() const { return job_ != nullptr; }
  [[nodiscard]] std::optional<PlayRequest> takeAudio() { return std::exchange(ready_, std::nullopt); }
  [[nodiscard]] bool wantStop() { return std::exchange(stopPlayback_, false); }
  // Starts a render as the Play button does (for --builder captures).
  void play(bool compare = false) { start(compare); }
  // Loads a saved player by "instrument:name", or by name alone when only
  // one player has it (for --builder-load captures). On failure the reason
  // is in loadError().
  bool loadByName(const std::string& spec) {
    lib_.ensureLoaded();
    const int k = mmathen::findPlayer(lib_.players(), spec, loadError_);
    if (k < 0) return false;
    savedSel_ = k;
    loadNow(k);
    return true;
  }
  [[nodiscard]] const std::string& loadError() const { return loadError_; }

  // Quitting: true when nothing is unsaved (quit now). Otherwise the panel
  // asks (the app shows the builder) and quitApproved() turns true once
  // the user chooses to quit without saving.
  bool requestQuit() {
    if (!lib_.anyUnsaved() || quitOk_) return true;
    askQuit_ = true;
    return false;
  }
  [[nodiscard]] bool quitApproved() { return std::exchange(quitNow_, false); }
  [[nodiscard]] bool anyUnsaved() const { return lib_.anyUnsaved(); }

  void draw() {
    poll();
    takeDialogResults();
    lib_.ensureLoaded();
    (void)lib_.values();
    const float u = ui::unit();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float leftW = std::clamp(avail.x * 0.18f, 200.0f * u, 260.0f * u);
    const float rightW = std::clamp(avail.x * 0.27f, 300.0f * u, 400.0f * u);
    const float midW = std::max(300.0f * u, avail.x - leftW - rightW - 2 * gap);

    ImGui::BeginChild("pb-instruments", ImVec2(leftW, avail.y), ImGuiChildFlags_Borders);
    drawInstrumentList();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("pb-sliders", ImVec2(midW, avail.y), ImGuiChildFlags_Borders);
    drawSliders();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("pb-right", ImVec2(0, avail.y));
    const float listenH = 250.0f * u;
    ImGui::BeginChild("pb-listen", ImVec2(0, listenH), ImGuiChildFlags_Borders);
    drawListen();
    ImGui::EndChild();
    ImGui::BeginChild("pb-saved", ImVec2(0, 0), ImGuiChildFlags_Borders);
    drawSaved();
    ImGui::EndChild();
    ImGui::EndChild();
    drawQuitPrompt();
  }

 private:
  struct Job {
    std::thread thread;
    std::atomic<bool> cancel{false}, done{false};
    std::atomic<float> progress{0.0f}, progress2{0.0f};  // built-in render, then yours
    bool compare = false;
    PlayerInstrument instrument{};
    audition::Phrase phrase{};
    mmathen::PlayerRig custom, standard;
    bool needStandard = false;
    audition::Audition standardOut, customOut;
    std::string label;
  };
  struct CacheKey {
    PlayerInstrument i;
    audition::Phrase p;
    bool operator<(const CacheKey& o) const {
      return std::make_pair(static_cast<int>(i), static_cast<int>(p)) < std::make_pair(static_cast<int>(o.i), static_cast<int>(o.p));
    }
  };
  enum class DialogKind { Export, Import };

  PlayerLibrary lib_;
  audition::Phrase phrase_ = audition::Phrase::Tune;
  std::unique_ptr<Job> job_;
  std::map<CacheKey, std::vector<double>> standard_;  // the built-in player's render, per phrase
  std::optional<PlayRequest> ready_;
  bool stopPlayback_ = false;
  std::string status_ = "Pick an instrument, move a slider, then press Play.";
  bool statusWarn_ = false;
  int savedSel_ = -1;
  char name_[256] = "My player";  // kPlayerNameMaxChars UTF-8 characters fit
  std::string pendingDelete_;
  std::string loadError_;
  int pendingLoad_ = -1;          // a load waiting for "discard unsaved changes?"
  bool askLoad_ = false, askOverwrite_ = false, askQuit_ = false, quitOk_ = false, quitNow_ = false;
  std::string pendingSaveName_;
  // File dialogs answer on another thread.
  std::mutex dialogMu_;
  std::vector<std::pair<DialogKind, std::string>> dialogResults_;
  int exportIndex_ = -1;
  PlayerLibrary::ImportFile importing_;
  bool askImport_ = false;

  // --- 1. instrument list ---------------------------------------------------
  void drawInstrumentList() {
    ui::heading("1. Pick an instrument", 19);
    ImGui::Spacing();
    std::string category;
    for (const auto& info : mmathen::playerInstruments()) {
      if (category != info.category) {
        category = info.category;
        ImGui::Spacing();
        ui::caption(info.category);
      }
      const bool sel = info.id == lib_.instrument();
      const bool unsaved = lib_.unsaved(info.id);
      std::string label = std::string("  ") + info.label;
      if (unsaved) label += "  *";
      label += "##pbi";
      label += info.key;
      // Switching keeps every instrument's edits (marked *), so nothing
      // is lost and no question is needed.
      if (ImGui::Selectable(label.c_str(), sel)) lib_.select(info.id);
      if (unsaved && (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)))
        ImGui::SetTooltip("Changed and not saved yet. The changes stay while you look at other instruments.");
    }
  }

  // --- 2. sliders -------------------------------------------------------------
  static const char* sourceText(mmathen::FieldSource s) {
    using S = mmathen::FieldSource;
    switch (s) {
      case S::Measured: return "measured";
      case S::Verify: return "from research";
      case S::Tuned: return "tuned by ear";
      default: return "best guess";
    }
  }
  static ImVec4 sourceColor(mmathen::FieldSource s) {
    using S = mmathen::FieldSource;
    switch (s) {
      case S::Measured: return ImVec4(.35f, .85f, .55f, 1);
      case S::Verify: return ImVec4(.95f, .78f, .35f, 1);
      case S::Tuned: return ImVec4(.45f, .70f, 1.0f, 1);
      default: return ui::muted;
    }
  }
  static std::string shown(const PlayerField& f, double stored) {
    char buf[64];
    if (f.toggle) return stored >= 0.5 ? "on" : "off";
    const double x = stored * f.displayScale;
    const int dp = displayDecimals(f);
    std::snprintf(buf, sizeof buf, "%.*f", dp, x);
    std::string s = buf;
    if (std::string(f.unit) == "x") return s + "x";
    if (f.unit[0]) s += std::string(" ") + f.unit;
    return s;
  }
  static std::string sliderFormat(const PlayerField& f) {
    const std::string unit = f.unit;
    const std::string fmt = "%." + std::to_string(displayDecimals(f)) + "f";
    if (unit == "%") return fmt + " %%";
    if (unit == "x") return fmt + "x";
    if (!unit.empty()) return fmt + " " + unit;
    return fmt;
  }
  // Help for a setting: on mouse hover, and on keyboard focus (Tab / arrows).
  static bool wantsHelp() { return ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible); }

  void drawField(const PlayerField& f, float labelW, float sliderW) {
    const float u = ui::unit();
    const double stored = lib_.values()[f.key];
    const bool changed = stored != f.defaultValue;
    ImGui::PushID(f.key);
    ImGui::AlignTextToFramePadding();
    if (changed) ImGui::TextColored(ui::accent, "%s", f.label);
    else ImGui::TextUnformatted(f.label);
    const bool hoverLabel = ImGui::IsItemHovered();
    ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(sliderW);
    if (f.toggle) {
      bool on = stored >= 0.5;
      if (ImGui::Checkbox(on ? "On" : "Off", &on)) lib_.setValue(f, on ? 1.0 : 0.0);
    } else if (f.integer) {
      int x = static_cast<int>(std::llround(stored));
      if (ImGui::SliderInt("##v", &x, static_cast<int>(f.minValue), static_cast<int>(f.maxValue), "%d", ImGuiSliderFlags_AlwaysClamp))
        lib_.setValue(f, x);
    } else {
      float x = static_cast<float>(stored * f.displayScale);
      const std::string fmt = sliderFormat(f);
      if (ImGui::SliderFloat("##v", &x, static_cast<float>(f.minValue * f.displayScale), static_cast<float>(f.maxValue * f.displayScale),
                             fmt.c_str(), ImGuiSliderFlags_AlwaysClamp))
        lib_.setValue(f, static_cast<double>(x) / f.displayScale);  // snapped: back on the default reads as unchanged
    }
    const bool helpSlider = wantsHelp();
    ImGui::SameLine();
    ImGui::TextColored(sourceColor(f.source), "%s", sourceText(f.source));
    const bool hoverSource = ImGui::IsItemHovered();
    ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 62.0f * u);
    ImGui::BeginDisabled(!changed);
    if (ImGui::Button("Reset", ImVec2(62.0f * u, 0))) lib_.resetField(f);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("Back to the built-in player's %s.", shown(f, f.defaultValue).c_str());
    if (hoverLabel || helpSlider || hoverSource) {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
      ImGui::TextUnformatted(f.tooltip);
      ImGui::TextColored(ui::muted, "Built-in player: %s   Safe range: %s to %s", shown(f, f.defaultValue).c_str(),
                         shown(f, f.minValue).c_str(), shown(f, f.maxValue).c_str());
      ImGui::TextColored(sourceColor(f.source), "Where the default comes from (%s): %s", sourceText(f.source), f.citation);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
    ImGui::PopID();
  }

  void drawSliders() {
    const float u = ui::unit();
    const PlayerInstrument inst = lib_.instrument();
    const auto& info = mmathen::playerInstrumentInfo(inst);
    ui::heading("2. Shape the player", 19);
    const int changed = lib_.changedFromBuiltIn(inst);
    const auto saved = lib_.savedName(inst);
    const float bw = 96.0f * u, bw2 = 128.0f * u;
    ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - bw - bw2 - ImGui::GetStyle().ItemSpacing.x);
    ImGui::BeginDisabled(!lib_.canRevert());
    if (ImGui::Button("Revert to saved", ImVec2(bw2, 0))) lib_.revertToSaved();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("%s", saved ? ("Put every slider back to the saved player \"" + *saved + "\".").c_str()
                                    : "Load or save a player first; then this undoes changes made since.");
    ImGui::SameLine();
    ImGui::BeginDisabled(changed == 0);
    if (ImGui::Button("Reset all", ImVec2(bw, 0))) lib_.resetToBuiltIn();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("Put every slider back to the built-in player.");
    std::string line = std::string(info.label) + (saved ? ", editing your saved player \"" + *saved + "\"" : std::string(", starting from the built-in \"") +
                                                              mmathen::builtInPlayerName(mmathen::builtInPlayer(inst)) + "\" player") + ".  ";
    line += changed == 0 ? "Same as the built-in player." : std::to_string(changed) + (changed == 1 ? " setting differs" : " settings differ") + " from the built-in player.";
    if (lib_.unsaved(inst)) line += "  Not saved.";
    ui::caption(line.c_str());
    ui::caption("Hover or Tab to any setting for what it does and where its starting value comes from.");
    ImGui::Separator();
    ImGui::BeginChild("pb-fields");
    const auto fields = mmathen::playerFields(inst);
    // Groups in first-seen order.
    std::vector<std::string> groups;
    for (const auto& f : fields)
      if (std::find(groups.begin(), groups.end(), f.group) == groups.end()) groups.emplace_back(f.group);
    // Mistakes go last.
    std::stable_partition(groups.begin(), groups.end(), [](const std::string& g) { return g != "Mistakes"; });
    const float w = ImGui::GetContentRegionAvail().x;
    const float labelW = std::min(210.0f * u, w * 0.36f);
    const float sliderW = std::max(120.0f * u, w - labelW - 190.0f * u);
    for (const auto& g : groups) {
      ImGui::SetNextItemOpen(true, ImGuiCond_Once);
      if (!ImGui::CollapsingHeader(g.c_str())) continue;
      for (const auto& f : fields)
        if (g == f.group) drawField(f, labelW, sliderW);
      ImGui::Spacing();
    }
    ImGui::EndChild();
  }

  // --- 3. listen ----------------------------------------------------------------
  void drawListen() {
    const float u = ui::unit();
    ui::heading("3. Listen", 19);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Play");
    ImGui::SameLine(80.0f * u);
    ImGui::SetNextItemWidth(-1);
    const char* cur = audition::phraseLabel(lib_.instrument(), phrase_);
    if (ImGui::BeginCombo("##phrase", cur)) {
      for (audition::Phrase p : {audition::Phrase::Tune, audition::Phrase::Scale})
        if (ImGui::Selectable(audition::phraseLabel(lib_.instrument(), p), p == phrase_)) phrase_ = p;
      ImGui::EndCombo();
    }
    const float bw = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
    ImGui::BeginDisabled(busy());
    if (ImGui::Button("Play", ImVec2(bw, 0))) start(false);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Hear your player play the phrase.");
    ImGui::SameLine();
    if (ImGui::Button("Compare", ImVec2(bw, 0))) start(true);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip("Hear the built-in player first, then yours, at the same volume.");
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(bw, 0))) {
      if (job_) job_->cancel.store(true);
      stopPlayback_ = true;
    }
    if (job_) {
      const float p = job_->needStandard ? 0.5f * (job_->progress.load() + job_->progress2.load()) : job_->progress2.load();
      char buf[64];
      std::snprintf(buf, sizeof buf, "Playing it through... %.0f%%", 100.0 * static_cast<double>(p));
      ImGui::ProgressBar(p, ImVec2(-1, 0), buf);
    } else {
      ImGui::Spacing();
    }
    ImGui::PushTextWrapPos(0);
    if (statusWarn_) ImGui::TextColored(ImVec4(1, .68f, .38f, 1), "%s", status_.c_str());
    else ui::caption(status_.c_str());
    ImGui::PopTextWrapPos();
  }

  // --- 4. saved players -----------------------------------------------------------
  [[nodiscard]] bool hasSel() const { return savedSel_ >= 0 && savedSel_ < static_cast<int>(lib_.players().size()); }
  void drawSaved() {
    const float u = ui::unit();
    ui::heading("4. My players", 19);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f * u);
    ImGui::InputTextWithHint("##name", "Name this player", name_, sizeof name_);
    ImGui::SameLine();
    const std::string clean = mmathen::cleanPlayerName(name_);
    const bool canSave = lib_.canSave();
    ImGui::BeginDisabled(clean.empty() || !canSave);
    if (ImGui::Button("Save", ImVec2(-1, 0))) {
      if (lib_.nameTaken(clean)) {
        pendingSaveName_ = clean;
        askOverwrite_ = true;
      } else {
        report(lib_.save(clean));
        selectSaved(lib_.instrument(), clean);
      }
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("%s", canSave ? "Keep these settings under this name (asks before replacing one)."
                                      : "Saving is off until the players file can be read (see the note below the list).");
    const bool notice = !canSave || !lib_.warnings().empty();
    const float listH = std::max(60.0f * u, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * (notice ? 4.3f : 3.2f));
    ImGui::BeginChild("pb-list", ImVec2(0, listH), ImGuiChildFlags_FrameStyle);
    const auto& players = lib_.players();
    if (players.empty()) ui::caption("No saved players yet.");
    for (int k = 0; k < static_cast<int>(players.size()); ++k) {
      const auto& p = players[static_cast<std::size_t>(k)];
      const std::string label = p.name + "   (" + mmathen::playerInstrumentInfo(p.instrument).label + ")##pbs" + std::to_string(k);
      if (ImGui::Selectable(label.c_str(), k == savedSel_, ImGuiSelectableFlags_AllowDoubleClick)) {
        savedSel_ = k;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) askToLoad(k);
      }
    }
    ImGui::EndChild();
    const bool has = hasSel();
    const float sp = ImGui::GetStyle().ItemSpacing.x;
    const float bw = (ImGui::GetContentRegionAvail().x - sp) / 2.0f;
    ImGui::BeginDisabled(!has);
    if (ImGui::Button("Load", ImVec2(bw, 0))) askToLoad(savedSel_);
    ImGui::SameLine();
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button("Delete", ImVec2(bw, 0))) {
      pendingDelete_ = lib_.players()[static_cast<std::size_t>(savedSel_)].name;
      ImGui::OpenPopup("Delete player?");
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    const float bw3 = (ImGui::GetContentRegionAvail().x - 2 * sp) / 3.0f;
    ImGui::BeginDisabled(!has);
    if (ImGui::Button("Export...", ImVec2(bw3, 0))) startExport();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("Save the selected player as one .mmplayer file to share.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button("Import...", ImVec2(bw3, 0))) startImport();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("Add players from a .mmplayer (or players.txt) file.");
    ImGui::SameLine();
    if (ImGui::Button("Open folder", ImVec2(bw3, 0))) openFolder();
    if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible))
      ImGui::SetTooltip("Show the folder with players.txt and its backup.");
    if (ImGui::BeginPopupModal("Delete player?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Delete \"%s\"? The previous file is kept as players.txt.bak.", pendingDelete_.c_str());
      if (ImGui::Button("Delete", ImVec2(100.0f * u, 0))) {
        report(lib_.remove(savedSel_));
        savedSel_ = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Keep it", ImVec2(100.0f * u, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    drawFileNotice();
    drawLoadPrompt();
    drawOverwritePrompt();
    drawImportPrompt();
    const std::string where = "Saved in " + utf8(lib_.path());
    ui::caption(where.c_str());
  }

  // The players file's state: a red note when it can't be read (Save and
  // Delete are off, with Try again), an orange one listing the lines the
  // app kept but could not use.
  void drawFileNotice() {
    const float u = ui::unit();
    using S = mmathen::PlayersFileState;
    if (lib_.state() == S::Unreadable) {
      ImGui::TextColored(ImVec4(1, .45f, .40f, 1), "Can't read the players file, so saving is off.");
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s).\nNothing has been changed in it.", utf8(lib_.path()).c_str(), lib_.fileError().c_str());
      ImGui::SameLine();
      if (ImGui::SmallButton("Try again")) reloadList();
    } else if (lib_.state() == S::Newer) {
      ImGui::TextColored(ImVec4(1, .68f, .38f, 1), "Saved by a newer version: read-only here.");
    }
    const auto& warnings = lib_.warnings();
    if (!warnings.empty()) {
      char line[96];
      std::snprintf(line, sizeof line, "%d line%s in the file need%s a look (kept unchanged).", static_cast<int>(warnings.size()),
                    warnings.size() == 1 ? "" : "s", warnings.size() == 1 ? "s" : "");
      ImGui::TextColored(ImVec4(1, .68f, .38f, 1), "%s", line);
      ImGui::SameLine();
      if (ImGui::SmallButton("Show")) ImGui::OpenPopup("Players file problems");
    }
    if (ImGui::BeginPopupModal("Players file problems", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted(utf8(lib_.path()).c_str());
      ui::caption("Lines the app could not use. They stay in the file exactly as written; fix them in a text editor, then press Reload.");
      ImGui::BeginChild("pb-warn", ImVec2(560.0f * u, 220.0f * u), ImGuiChildFlags_FrameStyle);
      for (const auto& w : lib_.warnings()) ImGui::Text("line %d: %s", w.line, w.message.c_str());
      ImGui::EndChild();
      if (ImGui::Button("Reload", ImVec2(100.0f * u, 0))) reloadList();
      ImGui::SameLine();
      if (ImGui::Button("Close", ImVec2(100.0f * u, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
  }

  // --- confirmations ------------------------------------------------------------------
  void askToLoad(int k) {
    if (lib_.loadWouldDiscard(k)) {
      pendingLoad_ = k;
      askLoad_ = true;
    } else {
      loadNow(k);
    }
  }
  void loadNow(int k) {
    const Outcome r = lib_.load(k);
    report(r);
    if (r.ok) std::snprintf(name_, sizeof name_, "%s", lib_.players()[static_cast<std::size_t>(k)].name.c_str());
  }
  void drawLoadPrompt() {
    const float u = ui::unit();
    if (askLoad_) {
      ImGui::OpenPopup("Unsaved changes");
      askLoad_ = false;
    }
    if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      const bool ok = pendingLoad_ >= 0 && pendingLoad_ < static_cast<int>(lib_.players().size());
      if (ok) {
        const auto& p = lib_.players()[static_cast<std::size_t>(pendingLoad_)];
        ImGui::Text("Your %s changes are not saved.", mmathen::playerInstrumentInfo(p.instrument).label);
        ImGui::Text("Load \"%s\" and lose them?", p.name.c_str());
      }
      if (ImGui::Button("Load and lose changes", ImVec2(190.0f * u, 0))) {
        if (ok) loadNow(pendingLoad_);
        pendingLoad_ = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(100.0f * u, 0))) {
        pendingLoad_ = -1;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  void drawOverwritePrompt() {
    const float u = ui::unit();
    if (askOverwrite_) {
      ImGui::OpenPopup("Replace saved player?");
      askOverwrite_ = false;
    }
    if (ImGui::BeginPopupModal("Replace saved player?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("You already have a %s player called \"%s\".", mmathen::playerInstrumentInfo(lib_.instrument()).label, pendingSaveName_.c_str());
      ImGui::TextUnformatted("Replace it with these settings? (The previous file is kept as players.txt.bak.)");
      if (ImGui::Button("Replace", ImVec2(100.0f * u, 0))) {
        report(lib_.save(pendingSaveName_));
        selectSaved(lib_.instrument(), pendingSaveName_);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(100.0f * u, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
  }
  void drawImportPrompt() {
    const float u = ui::unit();
    if (askImport_) {
      ImGui::OpenPopup("Import players");
      askImport_ = false;
    }
    if (ImGui::BeginPopupModal("Import players", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("%d player%s to import.", static_cast<int>(importing_.players.size()), importing_.players.size() == 1 ? "" : "s");
      if (importing_.warnings > 0) ImGui::Text("%d line%s in that file could not be used and are left out.", importing_.warnings, importing_.warnings == 1 ? "" : "s");
      ImGui::Text("%d of them %s the same instrument and name as one of yours.", importing_.clashes, importing_.clashes == 1 ? "has" : "have");
      auto finish = [&](PlayerLibrary::Clash c) {
        report(lib_.importPlayers(importing_.players, c));
        importing_ = {};
        ImGui::CloseCurrentPopup();
      };
      if (ImGui::Button("Keep both", ImVec2(110.0f * u, 0))) finish(PlayerLibrary::Clash::KeepBoth);
      if (ImGui::IsItemHovered() || (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)) ImGui::SetTooltip("Imported ones get \" (2)\" added to the name.");
      ImGui::SameLine();
      if (ImGui::Button("Replace mine", ImVec2(110.0f * u, 0))) finish(PlayerLibrary::Clash::Replace);
      ImGui::SameLine();
      if (ImGui::Button("Skip those", ImVec2(110.0f * u, 0))) finish(PlayerLibrary::Clash::Skip);
      ImGui::SameLine();
      if (ImGui::Button("Cancel", ImVec2(90.0f * u, 0))) {
        importing_ = {};
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  void drawQuitPrompt() {
    const float u = ui::unit();
    if (askQuit_) {
      ImGui::OpenPopup("Quit without saving?");
      askQuit_ = false;
    }
    if (ImGui::BeginPopupModal("Quit without saving?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      std::string list;
      for (PlayerInstrument i : lib_.unsavedInstruments()) list += std::string(list.empty() ? "" : ", ") + mmathen::playerInstrumentInfo(i).label;
      ImGui::Text("These players have changes that are not saved: %s.", list.c_str());
      if (ImGui::Button("Quit without saving", ImVec2(170.0f * u, 0))) {
        quitOk_ = quitNow_ = true;
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Go back", ImVec2(100.0f * u, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
  }

  // --- export / import / folder ---------------------------------------------------------
  static void SDLCALL onExportChosen(void* user, const char* const* list, int) { pushDialog(user, DialogKind::Export, list); }
  static void SDLCALL onImportChosen(void* user, const char* const* list, int) { pushDialog(user, DialogKind::Import, list); }
  static void pushDialog(void* user, DialogKind kind, const char* const* list) {
    auto* self = static_cast<Panel*>(user);
    if (!list || !list[0]) return;  // error or cancelled
    std::lock_guard<std::mutex> lock(self->dialogMu_);
    self->dialogResults_.emplace_back(kind, list[0]);
  }
  void takeDialogResults() {
    std::vector<std::pair<DialogKind, std::string>> todo;
    {
      std::lock_guard<std::mutex> lock(dialogMu_);
      todo.swap(dialogResults_);
    }
    for (const auto& [kind, file] : todo) {
      std::filesystem::path p(std::u8string(reinterpret_cast<const char8_t*>(file.data()), file.size()));  // SDL gives UTF-8
      if (kind == DialogKind::Export) {
        if (p.extension().empty()) p += std::string(".") + kPlayerExportExtension;
        report(lib_.exportPlayer(exportIndex_, p));
      } else {
        importing_ = lib_.readImport(p);
        if (!importing_.ok) {
          setStatus(importing_.message, true);
        } else if (importing_.clashes == 0 && importing_.warnings == 0) {
          report(lib_.importPlayers(importing_.players, PlayerLibrary::Clash::KeepBoth));
          importing_ = {};
        } else {
          askImport_ = true;
        }
      }
    }
  }
  void startExport() {
    if (!hasSel()) return;
    exportIndex_ = savedSel_;
    static const SDL_DialogFileFilter filters[] = {{"mmathEn player", kPlayerExportExtension}};
    std::error_code ec;
    std::filesystem::create_directories(lib_.folder(), ec);
    const std::string where = utf8(lib_.folder() / exportFileName(lib_.players()[static_cast<std::size_t>(savedSel_)]));
    SDL_ShowSaveFileDialog(&Panel::onExportChosen, this, window, filters, 1, where.c_str());
  }
  void startImport() {
    static const SDL_DialogFileFilter filters[] = {{"mmathEn player", kPlayerExportExtension}, {"Players file", "txt"}};
    // A trailing separator makes the Windows dialog open inside the folder, not beside it.
    const std::string where = utf8(lib_.folder() / "");
    SDL_ShowOpenFileDialog(&Panel::onImportChosen, this, window, filters, 2, where.c_str(), false);
  }
  void openFolder() {
    std::error_code ec;
    std::filesystem::create_directories(lib_.folder(), ec);
    if (!SDL_OpenURL(fileUrl(std::filesystem::absolute(lib_.folder(), ec)).c_str()))
      setStatus("Could not open " + utf8(lib_.folder()) + ": " + SDL_GetError(), true);
  }
  // SDL and ImGui take UTF-8.
  static std::string utf8(const std::filesystem::path& p) { return mmathen::pathText(p); }

  void selectSaved(PlayerInstrument i, const std::string& name) {
    const auto& players = lib_.players();
    for (int k = 0; k < static_cast<int>(players.size()); ++k)
      if (players[static_cast<std::size_t>(k)].instrument == i && players[static_cast<std::size_t>(k)].name == name) savedSel_ = k;
  }
  void reloadList() {
    savedSel_ = -1;
    lib_.reload();
    if (lib_.state() == mmathen::PlayersFileState::Unreadable)
      setStatus("Can't read " + utf8(lib_.path()) + " (" + lib_.fileError() + "). Saving is off so nothing in it is lost.", true);
    else
      setStatus("Players file read again.", false);
  }
  void report(const Outcome& r) { setStatus(r.message, !r.ok); }
  void setStatus(std::string s, bool warn) {
    status_ = std::move(s);
    statusWarn_ = warn;
  }

  // --- background render ------------------------------------------------------------
  void start(bool compare) {
    if (job_) return;
    stopPlayback_ = true;  // stop the last phrase while the next one renders
    auto j = std::make_unique<Job>();
    j->compare = compare;
    j->instrument = lib_.instrument();
    j->phrase = phrase_;
    j->custom = mmathen::makePlayer(lib_.instrument(), lib_.values());
    j->standard = mmathen::builtInPlayer(lib_.instrument());
    j->needStandard = standard_.find(CacheKey{lib_.instrument(), phrase_}) == standard_.end();
    j->label = std::string(mmathen::playerInstrumentInfo(lib_.instrument()).label) + ", " +
               (compare ? "built-in player then yours" : (lib_.changedFromBuiltIn(lib_.instrument()) ? "your player" : "built-in player"));
    Job* raw = j.get();
    raw->thread = std::thread([raw] {
      // The built-in render (cached) sets the playback level, so a louder
      // player really sounds louder.
      if (raw->needStandard) raw->standardOut = audition::renderAudition(raw->standard, raw->phrase, &raw->cancel, &raw->progress);
      raw->customOut = audition::renderAudition(raw->custom, raw->phrase, &raw->cancel, &raw->progress2);
      raw->done.store(true);
    });
    job_ = std::move(j);
    setStatus("Playing it through (a few seconds)...", false);
  }
  void stopWorker() {
    if (!job_) return;
    job_->cancel.store(true);
    if (job_->thread.joinable()) job_->thread.join();
    job_.reset();
  }
  void poll() {
    if (!job_ || !job_->done.load()) return;
    job_->thread.join();
    std::unique_ptr<Job> j = std::move(job_);
    const CacheKey key{j->instrument, j->phrase};
    if (j->cancel.load()) {
      setStatus("Stopped.", false);
      return;
    }
    if (j->needStandard) {
      if (!j->standardOut.ok) {
        setStatus("The built-in player could not be played: " + j->standardOut.error, true);
        return;
      }
      standard_[key] = std::move(j->standardOut.mono);
    }
    if (!j->customOut.ok) {
      setStatus("These settings could not be played (" + j->customOut.error + "). Try moving the last slider back.", true);
      return;
    }
    const std::vector<double>& base = standard_[key];
    const double basePeak = audition::peakOf(base);
    double gain = basePeak > 0.0 ? 0.5 / basePeak : 1.0;
    const double customPeak = audition::peakOf(j->customOut.mono);
    bool turnedDown = false;
    if (customPeak * gain > 0.98) {
      gain = 0.98 / customPeak;
      turnedDown = true;
    }
    if (!(customPeak > 0.0)) {
      setStatus("These settings make no sound. Try moving the last slider back.", true);
      return;
    }
    PlayRequest r;
    if (j->compare) {
      r.mono = base;
      r.mono.resize(r.mono.size() + static_cast<std::size_t>(0.6 * audition::kRate), 0.0);
      r.mono.insert(r.mono.end(), j->customOut.mono.begin(), j->customOut.mono.end());
    } else {
      r.mono = std::move(j->customOut.mono);
    }
    for (double& x : r.mono) x *= gain;
    r.name = "Player builder: " + j->label;
    ready_ = std::move(r);
    setStatus(turnedDown ? "Playing. Your player is much louder, so the volume was turned down to avoid distortion."
                         : (j->compare ? "Playing: the built-in player, a short gap, then yours." : "Playing."),
              false);
  }
};

}  // namespace app::builder
