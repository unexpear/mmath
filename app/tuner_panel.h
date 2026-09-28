#pragma once
#include <imgui.h>
#include <cstdio>
#include <mmathen/analysis/pitch.h>
#include "tuner_controls.h"
#include "tuner_calibration.h"
#include "desktop_style.h"
#include "piano_panel.h"
#include "analysis_profiles.h"

namespace app::tuner {
inline void diagram(const Shape& s, float width, float height = 235, bool showName = true) {
  const float u = ui::unit();
  ImGui::PushID(s.name);
  if (showName) ImGui::TextUnformatted(s.name);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("fretboard", {width, height});
  auto* d = ImGui::GetWindowDrawList();
  const float dx = std::max(16*u, std::min(48*u, (width-64*u)/5));
  const float left = origin.x + (width-5*dx)/2, top = origin.y+30*u;
  const float dy = (height-70*u)/5;
  const int first = firstFret(s);
  const auto ink = IM_COL32(143,167,185,255), dot = IM_COL32(89,217,186,255);
  for (int f=0; f<=5; ++f)
    d->AddLine({left,top+dy*f}, {left+dx*5,top+dy*f}, ink, (f==0 && first==1 ? 4.0f : 1.0f)*u);
  for (int i=0; i<6; ++i) d->AddLine({left+dx*i,top}, {left+dx*i,top+dy*5}, ink, u);
  // Draw barres first, so every finger number remains legible.
  for (int finger=1; finger<=4; ++finger) {
    for (int i=0; i<6; ++i) if (s.fingers[i]==finger && s.frets[i]>0) {
      int last=i;
      for (int j=i+1; j<6; ++j) if(s.fingers[j]==finger && s.frets[j]==s.frets[i]) last=j;
      if (last>i) {
        const float y=top+(static_cast<float>(s.frets[i]-first)+.5f)*dy;
        d->AddLine({left+dx*i,y}, {left+dx*last,y}, dot, 12*u);
        break;
      }
    }
  }
  const auto centered = [&](const char* text, float x, float y, ImU32 color) {
    const auto size=ImGui::CalcTextSize(text);
    d->AddText({x-size.x/2,y-size.y/2}, color, text);
  };
  for (int i=0; i<6; ++i) {
    const float x=left+dx*i;
    if (s.frets[i]<=0) centered(s.frets[i]<0 ? "x" : "o",x,top-17*u,ink);
    else {
      const float y=top+(static_cast<float>(s.frets[i]-first)+.5f)*dy;
      d->AddCircleFilled({x,y},12*u,dot);
      const auto label=std::to_string(s.fingers[i]);
      centered(label.c_str(),x,y,IM_COL32(12,28,34,255));
    }
    centered(note(tunings[0].midi[i]).c_str(),x,top+dy*5+17*u,ink);
  }
  if (first>1) centered(std::to_string(first).c_str(),left-23*u,top+.5f*dy,ink);
  ImGui::TextWrapped("Notes: %s",shapeNotes(s).c_str());
  ImGui::PopID();
}

struct Panel {
  int instrument=0; // General, piano, guitar; presentation and search range only.
  music::Explorer music;
  PitchRange pitchRange() const { return instrument==2 ? PitchRange::Guitar : PitchRange::Music; }
  int tuning=0, selectedString=-1, room=1, shape=0;
  float a4=440, tolerance=3;
  bool assist=false;
  bool microphone=false, refined=false, showCalibration=false;
  int calibrationScrollFrames=0;
  double referenceTone=440;
  Calibration calibration;
  Reading last;

  void tunerCard(const mmathen::PitchEstimate& p, double level, bool active) {
    const float u=ui::unit();
    ui::caption("LIVE TUNER");
    if(instrument==2) {
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##tuning",tuning<0 ? "Chromatic" : tunings[tuning].name)) {
      if (ImGui::Selectable("Chromatic",tuning<0)) { tuning=-1; selectedString=-1; assist=false; }
      for(int i=0; i<static_cast<int>(std::size(tunings)); ++i)
        if(ImGui::Selectable(tunings[i].name,tuning==i)) { tuning=i; selectedString=-1; assist=false; }
      ImGui::EndCombo();
    }
    } else ui::caption("Chromatic - follows any single note");
    const double gate=room==0 ? -60 : room==2 ? -48 : -55;
    last=read(calibration.correct(p.frequencyHz,microphone),active && p.voiced && level>gate,instrument==2?tuning:-1,selectedString,a4,instrument==2 && assist);
    const auto color=!last.valid ? ui::muted : std::abs(last.deviation)<=tolerance
                     ? ui::accent : ImVec4(1,.76f,.45f,1);
    ImGui::PushStyleColor(ImGuiCol_Text,color);
    ui::heading(last.valid ? note(last.midi).c_str() : "--",64);
    ImGui::PopStyleColor();
    if(last.valid) ImGui::Text("%.2f Hz    %+.1f cents",last.rawHz,last.deviation);
    else ui::caption("Listening for one clear note");
    const char* message=!active ? "Choose a microphone or play a recording" : level<=gate
        ? "Too quiet - play a clear note" : !last.valid ? "Let one note ring" : verdict(last,tolerance);
    ImGui::TextColored(color,"%s",message);
    if (last.valid) ui::caption(refined ? "Fine tracking" : "Following the note");
    const auto o=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::InvisibleButton("cents-meter",{width,100*u});
    auto* d=ImGui::GetWindowDrawList();
    const float padding=16*u, span=std::max(1.0f,width-2*padding);
    const auto x=[&](double c){ return o.x+padding+span*static_cast<float>((c+50)/100); };
    d->AddRectFilled({x(-tolerance),o.y+14*u},{x(tolerance),o.y+62*u},IM_COL32(28,83,70,255),4*u);
    for(int c=-50; c<=50; c+=10) {
      d->AddLine({x(c),o.y+32*u},{x(c),o.y+56*u},IM_COL32(123,148,167,255),u);
      const auto label=std::to_string(c);
      d->AddText({x(c)-ImGui::CalcTextSize(label.c_str()).x/2,o.y+68*u},IM_COL32(148,169,186,255),label.c_str());
    }
    if(last.valid) d->AddCircleFilled({x(std::clamp(last.deviation,-50.0,50.0)),o.y+24*u},7*u,ImGui::ColorConvertFloat4ToU32(color));
    if(last.valid) {
      const int midi=last.string>=0 ? tunings[tuning].midi[last.string] : last.midi;
      ImGui::Text("Target %s  /  %.2f Hz",note(midi).c_str(),last.targetHz);
      if(last.harmonic!=1) ImGui::TextWrapped("Harmonic assist: %.2f Hz / %.1f = %.2f Hz",last.rawHz,last.harmonic,last.hz);
    }
    if(instrument==2 && tuning>=0) {
      if(ui::button("Find string automatically",selectedString<0,{-1,36*u})) { selectedString=-1; assist=false; }
      if(ImGui::BeginTable("strings",3,ImGuiTableFlags_SizingStretchSame)) {
        for(int i=0;i<6;++i) {
          ImGui::TableNextColumn();
          const auto label=std::to_string(6-i)+"   "+note(tunings[tuning].midi[i]);
          if(ui::button(label.c_str(),selectedString==i,{-1,42*u})) selectedString=i;
        }
        ImGui::EndTable();
      }
    }
    if(ImGui::CollapsingHeader("Tuning settings")) {
      ImGui::SetNextItemWidth(160*u);
      ImGui::SliderFloat("A4 reference",&a4,430,450,"%.1f Hz",ImGuiSliderFlags_AlwaysClamp);
      ImGui::SetNextItemWidth(160*u);
      ImGui::SliderFloat("Green window",&tolerance,1,5,"+/- %.1f cents",ImGuiSliderFlags_AlwaysClamp);
      ImGui::SetNextItemWidth(160*u);
      ImGui::Combo("Room",&room,"Quiet\0Normal\0Noisy\0");
      if(instrument==2) {
      ImGui::BeginDisabled(tuning<0 || selectedString<0);
      ImGui::Checkbox("Selected-string harmonic assist",&assist);
      ImGui::EndDisabled();
      ImGui::TextWrapped("Assist adjusts harmonics toward the selected string. The heard frequency stays visible.");
      }
    }
    if(ImGui::CollapsingHeader("Signal details")) {
      ImGui::Text("Level %.1f dBFS  |  Confidence %.2f",level,p.confidence);
      ImGui::TextWrapped("Effects are bypassed in the tuner. The green window is a display tolerance, not an accuracy guarantee.");
      if(instrument!=2) ImGui::TextWrapped("Music range: 25-4500 Hz, limited by the recording sample rate. This is a single-note tracker, not polyphonic piano transcription. Frequencies are compared to equal temperament, not a piano stretch-tuning curve.");
      if (calibration.applied && microphone) ImGui::Text("Before device correction: %.4f Hz",p.frequencyHz);
    }
    const bool requestedCalibration=showCalibration;
    if(showCalibration) { ImGui::SetNextItemOpen(true); showCalibration=false; }
    if(ImGui::CollapsingHeader("Device calibration")) {
      const bool justOpened=ImGui::IsItemToggledOpen();
      ImGui::TextWrapped("Use an independent reference tone with a known frequency. A tone from an uncalibrated computer cannot verify absolute accuracy.");
      ImGui::TextWrapped("This corrects microphone readings for this session. Changing the audio source clears it. A4 tuning and recording playback are separate.");
      ImGui::BeginDisabled(!microphone || calibration.collecting);
      ImGui::SetNextItemWidth(170*u);
      ImGui::InputDouble("Reference tone (Hz)",&referenceTone,.1,1,"%.4f");
      if(ImGui::Button("Measure reference")) calibration.start(referenceTone);
      ImGui::EndDisabled();
      if(!microphone) ui::caption("Choose a microphone to measure a reference");
      if(calibration.collecting) {
        ImGui::ProgressBar(static_cast<float>(calibration.progress()),{-1,0},"Hold the reference tone steady");
        if(ImGui::Button("Cancel measurement")) calibration.cancel();
      }
      if(calibration.applied) {
        ImGui::Text("Device correction: %+.2f ppm",calibration.ppm());
        if(ImGui::Button("Clear correction")) calibration.reset();
      }
      ImGui::TextWrapped("%s",calibration.message.c_str());
      if(justOpened || requestedCalibration) calibrationScrollFrames=2;
      if(calibrationScrollFrames>0) { ImGui::SetScrollHereY(1); --calibrationScrollFrames; }
    }
  }

  void chordCard() {
    const float u=ui::unit();
    ui::caption("CHORD LIBRARY");
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##shape",shapes[shape].name)) {
      for(int i=0;i<static_cast<int>(std::size(shapes));++i)
        if(ImGui::Selectable(shapes[i].name,shape==i)) shape=i;
      ImGui::EndCombo();
    }
    ui::heading(shapes[shape].name,40);
    diagram(shapes[shape],ImGui::GetContentRegionAvail().x,280*u,false);
    ImGui::Spacing();
    ui::caption("x  Skip string     o  Open string");
    ImGui::TextWrapped("Numbers show which finger to use. Play each note slowly, then strum the chord.");
    if(ImGui::CollapsingHeader("About these shapes")) {
      ImGui::TextWrapped("42 example fingerings in standard guitar tuning, A4 = 440 Hz. Finger positions are not detected from audio.");
      ImGui::TextWrapped("Use Reference recordings to compare expected and detected chords. The live tuner follows one pitch at a time.");
    }
  }

  void draw(const mmathen::PitchEstimate& p,double level,bool active) {
    const float u=ui::unit();
    ImGui::SetNextItemWidth(190*u);
    const bool changed=ImGui::Combo("Instrument",&instrument,"General music\0Piano\0Guitar\0");
    ImGui::SameLine(); ui::caption(instrument==2 ? "String targets & fingerings" : "Notes, chords & frequency relationships");
    const bool wide=ImGui::GetContentRegionAvail().x>820*u;
    const float height=wide ? std::max(360*u,ImGui::GetContentRegionAvail().y-12*u) : 660*u;
    if(!ImGui::BeginTable("tuner-layout",wide?2:1,ImGuiTableFlags_SizingStretchProp)) return;
    ImGui::TableSetupColumn("Tuner",ImGuiTableColumnFlags_WidthStretch,1.25f);
    if(wide) ImGui::TableSetupColumn("Chords",ImGuiTableColumnFlags_WidthStretch,1);
    ImGui::TableNextColumn();
    if(ImGui::BeginChild("tuner-card",{0,height},ImGuiChildFlags_Borders)) tunerCard(p,level,active && !changed);
    ImGui::EndChild();
    ImGui::TableNextColumn();
    if(ImGui::BeginChild("chord-card",{0,wide?height:690*u},ImGuiChildFlags_Borders)) {
      if(instrument==2) chordCard(); else music.draw(a4,last.valid ? last.midi : -1);
    }
    ImGui::EndChild();
    ImGui::EndTable();
  }
};
}
