import numpy as np,soundfile as sf,sys,librosa
from scipy.signal import fftconvolve,lfilter
sys.argv=['x'];exec(open('/workspace/vt36/tr.py').read().split("x,fs=load")[0])
fs=48000
def parts(ir,g=2.43,directMs=6.4,eq=4.0):
    ir=ir.copy();pk=int(np.argmax(abs(ir)));end=min(len(ir),pk+int(directMs*1e-3*fs))
    e=np.sum(ir[:end]**2);s=np.sign(ir[pk]);ir[:end]=0
    A=10**(eq/40);w0=2*np.pi*450/fs;al=np.sin(w0)/2
    b=[1+al*A,-2*np.cos(w0),1-al*A];a=[1+al/A,-2*np.cos(w0),1-al/A]
    ir[end:]=lfilter(b,a,ir[end:]);d=np.zeros_like(ir);d[pk]=s*np.sqrt(e)*g;return d+ir
x=np.fromfile('/workspace/v42/v42d.raw')
real=tr(*load('/workspace/vt6/real.wav'))
print('real',np.round(real,2))
print('dry ',np.round(tr(x[:720000],fs),2))
for p in ['crash','hh','kick','ride','snare','toma','tomb','tomc']:
    w,sr=sf.read(p+'.wav');w=librosa.resample(w[:,0],orig_sr=sr,target_sr=fs)
    y=fftconvolve(x,parts(w))[:len(x)];print(p,np.round(tr(y[:720000],fs),2))
# eq2oct-equivalent: smoothed (1/3-oct) magnitude ratio eq2oct/raw from the hh pair, applied zero-phase
r=np.fromfile('hh_raw48.f64');q=np.fromfile('/workspace/gp/ir.f64');N=1<<17
R=np.abs(np.fft.rfft(r,N));Q=np.abs(np.fft.rfft(q,N));f=np.fft.rfftfreq(N,1/fs)
G=np.ones_like(f)
for i,fi in enumerate(f):
    if fi<20: continue
    k=(f>fi/2**(1/6))&(f<fi*2**(1/6)); G[i]=np.sqrt(np.sum(Q[k]**2)/np.sum(R[k]**2))
G[f<20]=G[np.argmax(f>=20)]
print('hh raw->eq check',np.round(tr(fftconvolve(x,parts(np.fft.irfft(np.fft.rfft(r,N)*G,N)[:len(r)]))[:720000],fs),2),'vs eq2oct file',np.round(tr(fftconvolve(x,parts(q))[:720000],fs),2))
for p in ['crash','tomb','ride']:
    w,sr=sf.read(p+'.wav');w=librosa.resample(w[:,0],orig_sr=sr,target_sr=fs);we=np.fft.irfft(np.fft.rfft(w,N)*G,N)[:len(w)]
    we.astype('<f8').tofile(f'studio_g6_{p}_W_48k_eq2oct.f64');print(p,'eq',np.round(tr(fftconvolve(x,parts(we))[:len(x)][:720000],fs),2))
