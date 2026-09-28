// Offline measurement only: receives audio, never reference annotations.
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "mmathen/analysis/chords.h"
#include "mmathen/analysis/pitch.h"
#include "mmathen/sink/wav_reader.h"

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "Usage: real_audio_probe input.wav output.json chords|pitch\n";
    return 2;
  }
  const std::string mode = argv[3];
  if (mode != "chords" && mode != "pitch") return 2;
  const auto wav = mmathen::readWavFile(argv[1]);
  if (!wav.ok() || wav.value().channels != 1) {
    std::cerr << "Cannot read supported mono WAV: " << argv[1] << '\n';
    return 1;
  }
  const auto& w = wav.value();
  std::ofstream out(argv[2]);
  if (!out) return 1;
  out << std::setprecision(17) << "{\"sampleRate\":" << w.rate.value()
      << ",\"samples\":" << w.frames << ",\"outOfRange\":" << w.outOfRange;
  if (mode == "chords") {
    const mmathen::ChordOptions opts;  // Frozen existing defaults; no fitting.
    const auto result = mmathen::recognizeChords(w.samples, w.rate, opts);
    if (!result.ok()) return 1;
    const auto& r = result.value();
    out << ",\"config\":{\"frameSize\":" << opts.chroma.frameSize
        << ",\"hop\":" << opts.chroma.hop
        << ",\"fMinHz\":" << opts.chroma.fMinHz
        << ",\"fMaxHz\":" << opts.chroma.fMaxHz
        << ",\"tuningHz\":" << opts.chroma.tuningHz
        << ",\"compression\":" << opts.chroma.compression
        << ",\"energyGateDb\":" << opts.chroma.energyGateDb
        << ",\"sharpness\":" << opts.sharpness
        << ",\"noChordPenalty\":" << opts.noChordPenalty
        << ",\"selfTransition\":" << opts.selfTransition
        << ",\"changeModel\":\"uniform\"},\"chords\":[";
    for (std::size_t i = 0; i < r.viterbi.size(); ++i) {
      if (i) out << ',';
      const double center = (static_cast<double>(i * opts.chroma.hop) +
                             0.5 * static_cast<double>(opts.chroma.frameSize)) /
                            w.rate.value();
      out << '[' << center << ',' << r.framewise[i] << ',' << r.viterbi[i] << ']';
    }
    out << ']';
  } else {
    // One fixed monophonic baseline, including its native voicing decision.
    constexpr std::size_t frameSize = 2048, hop = 512;
    const mmathen::PitchSearch opts;
    const auto result = mmathen::trackPitch(w.samples, w.rate, frameSize, hop,
                                           mmathen::PitchMethod::Yin, opts);
    if (!result.ok()) return 1;
    out << ",\"config\":{\"method\":\"YIN\",\"frameSize\":" << frameSize
        << ",\"hop\":" << hop << ",\"fMinHz\":" << opts.fMinHz
        << ",\"fMaxHz\":" << opts.fMaxHz
        << ",\"yinThreshold\":" << opts.yinThreshold << "},\"pitch\":[";
    bool first = true;
    for (const auto& f : result.value()) {
      if (!first) out << ',';
      first = false;
      out << '[' << f.timeSeconds << ',' << f.estimate.frequencyHz << ','
          << (f.estimate.voiced ? "true" : "false") << ',' << f.estimate.confidence << ']';
    }
    out << ']';
  }
  out << "}\n";
  out.close();
  return out.fail() ? 1 : 0;
}
