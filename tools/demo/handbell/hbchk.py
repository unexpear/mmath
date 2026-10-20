import numpy as n,sys
exec(open('pk.py').read().split('if __name__')[0])
y=n.fromfile(sys.argv[1]);seg=len(y)//4
R=['hb_F3_rung_557142','hb_c1_370002','hb_g1_370007','hb_c2_370001']
for k,f1 in enumerate([174.61,1046.5,1568,2093]):
  y[k*seg:(k+1)*seg].tofile('/tmp/c.f64');o,c,d=an('/tmp/c.f64',top=7);print('%5.0f MOD cen %5.0f '%(f1,c)+' '.join('%.2f(%.0f,%.1fs)'%(a/f1,b,t) for a,b,t in o))
  if len(sys.argv)<3: o,c,d=an(f'/workspace/band/ref/hb/{R[k]}.mp3',top=7);print('%5.0f REF cen %5.0f '%(f1,c)+' '.join('%.2f(%.0f,%.1fs)'%(a/f1,b,t) for a,b,t in o))
