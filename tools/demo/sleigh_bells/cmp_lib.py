import numpy as n,soundfile as sf
from sbpattern import transcribe
_c={}
def env(p):
    x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x
    w=int(.05*fs);e=n.sqrt(n.convolve(x**2,n.ones(w)/w,'same'))[::w//2];return 20*n.log10(e/e.max()+1e-9)
def pattern(ref,p):
    if ref not in _c:_c[ref]=(transcribe(ref)[0],env(ref))
    rs,re_=_c[ref];ms=transcribe(p)[0];me=env(p)[:len(re_)]
    ok=[1 for a,l in rs if min(abs(m[0]-a) for m in ms)<0.04]
    return [len(ms),len(ok),n.corrcoef(n.maximum(re_,-50),n.maximum(me,-50))[0,1]]
