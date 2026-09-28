// Exercise the retained research adapter and its independent scoring/label path.
#include "chordino_backend.h"
#include <iomanip>
#include <iostream>
#include <thread>

namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F action) {
  bool rejected = false;
  try { action(); } catch (const std::exception&) { rejected = true; }
  check(rejected, "Invalid input was accepted");
}
}
int main(int argc, char** argv) {
  // A copy of this validation executable acts as a crashing/stalled test helper.
  if (argc > 2 && std::string(argv[1]) == "-q") {
    if (std::string(argv[argc - 1]) == "hang") for (;;) SDL_Delay(50);
    std::cout << "0,\"N\"\n" << std::flush;
    return 7;
  }
  if (argc != 4) return 2;
  try {
    check(SDL_Init(0), "SDL init");
    if (std::string(argv[1]) == "--failures") {
      app::chordino::Backend backend{std::filesystem::absolute(argv[2])};
      auto w = mmathen::readWavFile(argv[3]); check(w.ok(), "Read failure fixture audio");
      const auto began = std::chrono::steady_clock::now();
      bool failed = false;
      try { (void)backend.measure(w.value(), "fail"); }
      catch (const std::exception& e) { failed = std::string(e.what()).find("analysis failed") != std::string::npos; }
      check(failed, "Nonzero exit with plausible CSV must not produce a score");
      std::jthread cancel([flag = backend.cancelled] { SDL_Delay(100); flag->store(true); });
      rejects([&] { (void)backend.measure(w.value(), "hang"); });
      check(std::chrono::steady_clock::now() - began < std::chrono::seconds(5), "Cancellation must release the worker");
      std::cout << "PASS: failed helper rejected; stalled helper cancelled\n";
      SDL_Quit();
      return 0;
    }
    app::chordino::Backend backend{std::filesystem::absolute(argv[1])};
    std::ifstream list(argv[2]); check(bool(list), "Cannot open check manifest");
    std::ofstream out(argv[3]); check(bool(out), "Cannot open check output");
    out << std::setprecision(17);
    std::string line;
    int tracks = 0;
    double correct = 0, eligible = 0;
    while (std::getline(list, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      const auto fields = app::reference::fields(line);
      check(fields.size() == 4, "Bad check manifest");
      const auto data = app::reference::read(fields[1]);
      auto w = mmathen::readWavFile(fields[0]); check(w.ok(), "Cannot read check audio");
      if (!tracks) {
        for (const char* csv : {"", "nan,\"N\"\n", "0,\"C\"\n0,\"D\"\n", "0,C\n", "1,\"N\"\n", "0,\"\"\n", "-1,\"N\"\n"})
          rejects([&] { (void)app::chordino::parse(csv, w.value()); });
        check(app::chordino::triad("C:7/3") == 0 && app::chordino::triad("Bb:min7/5") == 22 &&
              app::chordino::triad("F##:maj6") == 7 && app::chordino::triad("C:dim") == -2 &&
              app::chordino::triad("C/2") == -2 && app::chordino::triad("N") == 24, "Chord projection contract");
        const auto extended = app::chordino::parse("0,\"C:maj7\"\n1,\"C:7\"\n2,\"C:dim\"\n", w.value());
        app::reference::Data truth; truth.mode = "chords"; truth.duration = extended.duration;
        truth.chords.push_back({0, truth.duration, 0, "C:maj"});
        const auto scored = app::reference::score(truth, extended);
        check(scored.comparisons.size() == 3, "Extensions must not be merged away");
        check(app::reference::actualName(scored.comparisons[0]) == "C:maj7" &&
              app::reference::actualName(scored.comparisons[2]) == "C:dim", "Full labels retained");
        check(std::abs(scored.correct - (2 - 3072 / extended.rate)) < 1e-8 &&
              scored.eligible > scored.correct, "Unsupported predictions count as misses");
      }
      const auto measured = backend.measure(w.value(), fields[0]);
      const auto result = app::reference::score(data, measured);
      check(std::abs(result.correct - app::reference::number(fields[2])) < 1e-8, "Correct duration disagrees with mir_eval");
      check(std::abs(result.eligible - app::reference::number(fields[3])) < 1e-8, "Eligible duration disagrees with mir_eval");
      for (const auto& c : measured.chordIntervals)
        out << data.id << '\t' << c.start << '\t' << c.end << '\t' << c.state << '\t' << c.label << '\n';
      correct += result.correct; eligible += result.eligible; ++tracks;
    }
    check(tracks > 0 && bool(out), "Empty or incomplete check");
    std::cout << std::setprecision(12) << "PASS: " << tracks << " native Chordino runs; " << correct << " / " << eligible << " supported seconds\n";
    SDL_Quit();
    return 0;
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; SDL_Quit(); return 1; }
}
