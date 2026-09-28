// Exercise the production playback callback through SDL conversion and gain.
#include "../../app/audio_output.h"
#include <mmathen/sink/wav_reader.h>
#include <iostream>
#include <stdexcept>

void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main(int argc,char** argv) {
  if(argc!=2) return 2;
  try {
    using namespace app;
    auto wr=readWavFile(argv[1]); check(wr.ok(),"read real recording");
    const auto& w=wr.value(); check(w.channels==1,"mono input");
    auto engine=std::make_unique<Engine>();
    auto& e=*engine;
    e.trackFrames=w.frames;
    for(double v:w.samples) { e.track.push_back(static_cast<float>(v)); e.track.push_back(static_cast<float>(v)); }
    auto reader=SpliceReader<float>::create(2,220,FadeCurve::EqualPower);
    check(reader.ok(),"reader"); e.reader=std::move(reader).value();
    e.reader->setSource(e.track.data(),e.trackFrames);
    auto ring=SpscRing::create(std::size_t{1}<<17); check(ring.ok(),"ring"); e.ring=std::move(ring).value();
    const SDL_AudioSpec spec{SDL_AUDIO_F32,2,static_cast<int>(w.rate.value())};
    auto* stream=SDL_CreateAudioStream(&spec,&spec); check(stream!=nullptr,"SDL stream");
    AudioOutput output;
    check(restoreFileSource(e,stream,w.rate.value()),"restore file");
    e.playing=true;
    const auto render=[&]() {
      SDL_ClearAudioStream(stream);
      e.playback(stream,8192*static_cast<int>(sizeof(float))*2);
      std::vector<float> samples(16384);
      const int bytes=SDL_GetAudioStreamData(stream,samples.data(),static_cast<int>(samples.size()*sizeof(float)));
      check(bytes>0,"callback generated output"); samples.resize(static_cast<std::size_t>(bytes)/sizeof(float));
      double energy=0;
      for(float x:samples) { check(std::isfinite(x)&&std::abs(x)<=1,"finite unclipped output"); energy+=x*x; }
      return energy/static_cast<double>(samples.size());
    };
    check(output.apply(stream),"gain"); check(render()>1e-7,"audible output");
    output.muted=true; check(output.apply(stream),"mute"); check(render()==0,"mute is silent after SDL");
    check(e.ring->available()>0,"muted analysis still receives audio");
    output.muted=false; check(output.apply(stream),"unmute"); check(render()>1e-7,"unmute recovers sound");
    e.mode=kMic; check(e.buildProcessors(48000),"mic format");
    const auto position=e.reader->position();
    e.loopA=100; e.loopB=10000;
    check(restoreFileSource(e,stream,w.rate.value()),"mic to file restoration");
    SDL_AudioSpec input{}; check(SDL_GetAudioStreamFormat(stream,&input,nullptr),"get format");
    check(e.mode==kFile && input.freq==static_cast<int>(w.rate.value()) && e.rate==w.rate.value(),"file rate and callback restored");
    check(e.reader->position()==position && e.loopA==100 && e.loopB==10000,"position and loop preserved");
    check(!e.playing,"return to file paused");
    e.playing=true; check(render()>1e-7,"play after mic is audible");
    e.seekTo=1000; check(render()>1e-7,"seek remains audible");
    e.playing=false; (void)render();
    SDL_ClearAudioStream(stream); e.playback(stream,4096);
    check(SDL_GetAudioStreamAvailable(stream)==0,"pause stops producing audio after ramp");
    e.playing=true; check(render()>1e-7,"resume");
    output.volume=.5f; check(output.apply(stream),"volume");
    // The exact same samples at unity and half gain test SDL's final output.
    const std::array<float,4> signal{.2f,-.4f,.3f,-.1f}; std::array<float,4> actual{};
    SDL_ClearAudioStream(stream); check(SDL_PutAudioStreamData(stream,signal.data(),sizeof(signal)),"queue samples");
    check(SDL_GetAudioStreamData(stream,actual.data(),sizeof(actual))==sizeof(actual),"read samples");
    for(std::size_t i=0;i<4;++i) check(std::abs(actual[i]-signal[i]*.5f)<1e-7f,"volume applies exact half gain");
    SDL_DestroyAudioStream(stream);
    std::cout<<"PASS: real recording output, mute/unmute, analysis tap, 48k mic to file rate, position/loop, play/pause/seek/resume, half-volume\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
