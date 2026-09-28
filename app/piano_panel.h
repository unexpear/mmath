#pragma once
#include <imgui.h>
#include "desktop_style.h"
#include "music_explorer.h"

namespace app::music {
// White keys first, black keys on top. Hit testing follows the same order.
inline int keyboard(const char* id, int low, int high, const std::vector<int>& selected,
                    int heard, float height, bool labels=true) {
  const auto origin=ImGui::GetCursorScreenPos();
  const float width=std::max(1.0f,ImGui::GetContentRegionAvail().x);
  ImGui::InvisibleButton(id,{width,height});
  auto* d=ImGui::GetWindowDrawList();
  const auto mouse=ImGui::GetIO().MousePos;
  const bool hovered=ImGui::IsItemHovered();
  int whites=0;
  for(int n=low;n<=high;++n) if(!blackKey(n)) ++whites;
  if(!whites) return -1;
  const float step=width/static_cast<float>(whites);
  int hit=-1;
  for(int pass=0;pass<2;++pass) {
    int white=0;
    for(int n=low;n<=high;++n) {
      const bool black=blackKey(n);
      const float x=origin.x+static_cast<float>(white)*step;
      if(!black) ++white;
      if(black!=(pass==1)) continue;
      const ImVec2 a{black?x-step*.31f:x,origin.y};
      const ImVec2 b{black?x+step*.31f:x+step-1,origin.y+height*(black?.61f:1)};
      const bool chosen=std::find(selected.begin(),selected.end(),n)!=selected.end();
      const auto color=n==heard?IM_COL32(85,218,183,255):chosen?IM_COL32(97,165,235,255):
                       black?IM_COL32(20,29,39,255):IM_COL32(220,229,234,255);
      d->AddRectFilled(a,b,color,2);
      if(labels && (chosen || (!black && n%12==0))) {
        const auto name=tuner::note(n);
        const auto size=ImGui::CalcTextSize(name.c_str());
        d->AddText({(a.x+b.x-size.x)/2,b.y-size.y-7},black?IM_COL32(235,240,248,255):IM_COL32(18,34,48,255),name.c_str());
      }
      if(hovered && mouse.x>=a.x && mouse.x<b.x && mouse.y>=a.y && mouse.y<b.y) hit=n;
    }
  }
  if(hit>=0) ImGui::SetTooltip("%s (MIDI %d) - click to select",tuner::note(hit).c_str(),hit);
  return hit>=0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) ? hit : -1;
}

struct Explorer {
  int root=60, quality=0, inversion=0;
  std::vector<int> audition;
  void draw(double a4, int heard) {
    const float u=ui::unit();
    ui::caption("PIANO & MUSIC MATH");
    ui::heading((std::string(tuner::pitchNames[root%12])+" "+qualities[quality].name).c_str(),32);
    ImGui::TextWrapped("Choose a key to explore. Blue keys show the example chord; green shows the one note heard by the tuner.");
    const int selected=keyboard("88-keys",21,108,{root},heard,55*u,false);
    if(selected>=0) root=selected;
    ui::caption("A0                       All 88 piano keys                       C8");
    ImGui::SetNextItemWidth(155*u);
    if(ImGui::BeginCombo("Chord",qualities[quality].name)) {
      for(int i=0;i<static_cast<int>(std::size(qualities));++i)
        if(ImGui::Selectable(qualities[i].name,quality==i)) { quality=i; inversion=0; }
      ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(155*u);
    ImGui::SliderInt("Root note (MIDI)",&root,21,108,tuner::note(root).c_str(),ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetNextItemWidth(155*u);
    ImGui::SliderInt("Inversion",&inversion,0,qualities[quality].count-1,"%d",ImGuiSliderFlags_AlwaysClamp);
    const auto notes=voicing(root,quality,inversion);
    const int low=std::clamp(root/12*12,21,84), high=std::min(108,low+24);
    const int key=keyboard("detail-keys",low,high,notes,heard,140*u);
    if(key>=0) root=key;
    ImGui::Spacing();
    if(ImGui::Button("Hear root tone")) audition={root};
    ImGui::SameLine(); ImGui::BeginDisabled(notes.empty());
    if(ImGui::Button("Hear chord tones")) audition=notes;
    ImGui::EndDisabled();
    ui::caption("Plain reference tones; live tuning follows one note at a time");
    if(notes.empty()) ImGui::TextWrapped("This voicing extends beyond C8. Choose a lower root or inversion.");
    else if(ImGui::BeginTable("note-math",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
      for(const char* h:{"Note","Hz","Semitones","Ratio to root"}) ImGui::TableSetupColumn(h);
      ImGui::TableHeadersRow();
      for(int n:notes) {
        ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(tuner::note(n).c_str());
        ImGui::TableNextColumn(); ImGui::Text("%.2f",tuner::frequency(n,a4));
        ImGui::TableNextColumn(); ImGui::Text("%d",n-root);
        ImGui::TableNextColumn(); ImGui::Text("%.5f",std::exp2(static_cast<double>(n-root)/12));
      }
      ImGui::EndTable();
    }
    if(ImGui::CollapsingHeader("Why these numbers?")) {
      ImGui::TextWrapped("Equal temperament: frequency = A4 x 2^((MIDI - 69) / 12). One semitone is 100 cents; an octave doubles frequency.");
      for(int i=0;i<qualities[quality].count;++i) {
        const int step=qualities[quality].steps[static_cast<std::size_t>(i)];
        ImGui::Text("%s: %d semitones, %d cents",interval(step),step,step*100);
      }
      ImGui::TextWrapped("A pure 3:2 fifth is 701.955 cents; the equal-tempered fifth is 700. Piano stretch tuning and string inharmonicity can shift measured frequencies away from this ideal grid.");
    }
  }
};
}
