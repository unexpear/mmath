#pragma once

// VALIDATION ONLY: rejected on the September 23 reserved recordings. The
// desktop does not include or activate this candidate. Retained for audit.
// Original implementation of harmonic dictionary fitting, informed by Mauch &
// Dixon (ISMIR 2010). This is not a copy/port of the GPL Chordino source. Keep
// the established triad path; infer extensions and bass from nonnegative note
// activations. Offline only: tuning/whitening use the complete recording.
#include <numeric>
#include <stdexcept>
#include "analysis_profiles.h"

namespace app::harmonic {
struct Frame { std::string label; int triad = 24; };
struct Result { std::vector<Frame> frames; double tuningCents = 0; };
inline double sinc(double x) { return std::abs(x) < 1e-12 ? 1 : std::sin(std::numbers::pi*x)/(std::numbers::pi*x); }
struct Template { std::string label; int triad; std::array<double,12> chroma{}; int bass; bool extended; };
inline std::vector<Template> dictionary() {
  struct Quality { const char* name; std::vector<int> notes; bool minor; };
  const std::vector<Quality> qualities = {{"maj",{0,4,7},false},{"min",{0,3,7},true},
    {"7",{0,4,7,10},false},{"maj7",{0,4,7,11},false},{"min7",{0,3,7,10},true},
    {"maj6",{0,4,7,9},false},{"min6",{0,3,7,9},true}};
  constexpr const char* roots[] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
  constexpr const char* degrees[] = {"1","b2","2","b3","3","4","b5","5","b6","6","b7","7"};
  std::vector<Template> out;
  for (int root=0;root<12;++root) for (const auto& q:qualities) for (int inversion:q.notes) {
    Template t{std::string(roots[root])+":"+q.name,root+(q.minor?12:0),{},(root+inversion)%12,q.notes.size()>3};
    if (inversion) t.label += std::string("/")+degrees[inversion];
    for (int pc:q.notes) t.chroma[static_cast<std::size_t>((root+pc)%12)] = 1/std::sqrt(static_cast<double>(q.notes.size()));
    out.push_back(std::move(t));
  }
  out.push_back({"N",24,{},-1,false}); return out;
}

inline Result recognize(const std::vector<double>& audio, mmathen::SampleRateHz rate) {
  using namespace mmathen;
  const auto base = recognizeChords(audio,rate,guitarChordOptions());
  if (!base.ok()) throw std::runtime_error("Harmonic chord input is invalid");
  constexpr std::size_t n=8192, hop=2048, notes=60;
  const double fs=rate.value();
  const auto first=static_cast<std::size_t>(std::ceil(55*n/fs));
  const auto last=std::min(n/2,static_cast<std::size_t>(std::floor(5000*n/fs)));
  const auto bins=last-first+1, frames=base.value().viterbi.size();
  std::vector<double> window(n), mean(bins,0);
  std::vector<std::vector<double>> spectra(frames,std::vector<double>(bins));
  std::vector<std::complex<double>> fft(n);
  std::complex<double> tuning{};
  for (std::size_t i=0;i<n;++i) window[i]=.5-.5*std::cos(2*std::numbers::pi*static_cast<double>(i)/n);
  for (std::size_t frame=0;frame<frames;++frame) {
    for (std::size_t i=0;i<n;++i) fft[i]={frame*hop+i<audio.size()?audio[frame*hop+i]*window[i]:0,0};
    detail::fftInPlace(fft,false);
    auto& s=spectra[frame];
    for (std::size_t k=0;k<bins;++k) { s[k]=std::abs(fft[first+k]); mean[k]+=s[k]/static_cast<double>(frames); }
    const double threshold=.1 * *std::max_element(s.begin(),s.end());
    for (std::size_t k=1;k+1<bins;++k) if (s[k]>s[k-1] && s[k]>=s[k+1] && s[k]>threshold && static_cast<double>(k+first)*fs/n>=200) {
      const double a=std::log(std::max(s[k-1],1e-20)),b=std::log(s[k]),c=std::log(std::max(s[k+1],1e-20));
      const double offset=.5*(a-c)/(a-2*b+c);
      const double midi=69+12*std::log2((static_cast<double>(first+k)+offset)*fs/n/440);
      tuning+=std::polar(s[k],2*std::numbers::pi*midi);
    }
  }
  Result result; result.tuningCents=std::abs(tuning)>0?std::arg(tuning)*100/(2*std::numbers::pi):0;
  const double floor=.01 * *std::max_element(mean.begin(),mean.end());
  std::vector<double> whitening(bins);
  for (std::size_t k=0;k<bins;++k) whitening[k]=1/std::sqrt(std::max({mean[k],floor,1e-20}));
  std::array<std::vector<double>,notes> atoms;
  std::array<double,notes> norms{};
  for (std::size_t j=0;j<notes;++j) {
    atoms[j].resize(bins);
    const double hz=440*std::pow(2,(static_cast<double>(j)+36-69)/12+result.tuningCents/1200);
    for (std::size_t k=0;k<bins;++k) {
      double value=0;
      for (int h=1;h<=12;++h) {
        const double delta=static_cast<double>(first+k)-h*hz*n/fs;
        if (std::abs(delta)<3) value+=std::pow(.7,h-1)*std::abs(sinc(delta)+.5*sinc(delta-1)+.5*sinc(delta+1));
      }
      atoms[j][k]=value*whitening[k]; norms[j]+=atoms[j][k]*atoms[j][k];
    }
    norms[j]=std::sqrt(norms[j]);
    if (!(norms[j]>0)) throw std::runtime_error("Unsupported harmonic dictionary rate");
    for (auto& v:atoms[j]) v/=norms[j];
  }
  std::array<std::array<double,notes>,notes> gram{};
  for (std::size_t i=0;i<notes;++i) for (std::size_t j=0;j<notes;++j)
    gram[i][j]=std::inner_product(atoms[i].begin(),atoms[i].end(),atoms[j].begin(),0.0);
  const auto templates=dictionary(); const auto states=templates.size();
  std::vector<double> scores(states,0), next(states);
  std::vector<std::vector<std::size_t>> back(frames,std::vector<std::size_t>(states));
  for (std::size_t frame=0;frame<frames;++frame) {
    std::array<double,notes> x{},gradient{},rhs{};
    for (std::size_t j=0;j<notes;++j) for (std::size_t k=0;k<bins;++k) rhs[j]+=atoms[j][k]*spectra[frame][k]*whitening[k];
    gradient=rhs;
    const double tolerance=1e-10*std::max(1.0,*std::max_element(rhs.begin(),rhs.end()));
    // Cyclic coordinate minimization of ||Ax-y||^2 with x>=0. The independent
    // Python oracle uses scipy.optimize.nnls's active-set solver.
    bool converged=false;
    for (int iteration=0;iteration<2000;++iteration) {
      for (std::size_t j=0;j<notes;++j) {
        const double updated=std::max(0.0,x[j]+gradient[j]/gram[j][j]),change=updated-x[j];
        x[j]=updated;
        for (std::size_t k=0;k<notes;++k) gradient[k]-=gram[k][j]*change;
      }
      double violation=0;
      for (std::size_t j=0;j<notes;++j) violation=std::max(violation,x[j]>0?std::abs(gradient[j]):std::max(0.0,gradient[j]));
      if (violation<=tolerance) { converged=true; break; }
    }
    if (!converged) throw std::runtime_error("Harmonic note solver did not converge");
    std::array<double,12> chroma{},bass{};
    for (std::size_t j=0;j<notes;++j) {
      const double activation=x[j]/norms[j];
      chroma[j%12]+=activation; bass[j%12]+=activation*std::pow(2,-(static_cast<double>(j)+36-40)/12);
    }
    const auto normalize=[](auto& a) { double norm=std::sqrt(std::inner_product(a.begin(),a.end(),a.begin(),0.0)); for (auto& v:a) v/=std::max(norm,1e-20); };
    normalize(chroma); normalize(bass);
    const auto best=static_cast<std::size_t>(std::max_element(scores.begin(),scores.end())-scores.begin());
    for (std::size_t state=0;state<states;++state) {
      const auto& t=templates[state];
      const double emission=t.triad==static_cast<int>(base.value().viterbi[frame])
          ? 10*std::inner_product(chroma.begin(),chroma.end(),t.chroma.begin(),0.0)+(t.bass>=0?3*bass[static_cast<std::size_t>(t.bass)]:0)-(t.extended?.5:0)
          : -1e6;
      const bool stay=frame==0 || scores[state]>=scores[best]-3;
      back[frame][state]=stay?state:best;
      next[state]=(frame==0?0:stay?scores[state]:scores[best]-3)+emission;
    }
    const double maximum=*std::max_element(next.begin(),next.end());
    for (auto& v:next) v-=maximum;
    scores.swap(next);
  }
  result.frames.resize(frames);
  auto state=static_cast<std::size_t>(std::max_element(scores.begin(),scores.end())-scores.begin());
  for (std::size_t i=frames;i-- >0;) {
    result.frames[i]={templates[state].label,templates[state].triad}; state=back[i][state];
  }
  return result;
}
}
