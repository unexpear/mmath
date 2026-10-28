"""Silent Night string trio (step 14): violin = V:1 (melody), viola = V:2
(alto), double bass arco = V:4 (bass), at sounding pitch, from the same
Mutopia stille_nacht source and transposition (+2 to C, dotted quarter =
52) as arrange_sn.py. Starts at 0.5 s, no intro. Velocities (level =
vel / 80) are arrangement choices (UNSOURCED): violin 40, viola 34, bass
40; the last two phrases (from the 16th at 120) 0.8 x.
Writes sn_trio_violin.csv, sn_trio_viola.csv, sn_trio_bass.csv
(start,dur,midi,vel)."""
import sys,os,io,contextlib
src=open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'arrange_sn.py')).read()
ns={};exec(src.split('# flute')[0],ns)  # V, parse, S16 from arrange_sn.py (writes nothing)
parts,S16=ns['parts'],ns['S16'];T0=0.5
def rows(part,vel):
  out=[];s=0
  for p,d in part: out.append(f"{T0+s*S16:.4f},{d*S16:.4f},{p},{vel if s<120 else round(vel*0.8)}");s+=d
  return '\n'.join(out)+'\n'
for name,k,v in [('violin',1,40),('viola',2,34),('bass',4,40)]: open(f'sn_trio_{name}.csv','w').write(rows(parts[k],v))
print('end %.3f s'%(T0+144*S16))
