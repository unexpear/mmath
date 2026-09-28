// Audio only; independent Python evaluator owns all reference annotations.
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mmathen/sink/wav_reader.h>
#include "../../app/harmonic_chords.h"
#include "../../app/precision_tuner.h"

int main(int argc,char** argv) {
  if (argc!=4) return 2;
  try {
    const auto wr=mmathen::readWavFile(argv[1]);
    if (!wr.ok() || wr.value().channels!=1) return 1;
    const auto& w=wr.value(); const double fs=w.rate.value();
    std::ofstream out(argv[2]); if (!out) return 1;
    out<<std::setprecision(17)<<"{\"sampleRate\":"<<fs<<",\"samples\":"<<w.frames;
    const auto started=std::chrono::steady_clock::now();
    if (std::string(argv[3])=="chords") {
      const auto result=app::harmonic::recognize(w.samples,w.rate);
      out<<",\"tuningCents\":"<<result.tuningCents<<",\"chords\":[";
      for (std::size_t i=0;i<result.frames.size();++i) {
        if (i) out<<',';
        out<<'['<<static_cast<double>(i*2048+3072)/fs<<','<<std::min(static_cast<double>(w.frames)/fs,static_cast<double>(i*2048+5120)/fs)
           <<",\""<<result.frames[i].label<<"\"]";
      }
      out<<']';
    } else if (std::string(argv[3])=="pitch" || std::string(argv[3])=="music-pitch") {
      const auto range=std::string(argv[3])=="music-pitch" ? app::PitchRange::Music : app::PitchRange::Guitar;
      app::PrecisionTuner detector;
      std::vector<double> frame(app::precisionHistorySize(fs,range));
      out<<",\"pitch\":[";
      for (std::size_t tick=1;;++tick) {
        const auto end=static_cast<std::size_t>(std::llround(static_cast<double>(tick)*fs/15)); if (end>w.samples.size()) break;
        std::fill(frame.begin(),frame.end(),0); const auto count=std::min(end,frame.size());
        std::copy_n(w.samples.begin()+static_cast<std::ptrdiff_t>(end-count),count,frame.end()-static_cast<std::ptrdiff_t>(count));
        const auto begin=std::chrono::steady_clock::now();
        const auto p=detector.process(frame,fs,range);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        if (tick>1) out<<',';
        out<<'['<<static_cast<double>(end)/fs<<','<<static_cast<double>(end-count)/fs<<','<<p.frequencyHz<<','
           <<(p.voiced&&detector.levelDb>-55?"true":"false")<<','<<p.confidence<<','<<detector.levelDb<<','<<ms<<','
           <<detector.raw.frequencyHz<<','<<(detector.refined?"true":"false")<<']';
      }
      out<<']';
    } else return 2;
    out<<",\"processingMs\":"<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<"}\n";
    return out?0:1;
  } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
