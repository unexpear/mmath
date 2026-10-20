import numpy as n,soundfile as sf,librosa
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
fs=48000;NAMES=['G4','C5','G5','C6','G6','C7']
def seq(get):
  out=[]
  for k,nm in enumerate(NAMES):
    x=get(k,nm);i=n.argmax(abs(x)>0.05*abs(x).max());x=x[max(0,i-480):max(0,i-480)+int(2.2*fs)]
    x=x/(abs(x).max()+1e-12)*0.5;f=int(.05*fs);x[-f:]*=n.linspace(1,0,f);out+=[x,n.zeros(int(.25*fs))]
  return n.concatenate(out)
def ref(k,nm):
  x,r=sf.read(f'/workspace/band/ref/glock/glock_medium_{nm}.wav');x=x.mean(1);return librosa.resample(x,orig_sr=r,target_sr=fs)
y=n.fromfile('glA.f64');seg=len(y)//6
mod=lambda k,nm:y[k*seg:(k+1)*seg].copy()
segs=[('REAL glockenspiel','VSCO 2 CE glock_medium, sounding G5 C6 G6 C7 G7 C8 (CC0)',seq(ref)),
('MODEL glockenspiel','struck_bar.h: Timoshenko steel bars, Hertz mallet, dipole radiation',seq(mod))]
gap=n.zeros(int(.6*fs));aud=[];lst=open('vid5/list.txt','w')
for i,(t,s,x) in enumerate(segs):
  fig=plt.figure(figsize=(12.8,7.2),dpi=100);fig.patch.set_facecolor('#10131a')
  fig.text(.5,.58,t,ha='center',color='#ffd27f' if t.startswith('REAL') else '#9fd8ff',fontsize=46,weight='bold')
  fig.text(.5,.42,s,ha='center',color='w',fontsize=16);fig.text(.5,.08,'mmath physics sound engine - PR #2 (no samples in the model)',ha='center',color='#888',fontsize=12)
  fig.savefig(f'vid5/c{i}.png');plt.close(fig)
  d=(len(x)+len(gap))/fs;lst.write(f"file 'c{i}.png'\nduration {d:.3f}\n");aud+=[x,gap]
lst.write(f"file 'c{len(segs)-1}.png'\n");lst.close()
a=n.concatenate(aud);sf.write('vid5/a.wav',a.astype(n.float32),fs,subtype='PCM_16');print('dur',len(a)/fs)
