import numpy as n,soundfile as sf
fs=48000;T=int(45.8*fs)
def ld(p):
  x=n.fromfile(p);y=n.zeros(T);y[:min(T,len(x))]=x[:T];return y
st={k:ld(f'pat_{k}.f64') for k in ['fl','vc','gt','sd']}
tgt={'fl':0,'vc':-4,'gt':-7,'sd':-15}
def rms(x):
  e=x[int(1.8*fs):int(43.5*fs)];return n.sqrt((e**2).mean())
g={k:10**(tgt[k]/20)/rms(st[k]) for k in st}
mix=sum(g[k]*st[k] for k in st);pk=abs(mix).max();norm=0.7/pk
for k in st: g[k]*=norm
mix*=norm
fade=n.ones(T);fade[-int(1.2*fs):]=n.linspace(1,0,int(1.2*fs));mix*=fade
sf.write('patapan_band1.wav',mix.astype(n.float32),fs,subtype='PCM_16')
for k in st: sf.write(f'pstem_{k}.wav',(g[k]*st[k]).astype(n.float32),fs,subtype='PCM_16')
open('../tools/demo/band/pat_mix.txt','w').write('band = '+' + '.join(f'{g[k]:.6g}*{k}' for k in st)+'; rms targets flute 0, cello -4, guitar -7, snare -15 dB over 1.8-43.5 s, peak 0.7, 1.2 s fade\n')
print({k:'%.1f'%(20*n.log10(pk)) for k,pk in [(k,abs(st[k]).max()/rms(st[k])) for k in st]})
