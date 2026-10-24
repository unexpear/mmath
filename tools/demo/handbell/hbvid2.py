import numpy as n,soundfile as sf,librosa
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
fs=48000
def clip(x,d):
  i=n.argmax(abs(x)>0.05*abs(x).max());x=x[max(0,i-480):max(0,i-480)+int(d*fs)].copy();x=x/(abs(x).max()+1e-12)*0.5
  f=int(.08*fs);x[-f:]*=n.linspace(1,0,f);return x
r,_=librosa.load('/workspace/band/ref/hb/hb_F3_rung_557142.mp3',sr=fs)
a=n.fromfile('hs90.f64');b=n.fromfile('hs30.f64');s=len(a)//4
S=[('REAL handbell F3','Freesound 557142, brass F3, felt clapper (CC0)',clip(r,4.5)),('MODEL F3, side on','listener 90 deg from the bell axis',clip(a[:s],4.5)),('MODEL F3, 30 deg off axis','direct radiation ~ sin^m; breathing 2f is omni',clip(b[:s],4.5))]
gap=n.zeros(int(.6*fs));aud=[];L=open('vid10/list.txt','w')
for i,(t,sub,x) in enumerate(S):
  fig=plt.figure(figsize=(12.8,7.2),dpi=100);fig.patch.set_facecolor('#10131a')
  fig.text(.5,.58,t,ha='center',color='#ffd27f' if t.startswith('REAL') else '#9fd8ff',fontsize=40,weight='bold')
  fig.text(.5,.42,sub,ha='center',color='w',fontsize=16);fig.text(.5,.08,'mmath physics sound engine - PR #2 (no samples in the model)',ha='center',color='#888',fontsize=12)
  fig.savefig(f'vid10/c{i}.png');plt.close(fig);aud+=[x,gap];L.write(f"file 'c{i}.png'\nduration {(len(x)+len(gap))/fs:.3f}\n")
L.write(f"file 'c{len(S)-1}.png'\n");L.close();sf.write('vid10/a.wav',n.concatenate(aud).astype(n.float32),fs,subtype='PCM_16')
