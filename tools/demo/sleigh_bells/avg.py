# seed-averaged pattern + accent metrics: avg.py ref.wav name1 name2 ... (files name_<seed>.f64)
import sys,glob,numpy as n,soundfile as sf,io,contextlib,warnings
warnings.filterwarnings('ignore')
import acc,cmp_lib
ref=sys.argv[1]
print('%-8s %5s %5s %5s %5s | %6s %6s %6s | %6s %6s %6s | %4s %4s %5s'%('','rise','fall','pk','','at.3-2','at3-5','at12-15','rg.3-2','rg3-5','rg12-15','str','mat','corr'))
def row(name,v):print('%-8s '%name+' '.join('%5.1f'%a for a in v[:3])+'       '+' '.join('%6.1f'%a for a in v[3:9])+'   %4.0f %4.1f %5.2f'%tuple(v[9:12]))
R=acc.stats(acc.load(ref));p=cmp_lib.pattern(ref,ref);row('real',list(R)+p)
for nm in sys.argv[2:]:
    V=[]
    for f in sorted(glob.glob(nm+'_*.f64')):
        x=n.fromfile(f);w=f[:-4]+'.wav';sf.write(w,x/n.abs(x).max()*0.9,44100,subtype='FLOAT')
        V.append(list(acc.stats(acc.load(w)))+cmp_lib.pattern(ref,w))
    row(nm,n.mean(V,0))
