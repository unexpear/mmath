import numpy as n,sys
exec(open('pk.py').read().split('if __name__')[0])
y=n.fromfile(sys.argv[1]);seg=len(y)//4
for k,nm in enumerate(['C4','G4','C5','F5']):
  y[k*seg:(k+1)*seg].tofile('/tmp/c.f64');o,c,d=an('/tmp/c.f64');print(nm,'MOD cen %.0f'%c,' '.join('%.0f(%.0fdB,%.1fs)'%x for x in o))
