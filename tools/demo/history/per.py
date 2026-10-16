import numpy as np,soundfile as sf,sys
def feats(x,fs,hop=0.05,win=0.4):
    n=int(win*fs);h=int(hop*fs);L=[];C=[]
    f=np.fft.rfftfreq(n,1/fs);wd=np.hanning(n)
    for i in range(0,len(x)-n,h):
        s=x[i:i+n];e=np.mean(s**2);L.append(10*np.log10(e+1e-20))
        P=np.abs(np.fft.rfft(s*wd))**2;C.append((f*P).sum()/(P.sum()+1e-20))
    return np.array(L),np.array(C)
def ac(v):
    v=v-v.mean();a=np.correlate(v,v,'full')[len(v)-1:];return a/a[0]
def peak(v,hop=0.05):
    a=ac(v);lo=int(0.3/hop);hi=int(4/hop);k=lo+np.argmax(a[lo:hi]);return k*hop,a[k]
def load(p):
    x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x;return x[:15*fs],fs
if __name__=='__main__':
    for p in sys.argv[1:]:
        x,fs=load(p);L,C=feats(x,fs);pl=peak(L);pc=peak(C)
        print(f'{p:40s} loud ac peak {pl[0]:.2f}s r={pl[1]:.2f} | centroid ac peak {pc[0]:.2f}s r={pc[1]:.2f} | corr(L,C)={np.corrcoef(L,C)[0,1]:+.2f}')
