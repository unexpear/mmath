#pragma once
// Research comparison only. Never included or selected by the desktop app.
#include <SDL3/SDL.h>
#include <atomic>
#include <memory>
#include "../../app/reference_demo.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace app::chordino {
// Major/minor projection is used only for the existing comparison metric.
// Keep the complete Harte label for display, including extensions and bass.
inline int triad(const std::string& label) {
  if (label == "N") return 24;
  if (label.empty()) return -2;
  const std::string letters = "C D EF G A B";
  auto root = letters.find(label[0]);
  if (root == std::string::npos || label[0] == ' ') return -2;
  std::size_t i = 1;
  int pc = static_cast<int>(root);
  while (i < label.size() && (label[i] == '#' || label[i] == 'b')) pc += label[i++] == '#' ? 1 : -1;
  std::string quality = "maj";
  if (i < label.size() && label[i] == ':') {
    const auto end = label.find('/', i);
    quality = label.substr(i + 1, end == std::string::npos ? end : end - i - 1);
  } else if (i < label.size() && label[i] != '/') return -2;
  const bool major = quality == "maj" || quality == "maj6" || quality == "maj7" || quality == "7";
  const bool minor = quality == "min" || quality == "min6" || quality == "min7";
  if (!major && !minor) return -2;
  // mir_eval includes the bass in the pitch-class bitmap. An added second,
  // fourth, etc. can therefore invalidate even a major/minor projection.
  const auto slash = label.find('/');
  if (slash != std::string::npos) {
    auto bass = label.substr(slash + 1);
    std::size_t degreeStart = 0;
    int accidental = 0;
    while (degreeStart < bass.size() && (bass[degreeStart] == '#' || bass[degreeStart] == 'b'))
      accidental += bass[degreeStart++] == '#' ? 1 : -1;
    int degree = 0;
    try { degree = reference::integer(bass.substr(degreeStart), 1, 13); }
    catch (const std::exception&) { return -2; }
    constexpr int semitones[] = {0, 2, 4, 5, 7, 9, 11};
    const int note = ((semitones[(degree - 1) % 7] + accidental) % 12 + 12) % 12;
    if (note < 8 && note != 0 && note != 7 && note != (minor ? 3 : 4)) return -2;
  }
  return (pc % 12 + 12) % 12 + (minor ? 12 : 0);
}
inline reference::Measurement parse(const std::string& csv, const mmathen::WavData& w) {
  reference::Measurement m;
  m.rate = w.rate.value(); m.duration = static_cast<double>(w.frames) / m.rate;
  m.engine = "Chordino";
  std::vector<std::pair<double, std::string>> changes;
  std::istringstream input(csv);
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) continue;
    const auto comma = line.find(',');
    if (comma == std::string::npos) throw std::runtime_error("Unrecognized Chordino output");
    const double time = reference::number(line.substr(0, comma));
    auto label = line.substr(comma + 1);
    if (label.size() < 2 || label.front() != '"' || label.back() != '"') throw std::runtime_error("Invalid Chordino label");
    label = label.substr(1, label.size() - 2);
    if (label.empty() || label.find_first_of("\"\t\r\n") != std::string::npos || time < 0 || time > m.duration + 1e-7 || (!changes.empty() && time <= changes.back().first))
      throw std::runtime_error("Invalid Chordino timeline");
    changes.emplace_back(time, label);
  }
  if (changes.empty() || changes.front().first > 1e-8) throw std::runtime_error("Incomplete Chordino output");
  // Use the exact time coverage of our existing 8192/2048 comparison grid.
  const std::size_t frames = w.frames <= 8192 ? 1 : (w.frames - 8192) / 2048 + 1;
  const double lo = 3072 / m.rate;
  const double hi = std::min(m.duration, static_cast<double>((frames - 1) * 2048 + 5120) / m.rate);
  for (std::size_t i = 0; i < changes.size(); ++i) {
    const double a = std::max(lo, changes[i].first);
    const double b = std::min(hi, i + 1 < changes.size() ? changes[i + 1].first : m.duration);
    if (a < b) m.chordIntervals.push_back({a, b, triad(changes[i].second), changes[i].second});
  }
  if (m.chordIntervals.empty()) throw std::runtime_error("No Chordino intervals in the comparison window");
  return m;
}
struct Backend {
  std::filesystem::path root;
  std::shared_ptr<std::atomic<bool>> cancelled = std::make_shared<std::atomic<bool>>(false);
  bool available() const {
#ifdef _WIN32
    return std::filesystem::is_regular_file(root / "sonic/sonic-annotator-win64/sonic-annotator.exe") &&
           std::filesystem::is_regular_file(root / "chordino/nnls-chroma.dll") &&
           std::filesystem::is_regular_file(root / "chordino-harte.ttl");
#else
    return false;
#endif
  }
  reference::Measurement measure(const mmathen::WavData& w, const std::filesystem::path& audio) const {
#ifdef _WIN32
    if (!available()) throw std::runtime_error("Chordino is unavailable. Choose MMathEn or prepare the optional community tools.");
    if (cancelled->load()) throw std::runtime_error("Chordino analysis cancelled");
    const auto exe = (root / "sonic/sonic-annotator-win64/sonic-annotator.exe").string();
    const auto transform = (root / "chordino-harte.ttl").string();
    const auto path = audio.string(), plugins = (root / "chordino").string();
    const char* args[] = {exe.c_str(), "-q", "-t", transform.c_str(), "-w", "csv", "--csv-stdout", "--csv-omit-filename", "--csv-digits", "12", path.c_str(), nullptr};
    SDL_Environment* env = SDL_CreateEnvironment(true);
    if (!env) throw std::runtime_error(SDL_GetError());
    const auto props = SDL_CreateProperties();
    const bool configured = props && SDL_SetEnvironmentVariable(env, "VAMP_PATH", plugins.c_str(), true) &&
      SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, const_cast<char**>(args)) &&
      SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env) &&
      SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP) &&
      SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_NUMBER, SDL_PROCESS_STDIO_NULL) &&
      SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
    // SDL's background option suppresses the Windows console but masks exit codes.
    // Retain a Windows query handle so a failed/crashed helper cannot be scored.
    auto* process = configured ? SDL_CreateProcessWithProperties(props) : nullptr;
    SDL_DestroyProperties(props); SDL_DestroyEnvironment(env);
    if (!process) throw std::runtime_error(std::string("Cannot start Chordino: ") + SDL_GetError());
    struct Running {
      SDL_Process* process;
      HANDLE query = nullptr;
      ~Running() {
        if (!SDL_WaitProcess(process, false, nullptr)) SDL_KillProcess(process, true);
        SDL_DestroyProcess(process);
        if (query) CloseHandle(query);
      }
    } running{process};
    const auto pid = static_cast<DWORD>(SDL_GetNumberProperty(SDL_GetProcessProperties(process), SDL_PROP_PROCESS_PID_NUMBER, 0));
    running.query = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!running.query) throw std::runtime_error("Cannot verify Chordino process status");
    auto* pipe = SDL_GetProcessOutput(process);
    if (!pipe) throw std::runtime_error("Cannot read Chordino output");
    const auto started = std::chrono::steady_clock::now();
    std::string csv;
    bool ended = false;
    for (;;) {
      if (cancelled->load()) throw std::runtime_error("Chordino analysis cancelled");
      if (std::chrono::steady_clock::now() - started > std::chrono::seconds(60))
        throw std::runtime_error("Chordino exceeded the 60 second analysis limit");
      char buffer[4096];
      const auto count = SDL_ReadIO(pipe, buffer, sizeof buffer);
      if (count) {
        csv.append(buffer, count);
        if (csv.size() > 16 * 1024 * 1024) throw std::runtime_error("Chordino output is too large");
      } else {
        const auto status = SDL_GetIOStatus(pipe);
        if (status == SDL_IO_STATUS_EOF && ended) break;
        if (status != SDL_IO_STATUS_EOF && status != SDL_IO_STATUS_NOT_READY)
          throw std::runtime_error("Cannot finish reading Chordino output");
        ended = SDL_WaitProcess(process, false, nullptr);
        SDL_Delay(5);
      }
    }
    DWORD exitCode = STILL_ACTIVE;
    if (!GetExitCodeProcess(running.query, &exitCode) || exitCode != 0)
      throw std::runtime_error("Chordino analysis failed; no score was produced");
    return parse(csv, w);
#else
    (void)w; (void)audio;
    throw std::runtime_error("The optional Chordino helper is currently available on Windows only");
#endif
  }
};
}
