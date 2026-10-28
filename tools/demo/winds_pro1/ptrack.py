"""Fine per-note pitch + clarity for the winds pro trumpet excerpt.
YIN (librosa.yin, parabolic-interpolated, continuous Hz), frame 2048 hop 128 at 48 kHz.
Usage: ptrack.py tag[+tag...]   (files <tag>_trumpet.f64, 0.56 s lead-in; ref ref_trumpet.wav)"""
import sys,re,numpy as n,soundfile as sf,librosa
FS=48000;LEAD=0.56
t=open('/workspace/r/tests/wind_excerpts.h').read();blk=t.split('kProTrumpet[]')[1].split('};')[0]
notes=[];s=0.0
for nm,hz,d,tg in re.findall(r'\{"([^"]+)", ([\d.]+), ([\d.]+), (true|false)',blk):
  hz=float(hz);d=float(d)
  if hz>0: notes.append((nm,s,s+d,hz,tg=='true'))
  s+=d
def load(p):
  if p.endswith('.f64'): return n.fromfile(p)
  x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x;return librosa.resample(x,orig_sr=fs,target_sr=FS)
def analyse(x,off):
  x=x/(n.sqrt(n.mean(x**2))+1e-20);hop=128
  f0=librosa.yin(x.astype(n.float64),fmin=150,fmax=1200,sr=FS,frame_length=2048,hop_length=hop,trough_threshold=0.1);tt=n.arange(len(f0))*hop/FS+1024/FS*0
  env=n.sqrt(n.convolve(x**2,n.ones(240)/240,'same'));L=20*n.log10(env+1e-9)
  rows=[]
  for k,(nm,a,b,hz,tg) in enumerate(notes):
    A,B=a+off,b+off
    st=(tt>A+0.3*(b-a))&(tt<B-0.15*(b-a));at=(tt>A+0.015)&(tt<A+0.06)
    c=lambda m:1200*n.log2(n.median(f0[m])/hz) if m.sum()>1 else n.nan
    # settle: first time after A where f0 stays within 25 c of target for 30 ms
    cents=1200*n.log2(f0/hz);ok=n.abs(cents)<25;settle=n.nan;w=int(0.03*FS/hop)
    idx=n.where((tt>=A-0.02)&(tt<B))[0]
    for i in idx:
      if ok[i:i+w].all(): settle=(tt[i]-A)*1000;break
    # boundary dip: min level within +-30 ms of A re mean steady level
    ia,ib=int((A-0.03)*FS),int((A+0.03)*FS);s0,s1=int((A+0.3*(b-a))*FS),int((B-0.15*(b-a))*FS)
    dip=L[ia:ib].min()-n.median(L[s0:s1]) if k>0 else n.nan
    # steady jitter: std of cents in steady part
    jit=n.std(cents[st]) if st.sum()>2 else n.nan
    rows.append((nm,c(st),c(at),settle,dip,jit))
  return rows
tags=sys.argv[1].split('+')
R=analyse(load('ref_trumpet.wav'),0.0)
print('note | REF steady c / attack c / settle ms / boundary dip dB / jitter c || '+' || '.join(tags))
res={tg:analyse(load(f'{tg}_trumpet.f64'),LEAD) for tg in tags}
for k,r in enumerate(R):
  line=f"{r[0]:4s} | {r[1]:+6.1f} {r[2]:+6.1f} {r[3]:5.0f} {r[4]:+5.1f} {r[5]:4.1f}"
  for tg in tags:
    m=res[tg][k];line+=f" || {m[1]:+6.1f} {m[2]:+6.1f} {m[3]:5.0f} {m[4]:+5.1f} {m[5]:4.1f}"
  print(line)
for tg in ['REF']+tags:
  rr=R if tg=='REF' else res[tg];st=n.array([r[1] for r in rr])
  print(f"{tg}: steady mean {n.nanmean(st):+.1f} c, mean |c| {n.nanmean(abs(st)):.1f}, worst {st[n.nanargmax(abs(st))]:+.1f}")
