#pragma once

// Our offline guitar recognizer: four spectral bands, transposition-shared
// acoustic scores, then a uniform-change Viterbi path. Trained parameters have
// no dependence on a recording's name, reference labels, key or progression.
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include "analysis_profiles.h"
#include "guitar_chord_model.h"

namespace app::guitar {
inline constexpr std::size_t frameSize = 8192, hop = 2048;
using Features = std::array<double, 48>;
struct Frame { std::string label; int triad = 24; };
struct Result { std::vector<Frame> frames; };

inline Frame label(std::size_t state) {
  if (state == 12 * model::types.size()) return {"N",24};
  if (state > 12 * model::types.size()) throw std::invalid_argument("Invalid chord state");
  constexpr const char* roots[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
  const auto root = state / model::types.size();
  const auto& type = model::types[state % model::types.size()];
  return {std::string(roots[root])+":"+type.suffix,
          type.triad < 0 ? -1 : static_cast<int>(root)+type.triad};
}

inline std::vector<double> emissions(const Features& features, bool gated) {
  const auto states = 12 * model::types.size()+1;
  std::vector<double> out(states,-1e6);
  if (gated) { out.back()=0; return out; }
  for (std::size_t root=0; root<12; ++root) {
    Features x{};
    for (std::size_t j=0; j<48; ++j) x[j]=features[j/12*12+(j%12+root)%12];
    double quadratic=0;
    if constexpr (model::gaussian) {
      for (std::size_t i=0; i<48; ++i) for (std::size_t j=0; j<48; ++j)
        quadratic += .5*x[i]*model::precision[i*48+j]*x[j];
    }
    for (std::size_t type=0; type<model::types.size(); ++type) {
      const auto& w=model::weights[type]; double score=w[48]-quadratic;
      for (std::size_t j=0; j<48; ++j) score+=x[j]*w[j];
      out[root*model::types.size()+type]=score;
    }
  }
  return out;
}

inline Result recognize(const std::vector<double>& audio, mmathen::SampleRateHz rate) {
  const double fs=rate.value();
  if (audio.empty() || !std::isfinite(fs) || fs<=5000)
    throw std::invalid_argument("Invalid guitar recognition input");
  for (double v:audio) if (!std::isfinite(v)) throw std::invalid_argument("Non-finite guitar audio");
  constexpr auto n=frameSize;
  const auto count=audio.size()<=n?1:(audio.size()-n)/hop+1;
  const auto states=12*model::types.size()+1;
  std::vector<std::size_t> anchors;
  if constexpr (model::anchored) {
    auto options=guitarChordOptions();options.selfTransition=model::baseStay;
    auto base=mmathen::recognizeChords(audio,rate,options);
    if (!base.ok()) throw std::invalid_argument("Invalid guitar chord input");
    anchors=std::move(base).value().viterbi;
  }
  static_assert(12*model::types.size()+1 < 65536);
  std::array<double,n> window{};
  double windowEnergy=0;
  for (std::size_t i=0;i<n;++i) {
    window[i]=.5-.5*std::cos(2*std::numbers::pi*static_cast<double>(i)/n);
    windowEnergy+=window[i]*window[i];
  }
  std::array<int,n/2+1> pitchClass{},band{};pitchClass.fill(-1);
  for (std::size_t k=1;k<=n/2;++k) {
    const double hz=static_cast<double>(k)*fs/n;
    if (hz<80 || hz>2500) continue;
    pitchClass[k]=static_cast<int>(std::floor(69+12*std::log2(hz/440)+.5))%12;
    band[k]=hz<=300?1:hz<=1000?2:3;
  }
  std::vector<std::complex<double>> fft(n);
  std::vector<std::vector<std::uint16_t>> back(count,std::vector<std::uint16_t>(states));
  std::vector<double> scores(states,0),next(states);
  for (std::size_t frame=0;frame<count;++frame) {
    for (std::size_t i=0;i<n;++i) fft[i]={frame*hop+i<audio.size()?audio[frame*hop+i]*window[i]:0,0};
    mmathen::detail::fftInPlace(fft,false);
    Features x{};double power=0;
    for (std::size_t k=1;k<=n/2;++k) if (pitchClass[k]>=0) {
      const double p=std::norm(fft[k])/(n*windowEnergy/4);
      const auto pc=static_cast<std::size_t>(pitchClass[k]);
      x[pc]+=p; x[static_cast<std::size_t>(band[k])*12+pc]+=p; power+=p;
      // Python's inclusive band boundaries intentionally share exact edge bins.
      const double hz=static_cast<double>(k)*fs/n;
      if (hz==300) x[24+pc]+=p;
      if (hz==1000) x[36+pc]+=p;
    }
    for (std::size_t b=0;b<4;++b) {
      double maximum=0,norm=0;
      for (std::size_t j=0;j<12;++j) maximum=std::max(maximum,x[b*12+j]);
      for (std::size_t j=0;j<12;++j) {
        auto& v=x[b*12+j];v=std::log1p(10*v/std::max(maximum,1e-20));norm+=v*v;
      }
      for (std::size_t j=0;j<12;++j) x[b*12+j]/=std::max(std::sqrt(norm),1e-20);
    }
    auto row=emissions(x,power<1e-6);
    if constexpr (model::anchored) {
      for(std::size_t state=0;state+1<states;++state) {
        const auto& type=model::types[state%model::types.size()];
        const int triad=type.triad<0?-1:static_cast<int>(state/model::types.size())+type.triad;
        if (triad!=static_cast<int>(anchors[frame])) row[state]=-1e6;
        if (std::string_view(type.suffix)!="maj" && std::string_view(type.suffix)!="min") row[state]-=model::detailPenalty;
      }
      row.back()=anchors[frame]==24?0:-1e6;
    }
    const auto best=static_cast<std::size_t>(std::max_element(scores.begin(),scores.end())-scores.begin());
    for (std::size_t state=0;state<states;++state) {
      const bool stay=frame==0 || scores[state]>=scores[best]-model::switchCost;
      back[frame][state]=static_cast<std::uint16_t>(stay?state:best);
      next[state]=(frame==0?0:stay?scores[state]:scores[best]-model::switchCost)+row[state];
    }
    const double maximum=*std::max_element(next.begin(),next.end());
    for (auto& v:next) v-=maximum;
    scores.swap(next);
  }
  Result result;result.frames.resize(count);
  auto state=static_cast<std::size_t>(std::max_element(scores.begin(),scores.end())-scores.begin());
  for (std::size_t i=count;i-- >0;) { result.frames[i]=label(state);state=back[i][state]; }
  return result;
}
} // namespace app::guitar
