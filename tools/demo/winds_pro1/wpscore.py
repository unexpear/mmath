"""Winds pro-check scoreboard: model phrase vs CC/PD reference, same transcribed notes.
Usage: wpscore.py out.png tag inst[,inst...]  (model files wp1_<inst>.f64 or tag_<inst>.f64)"""
import sys,re,numpy as n,soundfile as sf,librosa
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
LEAD=0.56
def notes_of(inst):
  t=open('/tmp/exc.txt').read();blk=t.split(f'kPro{inst.capitalize()}[]')[1].split('};')[0]
  out=[];s=0.0
  for nm,hz,d,tg in re.findall(r'\{"([^"]+)", ([\d.]+), ([\d.]+), (true|false)',blk):
    hz=float(hz);d=float(d)
    if hz>0: out.append((s,s+d,hz,tg=='true'))
    s+=d
  return out
def load(p,fs0=48000):
  if p.endswith('.f64'): x=n.fromfile(p);fs=fs0
  else: x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x
  return librosa.resample(x,orig_sr=fs,target_sr=44100),44100
def metrics(x,fs,notes,off):
  d={};x=x/(n.sqrt(n.mean(x**2))+1e-20)
  f0,v,_=librosa.pyin(x.astype(n.float32),fmin=70,fmax=2000,sr=fs,frame_length=2048,hop_length=256);tt=n.arange(len(f0))*256/fs
  hop=int(0.005*fs);env=n.sqrt(n.convolve(x**2,n.ones(hop)/hop,'same'))[::hop];te=n.arange(len(env))*0.005;L=20*n.log10(env+1e-9)
  errs=[];h=[];noise=[];att=[];lev=[]
  for (a,b,hz,tg) in notes:
    a+=off;b+=off;m=(tt>a+0.25*(b-a))&(tt<b-0.15*(b-a))&v
    if m.sum()>2: errs.append(1200*n.log2(n.nanmedian(f0[m])/hz))
    i0,i1=int((a+0.3*(b-a))*fs),int((b-0.2*(b-a))*fs)
    if i1-i0>2048:
      seg=x[i0:i1]*n.hanning(i1-i0);S=n.abs(n.fft.rfft(seg,8*len(seg)));f=n.fft.rfftfreq(8*len(seg),1/fs)
      H=[S[(f>k*hz*0.97)&(f<k*hz*1.03)].max() for k in range(1,9)];h.append(20*n.log10(n.array(H)/max(H[0],1e-12)))
      mk=n.ones_like(f,bool)
      for k in range(1,40): mk&=n.abs(f-k*hz)>0.12*hz
      mk&=(f>200)&(f<6000);noise.append(20*n.log10(n.median(S[mk])/max(H)))
      lev.append(10*n.log10(n.mean(x[i0:i1]**2)))
    if tg:
      w=(te>=a-0.06)&(te<=a+0.25);e=env[w]
      if e.size>4:
        pk=e.max();lo=n.argmax(e>0.1*pk);hi=n.argmax(e>0.9*pk);att.append((hi-lo)*5.0)
  d['Pitch |error| median (cents)']=n.median(n.abs(errs)) if errs else n.nan
  d['Pitch worst note (cents)']=max(errs,key=abs) if errs else n.nan
  P=n.abs(librosa.stft(x.astype(n.float32),n_fft=4096))**2;f=librosa.fft_frequencies(sr=fs,n_fft=4096);S=P.mean(1)
  d['Spectral centroid (Hz)']=(f*S).sum()/S.sum()
  for lo,hi in [(0,1000),(1000,3000),(3000,8000),(8000,16000)]:
    k=(f>=lo)&(f<hi);d[f'Band {lo/1000:g}-{hi/1000:g} kHz (dB)']=10*n.log10(S[k].sum()/S.sum())
  hm=n.mean(h,0) if h else n.full(8,n.nan)
  for k in [2,3,4,6]: d[f'H{k} re H1 (dB)']=hm[k-1]
  d['Inter-harmonic floor (dB)']=n.median(noise) if noise else n.nan
  d['Attack 10-90% median (ms)']=n.median(att) if att else n.nan
  a,b=notes[-1][0]+off,notes[-1][1]+off;w=te>a;e=L[w];tw=te[w];pk=n.argmax(e)
  after=n.where(e[pk:]<e[pk]-30)[0];d['Ending: last note peak to -30 dB (ms)']=(tw[pk+after[0]]-b)*1000 if after.size else n.nan
  d['Note level std (dB)']=n.std(lev) if lev else n.nan
  return d
out,tag,insts=sys.argv[1],sys.argv[2],sys.argv[3].split(',')
rows={};cols=[]
for i in insts:
  N=notes_of(i)
  R=metrics(*load(f'ref_{i}.wav'),N,0.0)
  for k in R: rows.setdefault(k,{});rows[k][i+' real']=R[k]
  cols.append(i+' real')
  for tg in tag.split('+'):
    M=metrics(*load(f'{tg}_{i}.f64'),N,LEAD)
    for k in M: rows[k][i+' '+tg]=M[k]
    cols.append(i+' '+tg)
for k in rows: print(k,'|',' | '.join(f'{rows[k][c]:.4g}' for c in cols))
fig,ax=plt.subplots(figsize=(2.0*len(cols)+4,0.42*len(rows)+1.2));ax.axis('off')
cell=[[f'{rows[k][c]:.4g}' for c in cols] for k in rows]
tb=ax.table(cellText=cell,rowLabels=list(rows),colLabels=cols,loc='center',cellLoc='center');tb.auto_set_font_size(False);tb.set_fontsize(8);tb.scale(1,1.3)
ax.set_title(f'Winds {tag.replace("+"," / ")} vs CC/PD references (Wikimedia Commons: Serolillo CC BY 2.5; ChickSR CC BY-SA 4.0; M. Prevot CC BY-SA 4.0)',fontsize=9)
plt.savefig(out,dpi=130,bbox_inches='tight')
