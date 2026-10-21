# Vibrato width per note, calibrated for the estimator (step 10).
# Usage: python3 vibcal.py stem.f64 part.csv cents rateHz onsetS [...]
# Instantaneous frequency of H1-H3 (band-pass, Hilbert, 20 ms smoothing,
# quadratic detrend) over 0.45 s .. end - 0.05 s of each note >= 0.7 s,
# divided by the same estimator on a synthetic tone with the set vibrato
# (onset, 0.25 s ramp) over the same window. vib.py (step 12) read short
# notes low: a perfect 9 c tone reads 4.7-6.0 c on 0.77 s notes.
import numpy as n,sys
from scipy.signal import butter,sosfiltfilt,hilbert
fs=48000
def ext(x,f0,h=1):
  sos=butter(4,[h*f0*0.9,h*f0*1.1],'band',fs=fs,output='sos');y=sosfiltfilt(sos,x)
  ph=n.unwrap(n.angle(hilbert(y)));fi=n.diff(ph)*fs/(2*n.pi)/h
  w=int(0.02*fs);fi=n.convolve(fi,n.ones(w)/w,'same')
  c=1200*n.log2(fi/n.median(fi));c=c[int(0.05*fs):-int(0.05*fs)]
  t=n.arange(len(c));c=c-n.polyval(n.polyfit(t,c,2),t)
  return n.sqrt(2)*c.std()
import warnings;warnings.filterwarnings('ignore')
fs=48000
def synth(f0,d,cents,rate=6.0,on=0.15,ramp=0.25):
  t=n.arange(int((d+0.2)*fs))/fs;tv=n.clip(t-on,0,None);r=n.clip(tv/ramp,0,1)
  c=cents*r*n.sin(2*n.pi*rate*tv);ph=2*n.pi*n.cumsum(f0*2**(c/1200))/fs
  return sum(n.sin(h*ph)/h for h in range(1,8))
def run(path,csv,cents,rate=6.0,on=0.15):
  x=n.fromfile(path);R=[]
  for l in open(csv).read().split():
    s,d,m,v=l.split(',');s,d,m=float(s),float(d),int(m)
    if d<0.7: continue
    f0=440*2**((m-69)/12);a,b=0.45,d-0.05;sy=synth(f0,d,cents,rate,on)
    seg=x[int((s+a)*fs):int((s+b)*fs)];ss=sy[int(a*fs):int(b*fs)]
    R.append([ext(seg,f0,h)/ext(ss,f0,h) for h in (1,2,3)]+[m])
  R=n.array(R);return R
for p,csv,c,r,o in [(a,b,float(c),float(r),float(o)) for a,b,c,r,o in zip(*[iter(sys.argv[1:])]*5)]:
  R=run(p,csv,c,r,o);print(f'{p[-22:]:22s} notes {len(R)}  width / set (estimator-calibrated): H1 {n.median(R[:,0]):.2f}  H2 {n.median(R[:,1]):.2f}  H3 {n.median(R[:,2]):.2f}  mean(H1..H3) {n.median(R[:,:3].mean(1)):.2f}')
  if len(sys.argv)<8:
    for row in R: print('   midi %d  H1 %.2f H2 %.2f H3 %.2f'%(row[3],row[0],row[1],row[2]))
