import numpy as n,librosa,sys
from scipy.signal import find_peaks,stft
def an(path,fs=48000,lo=80,top=7):
  if path.endswith('.f64'): x=n.fromfile(path);r=fs
  else: x,r=librosa.load(path,sr=None,mono=True)
  i=n.argmax(abs(x)>0.05*abs(x).max());x=x[i:]
  s=x[:min(len(x),int(.6*r))];S=abs(n.fft.rfft(s*n.hanning(len(s))));f=n.fft.rfftfreq(len(s),1/r);L=20*n.log10(S/S.max()+1e-12)
  pk,_=find_peaks(L,prominence=10,distance=max(1,int(20/(f[1]))));pk=[p for p in pk if f[p]>lo and L[p]>-45];pk=sorted(sorted(pk,key=lambda p:-L[p])[:top])
  fr,t,Z=stft(x,r,nperseg=4096,noverlap=4096-512);A=20*n.log10(abs(Z)+1e-12)
  out=[]
  for p in pk:
    j=n.argmin(abs(fr-f[p]));tr=A[j];m=(t>0.08)&(t<min(t[-1],4))&(tr>tr.max()-45)
    t60=-60/n.polyfit(t[m],tr[m],1)[0] if m.sum()>4 else n.nan
    out.append((f[p],L[p],t60))
  S3=abs(n.fft.rfft(x[:int(.3*r)]))**2;f3=n.fft.rfftfreq(len(x[:int(.3*r)]),1/r);cen=(S3*f3).sum()/S3.sum()
  return out,cen,len(x)/r
if __name__=='__main__':
  for p in sys.argv[1:]:
    o,c,d=an(p);print(p.split('/')[-1],'cen %.0f dur %.1f'%(c,d));print('   '+' '.join('%.0f(%.0fdB,%.1fs)'%x for x in o))
