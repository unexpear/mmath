"""Sleigh-bell scoreboard: clips vs CC0 ref. Usage: sbscore.py out.png title ref.wav name=clip ..."""
import sys,numpy as n,soundfile as sf,librosa
from scipy.signal import find_peaks,butter,sosfiltfilt
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
def load(p):
  if p.endswith('.f64'): x=n.fromfile(p);fs=44100
  else: x,fs=sf.read(p); x=x.mean(1) if x.ndim>1 else x
  if fs!=44100: x=librosa.resample(x,orig_sr=fs,target_sr=44100);fs=44100
  return x/ (n.sqrt((x**2).mean())+1e-20),fs
def m(x,fs):
  P=n.abs(librosa.stft(x.astype(n.float32),n_fft=8192))**2;f=librosa.fft_frequencies(sr=fs,n_fft=8192);S=P.mean(1)
  d={};d['Spectral centroid (Hz)']=(f*S).sum()/S.sum()
  for lo,hi in [(0,2000),(2000,4000),(4000,8000),(8000,16000),(16000,22050)]:
    k=(f>=lo)&(f<hi);d[f'Band {lo/1000:g}-{hi/1000:g} kHz share (dB)']=10*n.log10(S[k].sum()/S.sum())
  db=10*n.log10(S+1e-30);pk,_=find_peaks(db,prominence=6,distance=20);top=sorted(pk,key=lambda i:-db[i])[:7]
  d['_peaks']=sorted(f[top])
  d['Spectral flatness 2-16 kHz']=float(n.exp(n.mean(n.log(S[(f>2000)&(f<16000)])))/n.mean(S[(f>2000)&(f<16000)]))
  on=librosa.onset.onset_detect(y=x.astype(n.float32),sr=fs,units='time');d['Onset rate (/s)']=len(on)/(len(x)/fs)
  env=n.abs(librosa.util.frame(x,frame_length=441,hop_length=441)).max(0);E=n.abs(n.fft.rfft(env-env.mean()));fe=n.fft.rfftfreq(len(env),0.01)
  k=(fe>1)&(fe<20);d['Envelope modulation peak (Hz)']=fe[k][E[k].argmax()]
  L=20*n.log10(env+1e-9);d['Envelope p90-p10 (dB)']=n.percentile(L,90)-n.percentile(L,10)
  h=int(0.1*fs);w=3*fs;s=[10*n.log10(n.mean(x[i:i+w]**2)) for i in range(0,len(x)-w,h)];d['3 s loudness std (dB)']=n.std(s)
  d['Crest factor (dB)']=20*n.log10(abs(x).max()/n.sqrt((x**2).mean()))
  for fc in [2514,6659,10228]:
    y=sosfiltfilt(butter(2,[fc*0.94,fc*1.06],'band',fs=fs,output='sos'),x);e=20*n.log10(n.abs(librosa.util.frame(y,frame_length=441,hop_length=441)).max(0)+1e-9)
    sl=[n.polyfit(n.arange(10)*0.01,e[i:i+10],1)[0] for i in range(len(e)-10)];sl=n.array(sl);sl=sl[sl<0]
    d[f'Free decay near {fc/1000:.1f} kHz (dB/s, p75)']=n.percentile(sl,75)
  return d
ref=sys.argv[3];clips=[('real',ref)]+[tuple(a.split('=',1)) for a in sys.argv[4:]]
R={nm:m(*load(p)) for nm,p in clips}
rp=R['real']['_peaks']
for nm in R:
  pk=R[nm]['_peaks'];R[nm]['Ref peaks matched within 3% (of 7)']=sum(any(abs(q/p-1)<0.03 for q in pk) for p in rp)
keys=[k for k in R['real'] if not k.startswith('_')]
names=[nm for nm,_ in clips]
rows=[];cols=[]
for k in keys:
  rv=R['real'][k];row=[k]+[f'{R[nm][k]:.3g}' for nm in names];rows.append(row)
  c=['w']
  for nm in names:
    v=R[nm][k]
    if nm=='real': c.append('#dddddd');continue
    if 'dB' in k or 'matched' in k: e=abs(v-rv); g=e<=1.5 if 'matched' not in k else e<=1; y=e<=4 if 'matched' not in k else e<=3
    else: e=abs(v-rv)/(abs(rv)+1e-9); g=e<=0.15; y=e<=0.5
    c.append('#b6e3b6' if g else '#fff1a8' if y else '#f4b6b6')
  cols.append(c)
for r in rows: print(' | '.join(r))
print('peaks', {nm:[round(p) for p in R[nm]['_peaks']] for nm in names})
fig,ax=plt.subplots(figsize=(4+2.2*len(names),0.42*len(rows)+1.2));ax.axis('off')
t=ax.table(cellText=rows,colLabels=['metric']+names,cellColours=cols,loc='center',cellLoc='center');t.auto_set_font_size(False);t.set_fontsize(9);t.scale(1,1.4)
t.auto_set_column_width(list(range(len(names)+1)))
ax.set_title(sys.argv[2],fontsize=10);plt.savefig(sys.argv[1],dpi=130,bbox_inches='tight')
