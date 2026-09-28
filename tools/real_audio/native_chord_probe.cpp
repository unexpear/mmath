// The recognizer sees audio only; scoring stays in a separate process.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mmathen/sink/wav_reader.h>
#include "../../app/guitar_chords.h"
int main(int argc,char** argv) {
  if (argc!=3) return 2;
  try {
    const auto w=mmathen::readWavFile(argv[1]);
    if (!w.ok() || w.value().channels!=1) throw std::runtime_error("Expected mono WAV");
    const auto& audio=w.value();const auto fs=audio.rate.value();
    const auto start=std::chrono::steady_clock::now();
    const auto candidate=app::guitar::recognize(audio.samples,audio.rate);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    const auto baseline=mmathen::recognizeChords(audio.samples,audio.rate,app::guitarChordOptions());
    if (!baseline.ok()) throw std::runtime_error("Baseline failed");
    std::ofstream out(argv[2]);if (!out) return 1;
    out<<std::setprecision(17)<<"{\"processingMs\":"<<elapsed<<",\"chords\":[";
    for (std::size_t i=0;i<candidate.frames.size();++i) {
      if (i) out<<',';
      const auto& c=candidate.frames[i];const auto b=baseline.value().viterbi[i];
      auto bl=mmathen::chordName(b);
      if (b<24) { if (b>=12) bl.pop_back(); bl+=b>=12?":min":":maj"; }
      out<<'['<<static_cast<double>(i*2048+3072)/fs<<','<<std::min(static_cast<double>(audio.frames)/fs,static_cast<double>(i*2048+5120)/fs)
         <<",\""<<c.label<<"\",\""<<bl<<"\","<<c.triad<<']';
    }
    out<<"]}\n";return out?0:1;
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
