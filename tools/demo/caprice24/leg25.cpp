#include "mmathen/music/performance_dynamics.h"
#include "mmathen/instruments/bowing_gesture.h"
#include <mmathen/instruments/bowed_string.h>
#include <mmathen/instruments/violin.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <optional>
#include <string>
#include <mmathen/instruments/violin_body.h>
#include <mmathen/render/room_convolution.h>
#include <mmathen/music/expressive_timing.h>
using namespace mmathen;
int main(int argc,char**argv){
  const double FS=48000; auto vn=violin().value(); double total=std::atof(argv[3]); const bool art=argc>4 && std::atoi(argv[4]); const double hiT=argc>5?std::atof(argv[5]):0.25; const double bet=argc>6?std::atof(argv[6]):0.1;
  std::vector<double> mix((size_t)(total*FS),0.0); FILE* in=std::fopen(argv[1],"r");
  double s,d; int m,v; std::optional<OversampledBowedStringVoice> voice; int cur=-1; size_t pos=0; double strokeLeft=0;
  auto make=[&](int k){ auto c=vn.courses[k]; const double f=c.tuning.frequency.value(); auto ideal=courseString(c);
    auto loss=lossFromT60(ideal.value().speed,FrequencyHz{f},DurationSeconds{2.0},FrequencyHz{6*f},DurationSeconds{hiT}).value();
    const double mu=c.material.linearDensity.value(), Z=mu*std::sqrt(c.tension.value()/mu); const double fmax=2*Z*0.2/(0.1*0.5);
    (void)fmax; voice.emplace(OversampledBowedStringVoice::create(c,violinBowStroke(c,kViolinPlayingBeta,0.2),violinStringLoss(c).value(),0,4,0,SampleRateHz{FS},(std::uint64_t)(total*FS),4).value());
    BowExpression e = (argc>12 && std::atoi(argv[12]) && (argc>18?(std::atoi(argv[18])&1):1)) ? BowExpression{kViolinVibratoRateHz,kViolinVibratoCents,kViolinVibratoOnsetSeconds,kViolinVibratoRampSeconds,0.0,1.0,0.0} : BowExpression{6.0,15.0,0.3,0.3,0.0,1.0,0.0}; (void)voice->setExpression(e); if(argc>21 && std::atoi(argv[21])) voice->setBridgeForceOutput(true); };
  std::vector<ScoreNote> sc; while(std::fscanf(in,"%lf,%lf,%d,%d",&s,&d,&m,&v)==4) sc.push_back({s,d,m,v});
  TimingRules tr; tr.tempoScale=argc>7?std::atof(argv[7]):1.0; tr.phraseScoreSeconds=(argc>27?std::atof(argv[27]):8)*0.32258; tr.phraseArchDepth=argc>8?std::atof(argv[8]):0.0; tr.detachedRatio=argc>9?std::atof(argv[9]):1.0; const bool v6=argc>12 && std::atoi(argv[12]); if(v6){ tr.levelArchDb=argc>14?std::atof(argv[14]):4; tr.velocityDbPer64=argc>15?std::atof(argv[15]):6; tr.phraseVariation=argc>16?std::atof(argv[16]):0; tr.phraseLevelSdDb=argc>17?std::atof(argv[17]):0; const int vm=argc>18?std::atoi(argv[18]):7; tr.timingJitterFraction=(vm&2)?0.02:0; tr.intonationSdCents=(vm&4)?(argc>20?std::atof(argv[20]):4):0; tr.seed=1; tr.levelSmoothingSeconds=argc>22?std::atof(argv[22]):0; tr.levelFlattenSeconds=argc>23?std::atof(argv[23]):0; tr.levelFloorDb=argc>28?std::atof(argv[28]):0; }
  auto perf=applyExpressiveTiming(sc,tr); double snd=0, lvl=1, cof=0;
  for(const auto& pn:perf){ s=pn.onsetSeconds; d=pn.ioiSeconds; m=pn.midi; v=pn.velocity; snd=pn.soundSeconds; lvl=pn.level; cof=pn.centsOffset;
    const double f=440*std::pow(2.0,(m-69)/12.0); int k=3; while(k>0 && vn.courses[k].tuning.frequency.value()>f*1.0005) --k;
    size_t a=(size_t)(s*FS); if(a>mix.size()) break;
    if(voice && a>pos){ (void)voice->renderBlock(mix.data()+pos,FrameCount{a-pos}); pos=a; }
    if(k!=cur){ if(voice){ /* old string rings in its own buffer: lift it */ voice->sustainFor(0); std::vector<double> tail(std::min<size_t>((size_t)((argc>26?std::atof(argv[26]):0.5)*FS),mix.size()-pos)); (void)voice->renderBlock(tail.data(),FrameCount{tail.size()}); for(size_t i=0;i<tail.size();++i) mix[pos+i]+=tail[i]; }
      make(k); cur=k; strokeLeft=3.25; }
    else (void)voice->changeBow(0.03);
    voice->setForceJitter(argc>30?std::atof(argv[30]):0.0); (void)voice->setStoppedFrequency(f*std::pow(2.0,cof/1200.0)); if(argc>32 && std::atoi(argv[32])){ static BowingGestureModel gm((std::uint64_t)std::atoi(argv[32]), BowingGestureParams{.driftFraction=(argc>33?std::atof(argv[33]):0.02), .betaLagSeconds=(argc>34?std::atof(argv[34]):0.3)}); static std::vector<double> dynv; static size_t ni=0; if(dynv.empty()){ std::vector<DynamicsNote> dn; for(const auto& q:perf) dn.push_back({q.onsetSeconds,q.ioiSeconds,q.midi,q.velocity}); PerformanceDynamicsParams pp; if(argc>35) pp.archDepth=std::atof(argv[35]); if(argc>36) pp.highLoudPerOctave=std::atof(argv[36]); if(argc>37) pp.accentPerVelocity=std::atof(argv[37]); if(argc>38) pp.beatSeconds=std::atof(argv[38]); if(argc>39) pp.phraseLevelSd=std::atof(argv[39]); if(argc>40) pp.flattenSeconds=std::atof(argv[40]); if(argc>41) pp.strongBeatWeight=std::atof(argv[41]); if(argc>42) pp.archShapeSd=std::atof(argv[42]); if(argc>43) pp.stringCrossingDip=std::atof(argv[43]); if(argc>44) pp.softClipWidth=std::atof(argv[44]); if(argc>45) pp.bowAlternationWeight=std::atof(argv[45]); if(argc>46) pp.shortNoteAccent=std::atof(argv[46]); dynv=performanceDynamics(dn,pp,(std::uint64_t)std::atoi(argv[32])); } const double dyn=dynv[std::min(ni++,dynv.size()-1)]; const BowStroke gs=gm.strokeForNote(vn.courses[k],dyn,d); const bool okp=voice->setBowPosition(gs.bridgeFraction,gs.normalForceNewtons); const bool oks=voice->setStroke(gs); static int bad=0; if(!(okp&&oks)&&bad++<3) std::fprintf(stderr,"gesture rejected p%d s%d beta %.3f\n",okp,oks,gs.bridgeFraction); } if(art){ auto ar=articulationForNote(d,v); const int md=argc>11?std::atoi(argv[11]):0; if(md&1) ar.bounce=false; if(md&2) ar.restDampT60=0; if(md&4){ ar.contactSeconds=0; ar.bounce=false; ar.restDampT60=0;} if(md&8) ar.accentGain=1.0; if(ar.contactSeconds==0.0 && snd<d) ar.contactSeconds=snd; ar.level=lvl; if(argc>25 && !ar.bounce){ ar.noteSeconds=snd; ar.taperSeconds=std::min(std::atof(argv[24]),0.5*snd); ar.taperLevel=std::atof(argv[25]); } if(argc>19 && std::atoi(argv[19])){ const double ldb=20*std::log10(lvl); const double b=violinBetaForLevelDb(ldb); const auto cc=vn.courses[cur]; const double mu=cc.material.linearDensity.value(), Z=mu*std::sqrt(cc.tension.value()/mu); const double fmx=2*Z*0.2/(b*0.5); (void)voice->setBowPosition(b,kViolinBowForceFraction*fmx); } (void)voice->articulate(ar);}
    // long note: bow change when the 0.65 m stroke runs out
    double t=0; while(d-t>strokeLeft){ size_t b=a+(size_t)((t+strokeLeft)*FS); (void)voice->renderBlock(mix.data()+pos,FrameCount{b-pos}); pos=b; (void)voice->changeBow(0.05); t+=strokeLeft; strokeLeft=3.25; }
    strokeLeft-= (d-t);
  }
  if(pos<mix.size()) (void)voice->renderBlock(mix.data()+pos,FrameCount{mix.size()-pos});
  if(argc>29 && std::atoi(argv[29])){ BridgeHillFilter bh(FS); bh.processInPlace(mix); if(std::atoi(argv[29])>=2){ SignatureModeFilter sm(FS); sm.processInPlace(mix);} if(std::atoi(argv[29])>=3){ RadiationHighpass rh(FS); rh.processInPlace(mix);} }
  if(argc>10 && std::string(argv[10])!="none"){ FILE* fi=std::fopen(argv[10],"rb"); std::vector<double> ir; double q; while(std::fread(&q,8,1,fi)==1) ir.push_back(q); std::fclose(fi); if(argc>31 && std::atof(argv[31])>0) mix=studioRoom(mix,ir,2.43,std::atof(argv[31]),FS,kuttruffMeanFreePathMs(35.2)); else mix=mixRoom(mix,ir,argc>13?std::atof(argv[13]):1.0); }
  FILE* o=std::fopen(argv[2],"wb"); std::fwrite(mix.data(),8,mix.size(),o); std::fclose(o); printf("ok\n");
}

















