import numpy as n,soundfile as sf,librosa,subprocess,glob
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
fs=48000
def rd(p,a,b,norm=True):
  if p.endswith('.f64'): x=n.fromfile(p);r=48000
  else:
    x,r=sf.read(p);x=x.mean(1) if x.ndim>1 else x
  x=x[int(a*r):int(b*r)]
  if r!=fs: x=librosa.resample(x,orig_sr=r,target_sr=fs)
  if norm: x=x/(n.sqrt((x**2).mean())+1e-12)*0.08
  f=int(.02*fs);x[:f]*=n.linspace(0,1,f);x[-f:]*=n.linspace(1,0,f);return x
def cat(ps,a,b):
  y=n.concatenate([n.concatenate([rd(p,a,b,False),n.zeros(int(.3*fs))]) for p in ps]);return y/abs(y).max()*0.7
R='/workspace/band/ref/snare/'
sreal=cat([R+'Snare2-HitSN_v3_rr1_Sum.wav',R+'Snare2-HitSN_v5_rr1_Sum.wav',R+'Snare2-HitSN_v7_rr1_Sum.wav',R+'Snare2-HitNS_v5_rr1_Sum.wav'],0,1.2)
smod=cat(['snn_0.f64','snn_1.f64','snn_2.f64','snn_3.f64'],0,1.2)
segs=[('REAL cello','Commons: Bach Sarabande, public-domain recording',rd('/workspace/band/cello/sar_pd.wav',0.5,10.5)),
('MODEL deep cello','cello_voice.h: C2 D2 E2 G2 A2 C2, bow force 0.4, player intonation',rd('vdeep.f64',0.3,10.5)),
('REAL flute, soft','VSCO 2 CE Flute susNV A4 v1 (CC0)',rd('/workspace/band/ref/flute/LDFlute_susNV_A3_v1_1.wav',0.3,4.3)),
('MODEL flute, p','jet-drive flute, fluteVoicingFor("p"): aim + 1.5 kHz reflection corner',rd('v_fl_notes.f64',0.5,4.5)),
('REAL snare','VSCO 2 CE Snare: snares on v3, v5, v7; snares off v5 (CC0)',sreal),
('MODEL snare','snare_drum.h: stick 1, 3, 6 m/s; 3 m/s snares off (Bilbao 2012 wire contact)',smod),
('REAL classical guitar','Greeninger (CC0)',rd('/workspace/gp/ref.wav',0,12)),
('MODEL guitar, before','old harness: every string 2 g/m',rd('gtU.f64',0,12)),
('MODEL guitar, after','D\'Addario EJ45 chart tensions, mu = T/(2Lf)^2',rd('gtA.f64',0,12))]
gap=n.zeros(int(.6*fs));aud=[];lst=open('vid2/list.txt','w')
for i,(t,s,x) in enumerate(segs):
  fig=plt.figure(figsize=(12.8,7.2),dpi=100);fig.patch.set_facecolor('#10131a')
  fig.text(.5,.58,t,ha='center',color='#ffd27f' if t.startswith('REAL') else '#9fd8ff',fontsize=46,weight='bold')
  fig.text(.5,.42,s,ha='center',color='w',fontsize=16);fig.text(.5,.08,'mmath physics sound engine - PR #2 (no samples in the model)',ha='center',color='#888',fontsize=12)
  fig.savefig(f'vid2/c{i}.png');plt.close(fig)
  d=(len(x)+len(gap))/fs;lst.write(f"file 'c{i}.png'\nduration {d:.3f}\n");aud+=[x,gap]
lst.write(f"file 'c{len(segs)-1}.png'\n");lst.close()
a=n.concatenate(aud);a=a/abs(a).max()*0.8 if abs(a).max()>0.8 else a
sf.write('vid2/a.wav',a.astype(n.float32),fs,subtype='PCM_16');print('dur',len(a)/fs)
