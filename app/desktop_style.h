#pragma once
#include <imgui.h>

namespace app::ui {
inline const ImVec4 accent{.35f, .85f, .73f, 1};
inline const ImVec4 muted{.58f, .66f, .73f, 1};
inline float unit() { return ImGui::GetFontSize() / 16.0f; }
inline void heading(const char* text, float size = 24) {
  ImGui::PushFont(nullptr, size);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}
inline void caption(const char* text) { ImGui::TextColored(muted, "%s", text); }
inline bool button(const char* text, bool selected, ImVec2 size = {}) {
  if (selected) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.16f, .38f, .36f, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
  }
  const bool clicked = ImGui::Button(text, size);
  if (selected) ImGui::PopStyleColor(2);
  return clicked;
}
inline void theme(float dpi) {
  ImGui::StyleColorsDark();
  auto& s = ImGui::GetStyle();
  s.WindowPadding = {20, 18}; s.FramePadding = {12, 7}; s.ItemSpacing = {10, 10};
  s.ChildRounding = 12; s.FrameRounding = 6; s.PopupRounding = 8;
  s.GrabRounding = 5; s.TabRounding = 6; s.ScrollbarRounding = 6;
  s.WindowBorderSize = 0; s.ChildBorderSize = 1; s.FrameBorderSize = 0;
  s.Colors[ImGuiCol_WindowBg] = {.045f, .061f, .082f, 1};
  s.Colors[ImGuiCol_ChildBg] = {.065f, .09f, .12f, 1};
  s.Colors[ImGuiCol_PopupBg] = {.08f, .11f, .15f, 1};
  s.Colors[ImGuiCol_Border] = {.15f, .20f, .25f, 1};
  s.Colors[ImGuiCol_Text] = {.90f, .94f, .97f, 1};
  s.Colors[ImGuiCol_TextDisabled] = muted;
  s.Colors[ImGuiCol_FrameBg] = {.105f, .145f, .19f, 1};
  s.Colors[ImGuiCol_FrameBgHovered] = {.15f, .23f, .28f, 1};
  s.Colors[ImGuiCol_FrameBgActive] = {.18f, .30f, .32f, 1};
  s.Colors[ImGuiCol_Button] = {.12f, .18f, .23f, 1};
  s.Colors[ImGuiCol_ButtonHovered] = {.18f, .30f, .35f, 1};
  s.Colors[ImGuiCol_ButtonActive] = {.20f, .40f, .39f, 1};
  s.Colors[ImGuiCol_Header] = {.10f, .17f, .22f, 1};
  s.Colors[ImGuiCol_HeaderHovered] = {.17f, .29f, .33f, 1};
  s.Colors[ImGuiCol_HeaderActive] = {.18f, .36f, .36f, 1};
  s.Colors[ImGuiCol_CheckMark] = accent;
  s.Colors[ImGuiCol_SliderGrab] = accent;
  s.Colors[ImGuiCol_SliderGrabActive] = {.60f, .97f, .83f, 1};
  s.ScaleAllSizes(dpi); s.FontScaleDpi = dpi;
}
}
