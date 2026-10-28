import numpy as n,soundfile as sf,librosa,sys
fs=48000
def chk(wav,notes,fmin,fmax):
  x,_=sf.read(wav);errs=[];miss=0
  for s,d,m in notes:
    if d<0.25: continue
    seg=x[int((s+0.12)*fs):int((s+d-0.05)*fs)]
    f0,vf,_=librosa.pyin(seg,fmin=fmin,fmax=fmax,sr=fs,frame_length=4096,hop_length=480)
    if vf.sum()<3: miss+=1;continue
    errs.append(1200*n.log2(n.nanmedian(f0[vf])/(440*2**((m-69)/12))))
  e=n.array(errs);print(wav,'notes',len(e),'miss',miss,'median |err| %.1f c'%n.median(abs(e)),'octave errs',(abs(e)>600).sum())
vc=[tuple(map(float,l.split(',')[:3])) for l in open('../tools/demo/band/pat_cello.csv')]
chk('pstem_vc.wav',vc,80,500)
fl=[];t=0
for l in open('../tools/demo/band/pat_flute.txt'):
  if l[0]=='#':continue
  p=l.split();hz=float(p[1]);d=float(p[2])
  if hz>0: fl.append((t,d,69+12*n.log2(hz/440)))
  t+=d
chk('pstem_fl.wav',fl,200,1200)
