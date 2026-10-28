import sys,numpy as n,soundfile as sf
from sbpattern import transcribe
def env(p):
    x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x
    w=int(.05*fs);e=n.sqrt(n.convolve(x**2,n.ones(w)/w,'same'))[::w//2];return 20*n.log10(e/e.max()+1e-9)
rs,rb,_,_,_,_=transcribe(sys.argv[1]); re_=env(sys.argv[1])
for p in sys.argv[2:]:
    ms,mb,_,_,_,_=transcribe(p); me=env(p)[:len(re_)]
    hit=[(a,l,min(ms,key=lambda m:abs(m[0]-a))) for a,l in rs]
    ok=[h for h in hit if abs(h[2][0]-h[0])<0.04]
    lerr=n.mean([abs(h[2][1]-h[1]) for h in ok]) if ok else n.nan
    c=n.corrcoef(n.maximum(re_,-50),n.maximum(me,-50))[0,1]
    print(p,'strikes model %d real %d matched %d/%d  |level err| %.1f dB  env corr %.2f'%(len(ms),len(rs),len(ok),len(rs),lerr,c))
    print('  bg real',' '.join('%.0f'%v for _,v in rb)); print('  bg mod ',' '.join('%.0f'%v for _,v in mb))
