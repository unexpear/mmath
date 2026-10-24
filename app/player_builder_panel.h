#pragma once
// Step 23: the Player Builder window. Pick an instrument, shape how the
// player plays it with plain-English sliders (grouped; each with a tooltip,
// where its default comes from, and a Reset), listen to a short phrase
// rendered in the background, and keep named players in
// %APPDATA%\mmathEn\players.txt (player_builder.h).
//
// The panel owns its render thread. The app takes finished audio from
// takeAudio() and plays it through its normal transport; wantStop() asks it
// to stop playback.

#include "desktop_style.h"
#include "player_audition.h"

#include <mmathen/instruments/player_builder.h>

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace app::builder {

using mmathen::CustomPlayer;
using mmathen::PlayerField;
using mmathen::PlayerInstrument;
using mmathen::PlayerValues;

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

  // Where saved players live (set before the first draw to use another file).
  std::filesystem::path playersPath = mmathen::defaultPlayersPath();

  void select(PlayerInstrument i) {
    instrument_ = i;
    ensureValues();
  }
  [[nodiscard]] bool busy() const { return job_ != nullptr; }
  [[nodiscard]] std::optional<PlayRequest> takeAudio() { return std::exchange(ready_, std::nullopt); }
  [[nodiscard]] bool wantStop() { return std::exchange(stopPlayback_, false); }
  // Starts a render as the Play button does (for --builder captures).
  void play(bool compare = false) { start(compare); }
  // Loads a saved player by name (for --builder-load captures).
  bool loadByName(const std::string& name) {
    if (!loaded_) loadList();
    for (int k = 0; k < static_cast<int>(saved_.size()); ++k)
      if (saved_[static_cast<std::size_t>(k)].name == name) {
        savedSel_ = k;
        loadPlayer(k);
        return true;
      }
    return false;
  }

  void draw() {
    poll();
    if (!loaded_) loadList();
    ensureValues();
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

  PlayerInstrument instrument_ = PlayerInstrument::Violin;
  std::map<PlayerInstrument, PlayerValues> values_;  // edits kept per instrument
  audition::Phrase phrase_ = audition::Phrase::Tune;
  std::unique_ptr<Job> job_;
  std::map<CacheKey, std::vector<double>> standard_;  // the built-in player's render, per phrase
  std::optional<PlayRequest> ready_;
  bool stopPlayback_ = false;
  std::string status_ = "Pick an instrument, move a slider, then press Play.";
  bool statusWarn_ = false;
  std::vector<CustomPlayer> saved_;
  bool loaded_ = false;
  int savedSel_ = -1;
  char name_[64] = "My player";
  std::string pendingDelete_;

  PlayerValues& values() { return values_[instrument_]; }
  void ensureValues() {
    if (values_.find(instrument_) == values_.end()) values_[instrument_] = mmathen::defaultValues(instrument_);
  }
  [[nodiscard]] int changedCount() {
    const PlayerValues d = mmathen::defaultValues(instrument_);
    int n = 0;
    for (const auto& [k, x] : values()) {
      const auto it = d.find(k);
      if (it != d.end() && it->second != x) ++n;
    }
    return n;
  }

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
      const bool sel = info.id == instrument_;
      std::string label = std::string("  ") + info.label;
      const auto it = values_.find(info.id);
      if (it != values_.end() && it->second != mmathen::defaultValues(info.id)) label += "  *";
      label += "##pbi";
      label += info.key;
      if (ImGui::Selectable(label.c_str(), sel)) select(info.id);
      if (ImGui::IsItemHovered() && it != values_.end() && it->second != mmathen::defaultValues(info.id))
        ImGui::SetTooltip("You have changed this player (not saved yet).");
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
    const double span = (f.maxValue - f.minValue) * f.displayScale;
    const int dp = f.integer ? 0 : span >= 100 ? 0 : span >= 10 ? 1 : span >= 1 ? 2 : 3;
    std::snprintf(buf, sizeof buf, "%.*f", dp, x);
    std::string s = buf;
    if (std::string(f.unit) == "x") return s + "x";
    if (f.unit[0]) s += std::string(" ") + f.unit;
    return s;
  }
  static std::string sliderFormat(const PlayerField& f) {
    const double span = (f.maxValue - f.minValue) * f.displayScale;
    const int dp = span >= 100 ? 0 : span >= 10 ? 1 : span >= 1 ? 2 : 3;
    std::string unit = f.unit;
    std::string fmt = "%." + std::to_string(dp) + "f";
    if (unit == "%") return fmt + " %%";
    if (unit == "x") return fmt + "x";
    if (!unit.empty()) return fmt + " " + unit;
    return fmt;
  }

  void drawField(const PlayerField& f, float labelW, float sliderW) {
    const float u = ui::unit();
    PlayerValues& v = values();
    double& stored = v[f.key];
    const bool changed = stored != f.defaultValue;
    ImGui::PushID(f.key);
    ImGui::AlignTextToFramePadding();
    if (changed) ImGui::TextColored(ui::accent, "%s", f.label);
    else ImGui::TextUnformatted(f.label);
    const bool hoverLabel = ImGui::IsItemHovered();
    ImGui::SameLine(labelW);
    ImGui::SetNextItemWidth(sliderW);
    bool edited = false;
    if (f.toggle) {
      bool on = stored >= 0.5;
      if (ImGui::Checkbox(on ? "On" : "Off", &on)) { stored = on ? 1.0 : 0.0; edited = true; }
    } else if (f.integer) {
      int x = static_cast<int>(std::llround(stored));
      if (ImGui::SliderInt("##v", &x, static_cast<int>(f.minValue), static_cast<int>(f.maxValue), "%d", ImGuiSliderFlags_AlwaysClamp)) {
        stored = x;
        edited = true;
      }
    } else {
      float x = static_cast<float>(stored * f.displayScale);
      const std::string fmt = sliderFormat(f);
      if (ImGui::SliderFloat("##v", &x, static_cast<float>(f.minValue * f.displayScale), static_cast<float>(f.maxValue * f.displayScale),
                             fmt.c_str(), ImGuiSliderFlags_AlwaysClamp)) {
        stored = static_cast<double>(x) / f.displayScale;
        edited = true;
      }
    }
    const bool hoverSlider = ImGui::IsItemHovered();
    if (edited) stored = mmathen::clampFieldValue(f, stored);
    ImGui::SameLine();
    ImGui::TextColored(sourceColor(f.source), "%s", sourceText(f.source));
    const bool hoverSource = ImGui::IsItemHovered();
    ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 62.0f * u);
    ImGui::BeginDisabled(!changed);
    if (ImGui::Button("Reset", ImVec2(62.0f * u, 0))) stored = f.defaultValue;
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip("Back to the built-in player's %s.", shown(f, f.defaultValue).c_str());
    if (hoverLabel || hoverSlider || hoverSource) {
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
    const auto& info = mmathen::playerInstrumentInfo(instrument_);
    ui::heading("2. Shape the player", 19);
    ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 96.0f * u);
    const int changed = changedCount();
    ImGui::BeginDisabled(changed == 0);
    if (ImGui::Button("Reset all", ImVec2(96.0f * u, 0))) values() = mmathen::defaultValues(instrument_);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Put every slider back to the built-in player.");
    char line[200];
    std::snprintf(line, sizeof line, "%s, starting from the built-in \"%s\" player.  %s", info.label,
                  mmathen::builtInPlayerName(mmathen::builtInPlayer(instrument_)),
                  changed == 0 ? "Nothing changed yet." : (std::to_string(changed) + (changed == 1 ? " setting changed." : " settings changed.")).c_str());
    ui::caption(line);
    ui::caption("Hover any setting for what it does and where its starting value comes from.");
    ImGui::Separator();
    ImGui::BeginChild("pb-fields");
    const auto fields = mmathen::playerFields(instrument_);
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
    const char* cur = audition::phraseLabel(instrument_, phrase_);
    if (ImGui::BeginCombo("##phrase", cur)) {
      for (audition::Phrase p : {audition::Phrase::Tune, audition::Phrase::Scale})
        if (ImGui::Selectable(audition::phraseLabel(instrument_, p), p == phrase_)) phrase_ = p;
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
  void drawSaved() {
    const float u = ui::unit();
    ui::heading("4. My players", 19);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f * u);
    ImGui::InputTextWithHint("##name", "Name this player", name_, sizeof name_);
    ImGui::SameLine();
    const std::string clean = mmathen::cleanPlayerName(name_);
    ImGui::BeginDisabled(clean.empty());
    if (ImGui::Button("Save", ImVec2(-1, 0))) savePlayer(clean);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ImGui::SetTooltip("Keep these settings under this name (the same name replaces it).");
    const float listH = std::max(60.0f * u, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.2f);
    ImGui::BeginChild("pb-list", ImVec2(0, listH), ImGuiChildFlags_FrameStyle);
    if (saved_.empty()) ui::caption("No saved players yet.");
    for (int k = 0; k < static_cast<int>(saved_.size()); ++k) {
      const auto& p = saved_[static_cast<std::size_t>(k)];
      const std::string label = p.name + "   (" + mmathen::playerInstrumentInfo(p.instrument).label + ")##pbs" + std::to_string(k);
      if (ImGui::Selectable(label.c_str(), k == savedSel_, ImGuiSelectableFlags_AllowDoubleClick)) {
        savedSel_ = k;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) loadPlayer(k);
      }
    }
    ImGui::EndChild();
    const bool has = savedSel_ >= 0 && savedSel_ < static_cast<int>(saved_.size());
    const float bw = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    ImGui::BeginDisabled(!has);
    if (ImGui::Button("Load", ImVec2(bw, 0))) loadPlayer(savedSel_);
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(bw, 0))) {
      pendingDelete_ = saved_[static_cast<std::size_t>(savedSel_)].name;
      ImGui::OpenPopup("Delete player?");
    }
    ImGui::EndDisabled();
    if (ImGui::BeginPopupModal("Delete player?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Delete \"%s\"? This cannot be undone.", pendingDelete_.c_str());
      if (ImGui::Button("Delete", ImVec2(100.0f * u, 0))) {
        deletePlayer(savedSel_);
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Keep it", ImVec2(100.0f * u, 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    const std::string where = "Saved in " + playersPath.string();
    ui::caption(where.c_str());
  }

  void loadList() {
    loaded_ = true;
    saved_ = mmathen::loadPlayers(playersPath);
  }
  void savePlayer(const std::string& name) {
    CustomPlayer p;
    p.name = name;
    p.instrument = instrument_;
    p.values = values();
    std::vector<CustomPlayer> next = saved_;
    mmathen::upsertPlayer(next, p);
    if (!mmathen::savePlayers(playersPath, next)) {
      setStatus("Could not save to " + playersPath.string(), true);
      return;
    }
    saved_ = std::move(next);
    for (int k = 0; k < static_cast<int>(saved_.size()); ++k)
      if (saved_[static_cast<std::size_t>(k)].name == p.name && saved_[static_cast<std::size_t>(k)].instrument == p.instrument) savedSel_ = k;
    setStatus("Saved \"" + p.name + "\".", false);
  }
  void loadPlayer(int k) {
    if (k < 0 || k >= static_cast<int>(saved_.size())) return;
    const CustomPlayer& p = saved_[static_cast<std::size_t>(k)];
    instrument_ = p.instrument;
    values_[instrument_] = mmathen::clampValues(p.instrument, p.values);
    std::snprintf(name_, sizeof name_, "%s", p.name.c_str());
    setStatus("Loaded \"" + p.name + "\" (" + mmathen::playerInstrumentInfo(p.instrument).label + ").", false);
  }
  void deletePlayer(int k) {
    if (k < 0 || k >= static_cast<int>(saved_.size())) return;
    std::vector<CustomPlayer> next = saved_;
    const CustomPlayer gone = next[static_cast<std::size_t>(k)];
    (void)mmathen::removePlayer(next, gone.instrument, gone.name);
    if (!mmathen::savePlayers(playersPath, next)) {
      setStatus("Could not update " + playersPath.string(), true);
      return;
    }
    saved_ = std::move(next);
    savedSel_ = -1;
    setStatus("Deleted \"" + gone.name + "\".", false);
  }
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
    j->instrument = instrument_;
    j->phrase = phrase_;
    j->custom = mmathen::makePlayer(instrument_, values());
    j->standard = mmathen::builtInPlayer(instrument_);
    j->needStandard = standard_.find(CacheKey{instrument_, phrase_}) == standard_.end();
    j->label = std::string(mmathen::playerInstrumentInfo(instrument_).label) + ", " +
               (compare ? "built-in player then yours" : (changedCount() ? "your player" : "built-in player"));
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
