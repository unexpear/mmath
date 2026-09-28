// Validate every exported native reference and independently recompute scores.
#include "../../app/reference_demo.h"
#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  app::ReferenceDemo bench;
  if (!bench.open(argv[1])) { std::cerr << bench.error << '\n'; return 1; }
  std::cout << std::setprecision(17);
  try {
    // Fail-closed parser edge cases, without loading any audio.
    for (const char* bad : {"nan", "inf", "1oops"}) {
      bool rejected = false;
      try { (void)app::reference::number(bad); } catch (const std::exception&) { rejected = true; }
      if (!rejected) return 1;
    }
    bool rejected = false;
    try { (void)app::reference::child(bench.root, "../outside.wav"); }
    catch (const std::exception&) { rejected = true; }
    if (!rejected) return 1;
    for (const auto& entry : bench.entries) {
      const auto data = app::reference::read(app::reference::child(bench.root, entry.file));
      const auto audio = mmathen::readWavFile(app::reference::child(bench.root, data.audio).string());
      if (!audio.ok()) throw std::runtime_error("Cannot read audio");
      const auto measured = app::reference::measure(audio.value(), data.mode == "chords");
      const auto result = app::reference::score(data, measured);
      std::cout << data.id << '\t' << result.correct << '\t' << result.eligible << '\t' << result.accuracy() << '\n';
    }
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
  return 0;
}
