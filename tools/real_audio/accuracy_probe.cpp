// Development sweep; input is audio only. Labels never enter this executable.
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mmathen/analysis/chords.h>
#include <mmathen/analysis/pyin.h>
#include <mmathen/sink/wav_reader.h>
#include "../../app/streaming_tuner.h"
#include "../../app/reference_demo.h"

using namespace mmathen;
int main(int argc,char** argv) {
  if(argc<4) return 2;
  auto wr=readWavFile(argv[1]);
  if(!wr.ok() || wr.value().channels!=1) return 1;
  const auto& w=wr.value();
  std::ofstream out(argv[2]);
  if(!out) return 1;
  out<<std::setprecision(17)<<"{\"sampleRate\":"<<w.rate.value()<<",\"samples\":"<<w.frames<<",\"variants\":{";
  bool first=true;
  const bool production=argc>5 && std::string(argv[5])=="production";
  const auto begin=[&](const std::string& id) { if(!first) out<<','; first=false; out<<'\"'<<id<<"\":["; };
  const auto enabled=[&](const std::string& id) { return argc==4 || id==argv[4] || (!production && (id=="4096-60" || id=="5000-100")); };
  if(std::string(argv[3])=="pitch") {
    for(std::size_t size:{2048u,3072u,4096u}) for(int jump:{60,120}) {
      const auto id=std::to_string(size)+"-"+std::to_string(jump);
      if(!enabled(id)) continue;
      begin(id);
      std::vector<std::vector<PitchCandidate>> history;
      std::vector<double> times,frame(size);
      app::StreamingTuner desktop;
      if(production) frame.resize(app::tunerFrameSize(w.rate.value()));
      PyinOptions po; po.fMinHz=50; po.fMaxHz=1500; po.maxJumpBins=jump;
      for(std::size_t tick=1;;++tick) {
        const auto end=static_cast<std::size_t>(std::llround(static_cast<double>(tick)*w.rate.value()/15));
        if(end>w.samples.size()) break;
        const auto count=std::min(end,frame.size());
        std::fill(frame.begin(),frame.end(),0);
        std::copy_n(w.samples.begin()+static_cast<std::ptrdiff_t>(end-count),count,frame.end()-static_cast<std::ptrdiff_t>(count));
        PitchEstimate e;
        if(production) e=desktop.process(frame,w.rate.value());
        else {
          auto c=pyinCandidates(frame,w.rate,po); if(!c.ok()) return 1;
          history.push_back(std::move(c).value()); times.push_back(static_cast<double>(tick)/15);
          if(history.size()>24) { history.erase(history.begin()); times.erase(times.begin()); }
          auto p=pyinDecode(history,times,w.rate,po); if(!p.ok()) return 1;
          e=p.value().back().estimate;
        }
        double ss=0; for(double x:frame) ss+=x*x;
        const double db=ss>0 ? 10*std::log10(ss/static_cast<double>(frame.size())) : -120;
        if(tick>1) out<<',';
        out<<'['<<static_cast<double>(end)/w.rate.value()<<','<<static_cast<double>(end-count)/w.rate.value()<<','<<e.frequencyHz<<','<<(e.voiced&&db>-55 ? "true" : "false")<<','<<e.confidence<<','<<db<<",0]";
      }
      out<<']';
    }
  } else if(production) {
    const auto m=app::reference::measure(w,true);
    const auto co=app::guitarChordOptions().chroma;
    begin(argv[4]);
    for(std::size_t i=0;i<m.chords.viterbi.size();++i) {
      if(i) out<<',';
      out<<'['<<(static_cast<double>(i*co.hop)+.5*static_cast<double>(co.frameSize))/w.rate.value()<<','<<m.chords.framewise[i]<<','<<m.chords.viterbi[i]<<']';
    }
    out<<']';
  } else {
    for(double maxHz:{1200.0,2500.0,5000.0}) {
      ChromaOptions co; co.fMaxHz=maxHz; co.compression=0;
      auto cg=chromagram(w.samples,w.rate,co); if(!cg.ok()) return 1;
      for(double compression:{1.0,10.0,30.0,100.0}) {
        const auto id=std::to_string(static_cast<int>(maxHz))+"-"+std::to_string(static_cast<int>(compression));
        if(!enabled(id)) continue;
        begin(id);
        std::vector<std::vector<double>> a(25,std::vector<double>(25,.1/24));
        for(std::size_t i=0;i<25;++i) a[i][i]=.9;
        auto hmm=makeHmm(std::vector<double>(25,1.0/25),a);
        LogMatrix emissions;
        std::vector<std::size_t> raw;
        for(std::size_t i=0;i<cg.value().frames.size();++i) {
          auto c=cg.value().frames[i];
          const double mx=*std::max_element(c.begin(),c.end());
          double norm=0;
          for(auto& v:c) { v=mx>0 ? std::log1p(compression*v/mx) : 0; norm+=v*v; }
          for(auto& v:c) v=norm>0 ? v/std::sqrt(norm) : 0;
          const auto scores=chordScores(c,cg.value().gated[i],.2);
          std::vector<double> e; for(double s:scores) e.push_back(10*(s-1));
          emissions.push_back(e);
          raw.push_back(static_cast<std::size_t>(std::max_element(scores.begin(),scores.end())-scores.begin()));
        }
        auto v=viterbi(hmm.value(),emissions); if(!v.ok()) return 1;
        for(std::size_t i=0;i<raw.size();++i) {
          if(i) out<<',';
          out<<'['<<(static_cast<double>(i*co.hop)+.5*static_cast<double>(co.frameSize))/w.rate.value()<<','<<raw[i]<<','<<v.value().states[i]<<']';
        }
        out<<']';
      }
    }
  }
  out<<"}}\n";
  return out ? 0 : 1;
}
