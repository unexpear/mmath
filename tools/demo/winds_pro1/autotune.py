"""Retune the trumpet rows for a config by YIN steady cents (two passes), then report.
Usage: autotune.py tag 'ENV=VAL ENV2=VAL' [passes]"""
import sys,os,re,subprocess,collections,numpy as n,librosa
FS=48000;LEAD=0.56;tag=sys.argv[1];envs=dict(kv.split('=') for kv in sys.argv[2].split()) if len(sys.argv)>2 and sys.argv[2] else {}
passes=int(sys.argv[3]) if len(sys.argv)>3 else 2
H='/workspace/r/include/mmathen/instruments/wind_presets.h';S='/workspace/r/tools/demo/christmas/jingle_bells_melody.txt'
s=open(H).read();blk=s[s.index('kTrumpetFingers[] = {'):s.index('};',s.index('kTrumpetFingers[] = {'))]
rows={float(h):[float(l),int(m)] for h,l,m in re.findall(r'\{(\d+\.\d+), (\d+\.\d+), (\d)',blk)}
if os.environ.get('ROWSTART'):
  for l in open(os.environ['ROWSTART']):
    h,L=l.split();rows[float(h)][0]=float(L)
def song(path):
  out=[];t=LEAD
  for l in open(path):
    if l.startswith('#') or not l.strip(): continue
    p=l.split();hz=float(p[1]);d=float(p[2])
    if hz>0: out.append((p[0],t,t+d,hz))
    t+=d
  return out
ex=open('/workspace/r/tests/wind_excerpts.h').read().split('kProTrumpet[]')[1].split('};')[0];exn=[];t=LEAD
for nm,hz,d,tg in re.findall(r'\{"([^"]+)", ([\d.]+), ([\d.]+), (true|false)',ex):
  hz=float(hz);d=float(d)
  if hz>0: exn.append((nm,t,t+d,hz))
  t+=d
sets={'ex':(None,exn),'jb':(S,song(S)),'f5':('f5.txt',song('f5.txt'))}
def render(p):
  e=dict(os.environ,**envs,WP_ROWFILE=f'{tag}_rows.txt');procs=[]
  for k,(sf_,_) in sets.items():
    ee=dict(e);ee.pop('WP_SONGFILE',None)
    if sf_: ee['WP_SONGFILE']=sf_
    procs.append(subprocess.Popen([os.environ.get('WPBIN','../wp1'),'trumpet',f'{tag}{k}_trumpet.f64'],env=ee,stdout=subprocess.DEVNULL))
  for q in procs: q.wait()
def steady():
  c=collections.defaultdict(list)
  for k,(_,notes) in sets.items():
    x=n.fromfile(f'{tag}{k}_trumpet.f64');x=x/(n.sqrt(n.mean(x**2))+1e-20)
    f0=librosa.yin(x,fmin=150,fmax=1200,sr=FS,frame_length=2048,hop_length=128);tt=n.arange(len(f0))*128/FS
    for nm,a,b,hz in notes:
      st=(tt>a+0.3*(b-a))&(tt<b-0.15*(b-a));c[hz].append(1200*n.log2(n.median(f0[st])/hz))
  return {h:float(n.median(v)) for h,v in c.items()}
def write():
  open(f'{tag}_rows.txt','w').write('\n'.join(f'{h} {l:.5f}' for h,(l,m) in rows.items()))
for it in range(passes+1):
  write();render(it);c=steady()
  print(tag,'pass',it,' '.join(f'{h:.0f}:{v:+.1f}' for h,v in sorted(c.items())),flush=True)
  if it==passes: break
  for h,v in c.items():
    if h in rows and abs(v)<60:
      L,m=rows[h];ap=0 if m==0 else 0.26;rows[h][0]=(L+ap)*2**(v/1200)-ap
for k in sets: os.replace(f'{tag}{k}_trumpet.f64',f'{tag}{k}_trumpet.f64')
