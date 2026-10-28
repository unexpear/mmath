import sys,glob,numpy as np;sys.path.insert(0,'/workspace/vt32');from per import *
def lu(x,fs=48000):
    n=int(.4*fs);h=int(.1*fs);m=np.array([10*np.log10(np.mean(x[i:i+n]**2)+1e-20) for i in range(0,len(x)-n,h)]);m=m[m>m.max()-40];return np.percentile(m,95)-np.percentile(m,10)
def rep(name,x,fs):
    L,C=feats(x,fs);a=ac(L);b=ac(C);lo=6;mx=lo+np.argmax(a[lo:80]);mc=lo+np.argmax(b[lo:80])
    print(f'{name:22s} LU {lu(x,fs):5.1f} | loud ac max {a[mx]:+.2f}@{mx*.05:.2f}s @1.95 {a[39]:+.2f} | cent ac max {b[mc]:+.2f}@{mc*.05:.2f}s @1.95 {b[39]:+.2f} | corrLC {np.corrcoef(L,C)[0,1]:+.2f}')
if __name__=="__main__":
  x,fs=load('/workspace/vt6/real.wav');rep('real',x,fs)
  for f in sys.argv[1:]: rep(f.split('/')[-1],np.fromfile(f)[:15*48000],48000)
