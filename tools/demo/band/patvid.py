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
R='/workspace/band/ref/snare/'
sreal=cat([R+'Snare2-HitSN_v3_rr1_Sum.wav',R+'Snare2-HitSN_v5_rr1_Sum.wav',R+'Snare2-HitSN_v7_rr1_Sum.wav',R+'Snare2-HitNS_v5_rr1_Sum.wav'],0,1.2)
segs=[('REAL flute','VSCO 2 CE Flute susNV A4 v3 + E4 v3 (CC0)',n.concatenate([rd('/workspace/band/ref/flute/LDFlute_susNV_A3_v3_1.wav',0.2,3.2),rd('/workspace/band/ref/flute/LDFlute_susNV_E4_v3_1.wav',0.2,3.2)])),
('MODEL flute','Patapan melody stem (jet-drive flute, mf)',rd('pstem_fl.wav',1.6,11.6)),
('REAL cello','Commons: Bach Sarabande, public-domain recording',rd('/workspace/band/cello/sar_pd.wav',0.5,8.5)),
('MODEL deep cello','Patapan bass stem (cello_voice.h)',rd('pstem_vc.wav',1.6,11.6)),
('REAL classical guitar','Greeninger (CC0)',rd('/workspace/gp/ref.wav',0,8)),
('MODEL guitar','Patapan inner-voice stem (EJ45 strings)',rd('pstem_gt.wav',1.6,11.6)),
('REAL snare','VSCO 2 CE Snare, snares on v3 / v5 / v7 (CC0)',sreal),
('MODEL snare','Patapan snare stem (snare_drum.h)',rd('pstem_sd.wav',0,10)),
('MODEL band: Patapan','La Monnoye 1720, Charles Wood SATB (PD): flute, deep cello, guitar, snare',None)]
b,_=sf.read('patapan_band1.wav');segs[-1]=(segs[-1][0],segs[-1][1],b)
gap=n.zeros(int(.6*fs));aud=[];lst=open('vid3/list.txt','w')
for i,(t,s,x) in enumerate(segs):
  fig=plt.figure(figsize=(12.8,7.2),dpi=100);fig.patch.set_facecolor('#10131a')
  fig.text(.5,.58,t,ha='center',color='#ffd27f' if t.startswith('REAL') else '#9fd8ff',fontsize=46,weight='bold')
  fig.text(.5,.42,s,ha='center',color='w',fontsize=16);fig.text(.5,.08,'mmath physics sound engine - PR #2 (no samples in the model)',ha='center',color='#888',fontsize=12)
  fig.savefig(f'vid3/c{i}.png');plt.close(fig)
  d=(len(x)+len(gap))/fs;lst.write(f"file 'c{i}.png'\nduration {d:.3f}\n");aud+=[x,gap]
lst.write(f"file 'c{len(segs)-1}.png'\n");lst.close()
a=n.concatenate(aud);a=a/abs(a).max()*0.8 if abs(a).max()>0.8 else a
sf.write('vid3/a.wav',a.astype(n.float32),fs,subtype='PCM_16');print('dur',len(a)/fs)
