#pragma once

// Native reference bench. The worker receives only WAV samples and a mode;
// reference labels are consulted after recognition has finished.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <mmathen/analysis/chords.h>
#include <mmathen/analysis/pitch.h>
#include <mmathen/sink/wav_reader.h>
#include "analysis_profiles.h"
#include "guitar_chords.h"

namespace app {
namespace reference {
struct Chord { double start, end; int state; std::string label; };
struct Note { double start, end, midi; int string; };
struct Word { double start, end; std::string text; };
struct Pitch { double time, hz; };
struct Edit { std::string operation, expected, actual; };
struct Entry { std::string id, kind, mode, file; };
struct Data {
  std::string id, kind, mode, audio, stats, transcript, estimate;
  double duration = 0;
  std::vector<Chord> chords;
  std::vector<Note> notes;
  std::vector<Pitch> pitch;
  std::vector<Word> words;
  std::vector<Edit> edits;
};
inline std::vector<std::string> fields(const std::string& line) {
  std::vector<std::string> out;
  std::size_t start = 0;
  for (;;) {
    const auto end = line.find('\t', start);
    out.push_back(line.substr(start, end == std::string::npos ? end : end - start));
    if (end == std::string::npos) return out;
    start = end + 1;
  }
}
inline double number(const std::string& s) {
  std::size_t end = 0;
  const double value = std::stod(s, &end);
  if (end != s.size() || !std::isfinite(value)) throw std::runtime_error("Invalid reference number");
  return value;
}
inline int integer(const std::string& s, int low, int high) {
  const double value = number(s);
  if (value < low || value > high || value != std::floor(value)) throw std::runtime_error("Invalid reference index");
  return static_cast<int>(value);
}
inline std::vector<std::vector<std::string>> rows(const std::filesystem::path& path, const char* magic) {
  std::ifstream f(path);
  std::string line;
  if (!std::getline(f, line)) throw std::runtime_error("Cannot read reference bundle");
  if (!line.empty() && line.back() == '\r') line.pop_back();
  if (line != magic) throw std::runtime_error("Unrecognized reference bundle version");
  std::vector<std::vector<std::string>> out;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) out.push_back(fields(line));
  }
  if (f.bad()) throw std::runtime_error("Cannot finish reading reference bundle");
  return out;
}
inline std::filesystem::path child(const std::filesystem::path& root, const std::string& name) {
  const std::filesystem::path p(name);
  if (p.is_absolute() || p.has_root_name()) throw std::runtime_error("Expected a relative bundle path");
  for (const auto& part : p) if (part == "..") throw std::runtime_error("Reference path leaves bundle");
  return root / p;
}
inline Data read(const std::filesystem::path& path) {
  Data d;
  for (const auto& r : rows(path, "MMATHEN_REFERENCE_V1")) {
    if (r[0] == "META" && r.size() == 6 && d.id.empty()) {
      d.id = r[1]; d.kind = r[2]; d.mode = r[3]; d.audio = r[4]; d.duration = number(r[5]);
    } else if (r[0] == "STATS" && r.size() == 2) d.stats = r[1];
    else if (r[0] == "TRANSCRIPT" && r.size() == 2) d.transcript = r[1];
    else if (r[0] == "ESTIMATE" && r.size() == 2) d.estimate = r[1];
    else if (r[0] == "CHORD" && r.size() == 5)
      d.chords.push_back({number(r[1]), number(r[2]), integer(r[3], -1, 24), r[4]});
    else if (r[0] == "NOTE" && r.size() == 5)
      d.notes.push_back({number(r[1]), number(r[2]), number(r[3]), integer(r[4], 0, 5)});
    else if (r[0] == "PITCH" && r.size() == 3) d.pitch.push_back({number(r[1]), number(r[2])});
    else if (r[0] == "WORD" && r.size() == 4) d.words.push_back({number(r[1]), number(r[2]), r[3]});
    else if (r[0] == "EDIT" && r.size() == 4) d.edits.push_back({r[1], r[2], r[3]});
    else throw std::runtime_error("Malformed reference record");
  }
  if (d.id.empty() || d.audio.empty() || d.duration <= 0 ||
      (d.kind != "guitar" && d.kind != "speech") || (d.mode != "chords" && d.mode != "pitch"))
    throw std::runtime_error("Incomplete reference metadata");
  double previous = 0;
  for (const auto& c : d.chords) {
    if (c.start < previous - 1e-8 || c.end <= c.start || c.end > d.duration + .01)
      throw std::runtime_error("Invalid chord boundaries");
    previous = c.end;
  }
  previous = -1;
  for (const auto& p : d.pitch) {
    if (p.time < 0 || p.time <= previous || p.time > d.duration || p.hz < 0)
      throw std::runtime_error("Invalid reference pitch grid");
    previous = p.time;
  }
  for (const auto& n : d.notes)
    if (n.start < 0 || n.end <= n.start || n.end > d.duration + .01 || n.midi < 0 || n.midi > 127)
      throw std::runtime_error("Invalid note reference");
  for (const auto& w : d.words)
    if (w.start < 0 || w.end < w.start || w.end > d.duration + .1)
      throw std::runtime_error("Invalid estimated word timing");
  if ((d.mode == "chords" && d.chords.empty()) || (d.mode == "pitch" && d.pitch.empty()))
    throw std::runtime_error("Missing reference timeline");
  return d;
}

struct Measurement {
  double rate = 0, duration = 0;
  mmathen::ChordRecognition chords;
  std::vector<Chord> chordIntervals; // Exact analysis cells and full chord labels.
  std::string engine = "MMathEn";
  std::vector<mmathen::PitchFrame> pitch;
};
inline Measurement measure(const mmathen::WavData& w, bool chords) {
  if (w.channels != 1) throw std::runtime_error("Reference audio must be mono");
  Measurement m;
  m.rate = w.rate.value(); m.duration = static_cast<double>(w.frames) / m.rate;
  if (chords) {
    const auto result = guitar::recognize(w.samples,w.rate);
    for (std::size_t i=0;i<result.frames.size();++i) {
      const auto& c=result.frames[i];
      m.chordIntervals.push_back({static_cast<double>(i*guitar::hop+3072)/m.rate,
          std::min(m.duration,static_cast<double>(i*guitar::hop+5120)/m.rate),c.triad,c.label});
    }
  } else {
    auto result = mmathen::trackPitch(w.samples, w.rate, 2048, 512, mmathen::PitchMethod::Yin);
    if (!result.ok()) throw std::runtime_error("Pitch recognition failed");
    m.pitch = std::move(result).value();
  }
  return m;
}
struct Comparison { double start, end; int expected, actual; std::string label{}; };
inline std::string actualName(const Comparison& c) {
  return c.label.empty() ? mmathen::chordName(static_cast<std::size_t>(c.actual)) : c.label;
}
struct Score {
  double correct = 0, eligible = 0; // seconds for chords; frames for pitch
  double accuracy() const { return eligible > 0 ? correct / eligible : -1; }
  std::vector<Comparison> comparisons;
};
inline Score score(const Data& d, const Measurement& m) {
  if (std::abs(d.duration - m.duration) > 1e-5) throw std::runtime_error("Audio/reference duration mismatch");
  Score out;
  if (d.mode == "chords" && !m.chordIntervals.empty()) {
    for (const auto& actual : m.chordIntervals) for (const auto& c : d.chords) {
      const double a = std::max(c.start, actual.start), b = std::min(c.end, actual.end);
      if (a >= b) continue;
      out.comparisons.push_back({a, b, c.state, actual.state, actual.label});
      if (c.state < 0) continue;
      out.eligible += b - a;
      if (c.state == actual.state) out.correct += b - a;
    }
  } else if (d.mode == "chords") {
    const mmathen::ChordOptions options;
    const double hop = static_cast<double>(options.chroma.hop) / m.rate;
    for (std::size_t i = 0; i < m.chords.viterbi.size(); ++i) {
      const double center = (static_cast<double>(i * options.chroma.hop) +
                             .5 * static_cast<double>(options.chroma.frameSize)) / m.rate;
      const int actual = static_cast<int>(m.chords.viterbi[i]);
      for (const auto& c : d.chords) {
        const double a = std::max(c.start, center - hop / 2), b = std::min(c.end, center + hop / 2);
        if (a >= b) continue;
        out.comparisons.push_back({a, b, c.state, actual});
        if (c.state < 0) continue;
        out.eligible += b - a;
        if (c.state == actual) out.correct += b - a;
      }
    }
  } else {
    if (d.pitch.size() != m.pitch.size()) throw std::runtime_error("Audio/reference pitch frame count mismatch");
    for (std::size_t i = 0; i < d.pitch.size(); ++i) {
      const auto& p = m.pitch[i];
      if (std::abs(d.pitch[i].time - p.timeSeconds) > 1e-7) throw std::runtime_error("Pitch grid alignment mismatch");
      if (d.pitch[i].hz <= 0) continue;
      out.eligible += 1;
      const bool correct = p.estimate.voiced && std::abs(1200 * std::log2(p.estimate.frequencyHz / d.pitch[i].hz)) <= 50;
      if (correct) out.correct += 1;
      const double half = 256 / m.rate;
      out.comparisons.push_back({p.timeSeconds - half, p.timeSeconds + half, 1, correct ? 1 : 0});
    }
  }
  return out;
}
} // namespace reference

struct ReferenceDemo {
  std::filesystem::path root;
  std::vector<reference::Entry> entries;
  std::string summary, error;
  reference::Data data;
  reference::Measurement measured;
  reference::Score result;
  std::future<reference::Measurement> worker;
  int selected = -1;
  bool ready = false;
  std::function<reference::Measurement(const mmathen::WavData&, bool, const std::filesystem::path&)> measureAudio;

  bool busy() const { return worker.valid(); }
  bool open(const std::string& path) {
    try {
      std::vector<reference::Entry> next;
      std::string nextSummary;
      const auto nextRoot = std::filesystem::absolute(path).parent_path();
      for (const auto& r : reference::rows(path, "MMATHEN_SUITE_V1")) {
        if (r[0] == "SUMMARY" && r.size() == 2) nextSummary = r[1];
        else if (r[0] == "TRACK" && r.size() == 5) {
          (void)reference::child(nextRoot, r[4]);
          next.push_back({r[1], r[2], r[3], r[4]});
        } else throw std::runtime_error("Malformed reference catalog");
      }
      if (next.empty()) throw std::runtime_error("Empty reference catalog");
      root = nextRoot; entries = std::move(next); summary = std::move(nextSummary);
      selected = -1; ready = false; data = {}; error.clear();
      return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
  }
  bool select(int index, mmathen::WavData& wav) {
    if (busy() || index < 0 || static_cast<std::size_t>(index) >= entries.size()) return false;
    try {
      const auto& entry = entries[static_cast<std::size_t>(index)];
      auto next = reference::read(reference::child(root, entry.file));
      if (next.id != entry.id || next.kind != entry.kind || next.mode != entry.mode)
        throw std::runtime_error("Catalog/reference metadata mismatch");
      auto w = mmathen::readWavFile(reference::child(root, next.audio).string());
      if (!w.ok()) throw std::runtime_error("Cannot read the recording");
      if (w.value().channels != 1 || std::abs(static_cast<double>(w.value().frames) / w.value().rate.value() - next.duration) > 1e-5)
        throw std::runtime_error("Audio does not match this reference duration/format");
      wav = std::move(w).value();
      worker = std::async(std::launch::async, [input = wav, chords = next.mode == "chords", audioPath = reference::child(root, next.audio), custom = measureAudio] {
        return custom ? custom(input, chords, audioPath) : reference::measure(input, chords);
      });
      data = std::move(next); selected = index; ready = false; error.clear();
      measured = {}; result = {};
      return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
  }
  void poll() {
    if (!worker.valid() || worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try { measured = worker.get(); result = reference::score(data, measured); ready = true; }
    catch (const std::exception& e) { error = e.what(); ready = false; }
  }
};
} // namespace app
