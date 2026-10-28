"""Double bass pizz vs a second reference: University of Iowa MIS (anechoic, Wurlitzer upright,
Neumann KM 84 at 5 ft, mf pizz; theremin.music.uiowa.edu/MISdoublebass.html). Notes are split
by onset and checked by steady YIN (|c| > 60 skipped). Onset YIN 25 ms frames at 0/20/40/60 ms,
steady YIN at 0.4 s, level 0-0.4 s re the median. Usage: bass_iowa.py [band1 binary] (refs in
/workspace/band/ref/iowa: Bass.pizz.mf.sulE.E1B1 / sulA.A1B1 / sulA.C2B2 / sulD.D2B2 / sulG.G2B2 .aiff)."""
import numpy as n,soundfile as sf,librosa,sys
from scipy.signal import find_peaks
D='/workspace/band/ref/iowa/'
def split(p,first,count):
  x,fs=sf.read(D+p);x=x.mean(1) if x.ndim>1 else x
  e=n.sqrt(n.convolve(x**2,n.ones(int(.01*fs))/int(.01*fs),'same'));L=20*n.log10(e+1e-9)
  d=n.diff(L,prepend=L[0]);pk,_=find_peaks(L,height=L.max()-30,distance=int(1.0*fs),prominence=15)
  out={}
  for i,j in enumerate(pk[:count]):
    a=j-int(.05*fs);seg=x[a:a+int(2.5*fs)];out[first+i]=(seg,fs)
  return out,len(pk)
REF={}
for p,first,cnt,sel in [('Bass.pizz.mf.sulE.E1B1.aiff',28,8,range(28,33)),('Bass.pizz.mf.sulA.A1B1.aiff',33,3,range(33,36)),
  ('Bass.pizz.mf.sulA.C2B2.aiff',36,12,range(36,38)),('Bass.pizz.mf.sulD.D2B2.aiff',38,10,range(38,43)),('Bass.pizz.mf.sulG.G2B2.aiff',43,5,range(43,48))]:
  o,np_=split(p,first,cnt);print(p,'onsets',np_,file=sys.stderr)
  for m in sel:
    if m in o: REF[m]=o[m]
import numpy as n,subprocess,librosa,sys
def yin(x,f0,W,fs):
  lo=int(fs/(f0*1.3));hi=int(fs/(f0/1.3))
  d=n.array([((x[:W]-x[t:t+W])**2).sum() for t in range(1,hi+1)]);c=d*n.arange(1,hi+1)/n.cumsum(d)
  t=lo-1+n.argmin(c[lo-1:])
  if 1<=t<len(c)-1: a,b,cc=c[t-1],c[t],c[t+1];t=t+0.5*(a-cc)/(a-2*b+cc)
  return 1200*n.log2(fs/(t+1)/f0)
def feats(x,fs,f0):
  on=n.argmax(abs(x)>0.05*abs(x).max());x=x[on:]
  P=[yin(x[int(a*fs):],f0,int(0.025*fs),fs) for a in (0.0,0.02,0.04,0.06)]
  st=yin(x[int(0.4*fs):],f0,int(0.1*fs),fs);lv=10*n.log10((x[:int(.4*fs)]**2).mean())
  return P,st,lv
bin_=sys.argv[1] if len(sys.argv)>1 else './band1'
rows=[]
for m,(r,rf) in sorted(REF.items()):
  f0=librosa.midi_to_hz(m);Pr,sr,lr=feats(r,rf,f0)
  if not n.isfinite(sr) or abs(sr)>60: print(librosa.midi_to_note(m),'ref steady',sr,'c: skipped (split)');continue
  open('one.csv','w').write(f"0.5000,0.5000,{m},100\n");subprocess.run([bin_,'bass','score','one.csv','one.f64','3'],capture_output=True)
  Pm,sm,lm=feats(n.fromfile('one.f64'),48000,f0);rows.append((m,Pm,Pr,sm,sr,lm,lr))
dl=n.array([r[5]-r[6] for r in rows]);dl-=n.median(dl)
for (m,Pm,Pr,sm,sr,lm,lr),d in zip(rows,dl):
  print(f"{librosa.midi_to_note(m):4s} onset 0/20/40/60ms model "+' '.join('%+4.0f'%v for v in Pm)+" | iowa "+' '.join('%+4.0f'%v for v in Pr)+f" | steady {sm:+3.0f}/{sr:+3.0f}  level {d:+5.1f} dB")
on=n.array([[r[1][1],r[2][1]] for r in rows]);print('median onset (20 ms) model %+.0f iowa %+.0f; level spread (std) %.1f dB'%(n.median(on[:,0]),n.median(on[:,1]),dl.std()))
