"""Glockenspiel mode 2 vs a second reference: University of Iowa MIS bells.plastic.mf (C5B5, C6B6,
C7B7, C8E8 .aif in /workspace/band/ref/iowa; theremin.music.uiowa.edu/MISbells.html). Prints the
mode 2 (2.5-3.1 f1) level re f1 and the 0-0.3 s centroid for sounding G5 C6 G6 C7 G7 C8, the
glock1 notes; with a model file (glock1 output) the same for the model."""
import soundfile as sf,numpy as n,sys
from scipy.signal import find_peaks
D='/workspace/band/ref/iowa/';T=[(784,'C5B5'),(1047,'C6B6'),(1568,'C6B6'),(2093,'C7B7'),(3136,'C7B7'),(4186,'C8E8')]
def m2(x,fs,f1):
  s=x[:int(.5*fs)];S=abs(n.fft.rfft(s*n.hanning(len(s))));f=n.fft.rfftfreq(len(s),1/fs)
  L=lambda a,b:20*n.log10(S[(f>a*f1)&(f<b*f1)].max()+1e-12)
  s3=x[:int(.3*fs)];S3=abs(n.fft.rfft(s3))**2;f3=n.fft.rfftfreq(len(s3),1/fs)
  return L(2.5,3.1)-L(0.97,1.04),(S3*f3).sum()/S3.sum(),f[(f>0.97*f1)&(f<1.04*f1)][n.argmax(S[(f>0.97*f1)&(f<1.04*f1)])]
REF=[]
for f1,fn in T:
  x,fs=sf.read(D+f'bells.plastic.mf.{fn}.aif');x=x.mean(1) if x.ndim>1 else x
  e=n.sqrt(n.convolve(x**2,n.ones(441)/441,'same'));L=20*n.log10(e+1e-9);pk,_=find_peaks(L,height=L.max()-35,distance=int(.8*fs),prominence=15)
  best=None
  for j in pk:
    s=x[j-200:j+int(.3*fs)];S=abs(n.fft.rfft(s*n.hanning(len(s))));f=n.fft.rfftfreq(len(s),1/fs);pf=f[n.argmax(S*(f>500)*(f<6000))]
    if abs(n.log2(pf/f1))<0.04: best=j;break
  seg=x[best-200:best+int(1.5*fs)];REF.append(m2(seg,fs,f1*1.012))
print('iowa plastic mf mode2-f1 dB:',' '.join('%+.0f'%r[0] for r in REF),' centroid',' '.join('%.0f'%r[1] for r in REF))
if len(sys.argv)>1:
  y=n.fromfile(sys.argv[1]);seg=len(y)//6;M=[]
  for k,(f1,_) in enumerate(T):
    s=y[k*seg:(k+1)*seg];i=n.argmax(abs(s)>0.05*abs(s).max());M.append(m2(s[i:],48000,f1))
  print('model mode2-f1 dB:          ',' '.join('%+.0f'%r[0] for r in M),' centroid',' '.join('%.0f'%r[1] for r in M))
