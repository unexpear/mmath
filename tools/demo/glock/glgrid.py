import subprocess,re,os,itertools,math
def run(gk,gx):
  env=dict(os.environ,GK=str(gk),GX=str(gx));subprocess.run(['./glock1','gg.f64','1.5','4'],env=env,capture_output=True)
  out=subprocess.run(['python3','glk.py','gg.f64'],capture_output=True,text=True).stdout.splitlines()
  ref=[l for l in out if 'REF' in l];mod=[l for l in out if 'MOD' in l];err=0;rows=[]
  for r,m in zip(ref,mod):
    def parse(l):
      c=float(re.search(r'cen\s+(\d+)',l).group(1));pk=[(float(a),float(b)) for a,b in re.findall(r'([\d.]+)\((-?\d+)\)',l.split('peaks')[1])]
      return c,pk
    cr,pr=parse(r);cm,pm=parse(m)
    def lev(pk,lo,hi):
      v=[b for a,b in pk if lo<a<hi];return max(v) if v else -45
    e=sum(abs(lev(pr,lo,hi)-lev(pm,lo,hi)) for lo,hi in [(0.9,1.1),(2.5,3.2),(5.0,6.0)])+20*abs(math.log10(cm/cr))
    err+=e;rows.append(round(e))
  return err,rows
for gk,gx in itertools.product([1,1.5,2,3],[0,0.0102,0.014,0.02]):
  e,r=run(gk,gx);print(gk,gx,round(e),r)
