import numpy as n,soundfile as sf,librosa
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
fs=48000
def ld(p):
  if p.endswith('.f64'): return n.fromfile(p)
  x,r=librosa.load(p,sr=fs,mono=True);return x
def clip(x,d):
  i=n.argmax(abs(x)>0.05*abs(x).max());x=x[max(0,i-480):max(0,i-480)+int(d*fs)].copy();x=x/(abs(x).max()+1e-12)*0.5
  f=int(.08*fs);x[-f:]*=n.linspace(1,0,f);return n.concatenate([x,n.zeros(int(.25*fs))])
def segs(x,k,N): s=len(x)//N;return x[k*s:(k+1)*s]
cel=ld('cel.f64');tb=ld('tbF.f64');hb=ld('hb_2e9.f64')
C='/workspace/band/ref/cel/';T='/workspace/band/ref/tb/';H='/workspace/band/ref/hb/'
creal=n.concatenate([clip(ld(C+f),1.0) for f in ['cel_C_420494.mp3','cel_D_420492.mp3','cel_E_420491.mp3','cel_G_420497.mp3']])
cmod=n.concatenate([clip(segs(cel,k,6),1.0) for k in [2,3,4,5]])
treal=n.concatenate([clip(ld(T+f'TB_{nm}.wav'),4.0) for nm in ['C4','G4','C5','F5']])
tmod=n.concatenate([clip(segs(tb,k,4),4.0) for k in range(4)])
hreal=n.concatenate([clip(ld(H+'hb_F3_rung_557142.mp3'),3.0),clip(ld(H+'hb_g1_370007.mp3'),3.0)])
hmod=n.concatenate([clip(segs(hb,0,4),3.0),clip(segs(hb,2,4),3.0)])
S=[('REAL celesta','Freesound Macsat-Rd celesta C5 D5 E5 G5 (CC0)',creal),('MODEL celesta','celesta.h: steel bars, felt hammers, tuned resonators',cmod),
('REAL tubular bells','VSCO 2 CE TB_hit C4 G4 C5 F5 (CC0)',treal),('MODEL tubular bells','tubular_bells.h: brass tubes, mode 4 = 2 x note',tmod),
('REAL handbells','Freesound 557142 brass F3 (CC0); radwoc G6 (CC0)',hreal),('MODEL handbells','handbell.h: (2,0) + (3,0) = 3x, felt clapper',hmod)]
gap=n.zeros(int(.6*fs));aud=[];lst=open('vid6/list.txt','w')
for i,(t,s,x) in enumerate(S):
  fig=plt.figure(figsize=(12.8,7.2),dpi=100);fig.patch.set_facecolor('#10131a')
  fig.text(.5,.58,t,ha='center',color='#ffd27f' if t.startswith('REAL') else '#9fd8ff',fontsize=46,weight='bold')
  fig.text(.5,.42,s,ha='center',color='w',fontsize=16);fig.text(.5,.08,'mmath physics sound engine - PR #2 (no samples in the model)',ha='center',color='#888',fontsize=12)
  fig.savefig(f'vid6/c{i}.png');plt.close(fig)
  d=(len(x)+len(gap))/fs;lst.write(f"file 'c{i}.png'\nduration {d:.3f}\n");aud+=[x,gap]
lst.write(f"file 'c{len(S)-1}.png'\n");lst.close()
a=n.concatenate(aud);sf.write('vid6/a.wav',a.astype(n.float32),fs,subtype='PCM_16');print('dur',len(a)/fs)
