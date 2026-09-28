// Streaming replay: audio input only; labels are scored by compare_tuners.py.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include "../../app/streaming_tuner.h"
#include "TunerEngine.hpp"
#include <mmathen/sink/wav_reader.h>

int main(int argc, char** argv) {
  if (argc != 3) { std::cerr << "Usage: tuner_probe audio.wav output.json\n"; return 2; }
  const auto wav = mmathen::readWavFile(argv[1]);
  if (!wav.ok() || wav.value().channels != 1 || wav.value().rate.value() < 8000) return 1;
  const auto& w = wav.value();
  const double rate = w.rate.value();
  std::ofstream out(argv[2]);
  if (!out) return 1;
  out << std::setprecision(17) << "{\"sampleRate\":" << rate << ",\"samples\":" << w.frames
      << ",\"rowFormat\":[\"availableTime\",\"windowStart\",\"hz\",\"voiced\",\"confidence\",\"rmsDb\",\"cpuMs\"],\"desktop\":[";
  app::StreamingTuner desktop;
  std::vector<double> frame(4096);
  bool first = true;
  // Ideal 15 Hz UI scheduling; Windows/device/callback delays are not simulated.
  for (std::size_t tick = 1;; ++tick) {
    const auto end = static_cast<std::size_t>(std::llround(static_cast<double>(tick) * rate / 15));
    if (end > w.samples.size()) break;
    std::fill(frame.begin(), frame.end(), 0);
    const auto count = std::min(end, frame.size());
    std::copy_n(w.samples.begin() + static_cast<std::ptrdiff_t>(end - count), count,
                frame.end() - static_cast<std::ptrdiff_t>(count));
    const auto start = std::chrono::steady_clock::now();
    const auto p = desktop.process(frame, rate);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!first) out << ',';
    first = false;
    out << '[' << static_cast<double>(end) / rate << ',' << static_cast<double>(end - count) / rate
        << ',' << p.frequencyHz << ',' << (p.voiced && desktop.levelDb > -55 ? "true" : "false")
        << ',' << p.confidence << ',' << desktop.levelDb << ',' << ms << ']';
  }
  out << "],\"f3\":[";
  // Installed v1.1.3, f3 acoustic profile, normal room, balanced quality.
  // Preserve the native core, including filtering/smoothing over overlapping frames.
  TunerEngine f3(static_cast<float>(rate), 2048);
  f3.setFrequencyRange(55, 1400);
  f3.setNoiseGateDb(-55);
  f3.setConfidenceThreshold(0.75f);
  f3.setHpfCutoff(40);
  f3.setA4(440);
  f3.setOnsetDetectionEnabled(true);
  f3.setPostProcessorConfig({0.32f, 3});
  std::vector<float> input(2048);
  first = true;
  for (std::size_t off = 0; off + input.size() <= w.samples.size(); off += 1024) {
    std::transform(w.samples.begin() + static_cast<std::ptrdiff_t>(off),
                   w.samples.begin() + static_cast<std::ptrdiff_t>(off + input.size()),
                   input.begin(), [](double x) { return static_cast<float>(x); });
    const auto start = std::chrono::steady_clock::now();
    const auto p = f3.process(input.data(), static_cast<int>(input.size()));
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (!first) out << ',';
    first = false;
    out << '[' << static_cast<double>(off + input.size()) / rate << ',' << static_cast<double>(off) / rate
        << ',' << p.frequency << ',' << (p.hasPitch ? "true" : "false") << ',' << p.confidence
        << ',' << p.rmsDb << ',' << ms << ']';
  }
  out << "]}\n";
  out.close();
  return out.fail() ? 1 : 0;
}
