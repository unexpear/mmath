"""Per-note fine pitch (YIN) on a WP_SONGFILE trumpet render. Usage: jbtrack.py song.txt tag[+tag] (files <tag>_trumpet.f64, 0.56 s lead-in).
Prints per pitch class: steady cents median / attack cents (15-60 ms) median / settle ms median, and counts of notes with |attack|>50 c."""
import sys,numpy as n,librosa,collections
FS=48000;LEAD=0.56
notes=[];s=LEAD
for l in open(sys.argv[1]):
  if l.startswith('#') or not l.strip(): continue
  p=l.split();hz=float(p[1]);d=float(p[2])
  if hz>0: notes.append((p[0],s,s+d,hz))
  s+=d
def an(x):
  x=x/(n.sqrt(n.mean(x**2))+1e-20);hop=128
  f0=librosa.yin(x,fmin=150,fmax=1200,sr=FS,frame_length=2048,hop_length=hop);tt=n.arange(len(f0))*hop/FS
  env=n.sqrt(n.convolve(x**2,n.ones(480)/480,'same'));Lv=20*n.log10(env[n.minimum((tt*FS).astype(int),len(x)-1)]+1e-12)
  out=collections.defaultdict(list)
  for nm,a,b,hz in notes:
    c=1200*n.log2(f0/hz);st=(tt>a+0.3*(b-a))&(tt<b-0.15*(b-a));at=(tt>a+0.015)&(tt<a+0.06);at&=Lv>n.median(Lv[st])-30
    ok=n.abs(c)<25;w=int(0.03*FS/hop);se=n.nan
    for i in n.where((tt>=a-0.02)&(tt<b))[0]:
      if ok[i:i+w].all(): se=(tt[i]-a)*1000;break
    out[nm].append((n.median(c[st]),n.median(c[at]) if at.sum() else n.nan,se,a))
  return out
tags=sys.argv[2].split('+');R={t:an(n.fromfile(f'{t}_trumpet.f64')) for t in tags}
print('note n | '+' || '.join(f'{t}: steady/attack/settle/bad' for t in tags))
for nm in sorted(R[tags[0]],key=lambda k:[x[3] for x in notes if x[0]==k][0]):
  line=f'{nm:4s} {len(R[tags[0]][nm]):2d}'
  for t in tags:
    a=n.array(R[t][nm],dtype=float);line+=f' || {n.median(a[:,0]):+6.1f} {n.nanmedian(a[:,1]):+7.1f} {n.nanmedian(a[:,2]):5.0f} {int((abs(a[:,1])>50).sum()):2d}'
  print(line)
for t in tags:
  a=n.array([v for k in R[t] for v in R[t][k]],dtype=float)
  print(f'{t}: steady mean|c| {n.mean(abs(a[:,0])):.1f} worst {a[n.argmax(abs(a[:,0])),0]:+.0f} at {a[n.argmax(abs(a[:,0])),3]:.2f}s; attack mean|c| {n.nanmean(abs(a[:,1])):.0f} (gated -30 dB; {int(n.isnan(a[:,1]).sum())} silent); settle median {n.nanmedian(a[:,2]):.0f} ms; |attack|>50c {int((abs(a[:,1])>50).sum())}/{len(a)}')
