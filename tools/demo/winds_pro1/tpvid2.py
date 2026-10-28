import numpy as n,soundfile as sf,librosa
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
fs=48000
r,sr=sf.read('ref_trumpet.wav');r=r.mean(1) if r.ndim>1 else r;r=librosa.resample(r,orig_sr=sr,target_sr=fs)
def nm(x): return x/(abs(x).max()+1e-12)*0.5
S=[('REAL trumpet','Lick Trumpet.wav (ChickSR, CC BY-SA 4.0, Wikimedia Commons)',r),
('MODEL: default','linear bore + the old four-sample advance',n.fromfile('default_trumpet.f64')),
('MODEL: + Burgers steepening','opt-in: Godunov Burgers over 0.45 L (Hirschberg 1996, Myers 2012)',n.fromfile('burgers_trumpet.f64'))]
gap=n.zeros(int(.7*fs));aud=[];L=open('list.txt','w')
for i,(t,s,x) in enumerate(S):
  fig=plt.figure(figsize=(12.8,7.2),dpi=100);fig.patch.set_facecolor('#10131a')
  fig.text(.5,.58,t,ha='center',color='#ffd27f' if t.startswith('REAL') else '#9fd8ff',fontsize=40,weight='bold')
  fig.text(.5,.42,s,ha='center',color='w',fontsize=16);fig.text(.5,.08,'mmath physics sound engine - PR #2 - same transcribed phrase',ha='center',color='#888',fontsize=12)
  fig.savefig(f'c{i}.png');plt.close(fig);x=nm(x);aud+=[x,gap];L.write(f"file 'c{i}.png'\nduration {(len(x)+len(gap))/fs:.3f}\n")
L.write(f"file 'c{len(S)-1}.png'\n");L.close();sf.write('a.wav',n.concatenate(aud).astype(n.float32),fs,subtype='PCM_16')
