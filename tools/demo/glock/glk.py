import numpy as n,soundfile as sf,sys
from scipy.signal import find_peaks,stft
NAMES=['G4','C5','G5','C6','G6','C7']
def feats(x,fs,f1):
  i=n.argmax(abs(x)>0.05*abs(x).max());x=x[i:]
  s=x[:int(.5*fs)];S=abs(n.fft.rfft(s*n.hanning(len(s))));f=n.fft.rfftfreq(len(s),1/fs);L=20*n.log10(S/S.max()+1e-12)
  def lev(fc):
    m=(f>fc*0.97)&(f<fc*1.03);j=n.argmax(S*m);return f[j],L[j]
  pk,_=find_peaks(L,prominence=12,distance=20);pk=[p for p in pk if f[p]>f1*0.9 and L[p]>-40];pk=sorted(pk,key=lambda p:-L[p])[:5]
  s3=x[:int(.3*fs)];S3=abs(n.fft.rfft(s3))**2;f3=n.fft.rfftfreq(len(s3),1/fs);cen=(S3*f3).sum()/S3.sum()
  fr,t,Z=stft(x,fs,nperseg=4096,noverlap=4096-512);A=20*n.log10(abs(Z)+1e-12);j=n.argmin(abs(fr-f1));tr=A[j];m=(t>0.1)&(t<min(t[-1],3.0))&(tr>tr.max()-50)
  t60=-60/n.polyfit(t[m],tr[m],1)[0]
  return sorted([(f[p],L[p]) for p in pk]),cen,t60
def main(model):
  y=n.fromfile(model);seg=len(y)//6
  for k,nm in enumerate(NAMES):
    f1=440*2**((79+[0,5,12,17,24,29][k]-69)/12)
    x,fs=sf.read(f'/workspace/band/ref/glock/glock_medium_{nm}.wav');x=x.mean(1)
    a=feats(x,fs,f1);b=feats(y[k*seg:(k+1)*seg],48000,f1)
    print(f"{nm}->{f1:.0f}Hz  REF cen {a[1]:5.0f} T60 {a[2]:4.1f}s  peaks "+' '.join(f"{p/f1:.2f}({l:.0f})" for p,l in a[0]))
    print(f"{'':12s}MOD cen {b[1]:5.0f} T60 {b[2]:4.1f}s  peaks "+' '.join(f"{p/f1:.2f}({l:.0f})" for p,l in b[0]))
main(sys.argv[1])
