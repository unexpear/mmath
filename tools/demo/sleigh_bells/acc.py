import sys,numpy as n,soundfile as sf
from sbpattern import transcribe
fs=44100
rs,_,_,_,_,_=transcribe(sys.argv[1]); acc=[a for a,l in rs if l>=-8 and a>0.07]
def load(p):
    x,_=sf.read(p);x=x.mean(1) if x.ndim>1 else x;return x/n.abs(x).max()
def band(s,lo,hi):
    f=n.fft.rfftfreq(len(s),1/fs);P=n.abs(n.fft.rfft(s*n.hanning(len(s))))**2
    return P[(f>=lo)&(f<hi)].mean()
def stats(x):
    R=[]
    for a in acc:
        i0=int((a-0.06)*fs);s=x[i0:i0+int(0.3*fs)];w=88
        e=n.sqrt(n.convolve(s**2,n.ones(w)/w,'same'));pk=e.argmax();E=e/e[pk]
        pre=E[:pk];i10=n.where(pre<0.1)[0];i90=n.where(pre<0.9)[0]
        rise=(i90[-1]-(i10[-1] if len(i10) else 0))/fs*1000 if len(i90) else 0
        post=n.where(E[pk:]<10**(-10/20))[0];fall=post[0]/fs*1000 if len(post) else 240
        at=x[int(a*fs)-int(.002*fs):int(a*fs)+int(.02*fs)];rg=x[int((a+.12)*fs):int((a+.30)*fs)]
        ref=band(at,6400,6900)
        R.append([rise,fall,(pk/fs-0.06)*1000]+[10*n.log10(band(at,lo,hi)/ref) for lo,hi in [(300,2000),(3000,5000),(12000,15500)]]+[10*n.log10(band(rg,lo,hi)/band(rg,6400,6900)) for lo,hi in [(300,2000),(3000,5000),(12000,15500)]])
    return n.median(n.array(R),0)
print('%-12s %6s %6s %6s | attack band dB re 6.4-6.9k: %6s %6s %6s | ring: %6s %6s %6s'%('','rise','fall10','pk ms','.3-2k','3-5k','12-15k','.3-2k','3-5k','12-15k'))
for p in sys.argv[1:]:
    print('%-12s'%p.split('/')[-1][:12]+' '.join('%6.1f'%v for v in stats(load(p))))
