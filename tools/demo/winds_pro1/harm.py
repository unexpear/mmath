import sys,re,numpy as n,soundfile as sf,librosa
FS=48000
t=open('/workspace/r/tests/wind_excerpts.h').read();blk=t.split('kProTrumpet[]')[1].split('};')[0]
notes=[];s=0.0
for nm,hz,d,tg in re.findall(r'\{"([^"]+)", ([\d.]+), ([\d.]+), (true|false)',blk):
  hz=float(hz);d=float(d)
  if hz>0: notes.append((nm,s,s+d,hz))
  s+=d
def H(x,off):
  r=[]
  for nm,a,b,hz in notes:
    seg=x[int((a+off+0.3*(b-a))*FS):int((b+off-0.15*(b-a))*FS)];S=n.abs(n.fft.rfft(seg*n.hanning(len(seg)),1<<16));fr=n.fft.rfftfreq(1<<16,1/FS)
    h=[S[(fr>k*hz*0.97)&(fr<k*hz*1.03)].max() for k in (1,2,3,4)]
    r.append(f"{nm}:"+"/".join(f"{20*n.log10(v/h[0]):+.0f}" for v in h[1:]))
  return ' '.join(r)
x,fs=sf.read('ref_trumpet.wav');x=x.mean(1) if x.ndim>1 else x;x=librosa.resample(x,orig_sr=fs,target_sr=FS);print('REF',H(x,0))
for t in sys.argv[1].split('+'): print(t,H(n.fromfile(t+'_trumpet.f64'),0.56))
