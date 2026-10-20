"""Silent Night band arrangement: soft flute (melody), classical guitar
(arpeggios), cello (tenor line), piano (bar chords).
Score: F. X. Gruber, "Stille Nacht" (1818), Mutopia Project
GruberFX/stille_nacht (hymntune, SATB; typeset by Steve Dunlop from
cyberhymnal.org, Mutopia-2008/02/19-1295), Public Domain. Voices are the
ABC V:1-V:4 lines in that file (Bb major, 6/8, unit 1/16), transposed up a
whole tone to C as in tools/demo/christmas/silent_night_melody.txt;
dotted quarter = 52 (that file's tempo).
Parts: flute = V:1 (p, the last two phrases pp); cello = V:3 tenor at
pitch; guitar = per dotted quarter the V:4 bass (raised an octave below
E2), V:3 and V:2 notes sounding at that beat, as three eighths; piano =
per bar V:4 bass with V:2/V:3 chord, held the bar. Intro: two bars of the
tonic on guitar (C3 G3 E4 arpeggio). Voicings, the intro, the
dynamics and the velocities are arrangement choices (UNSOURCED).
Writes sn_flute.txt (wp1 WP_SONGFILE), sn_cello.csv, sn_guitar.csv,
sn_piano.csv (start,dur,midi,vel). Times count from the flute's start."""
import re,sys
V={
1:"F3G1F2D6F3G1F2D6c4c2A6B4B2F6G4G2B3A1G2F3G1F2D6G4G2B3A1G2F3G1F2D6c4c2e3c1A2B6d6B3F1D2F3E1C2B,12",
2:"D3E1D2B,6D3E1D2B,6E4E2C6D4D2D6E4E2G3F1E2D3E1D2B,6E4E2G3F1E2D3E1D2B,6E4E2A3E1C2D6F6D3F1B,2D3C1A,2B,12",
3:"B,4B,2F,6B,4B,2F,6A,4A,2F,6F,4F,2B,6B,4B,2G,3A,1B,2B,3B,1B,2F,6B,4B,2G,3A,1B,2B,3B,1B,2F,6A,4A,2C3A,1F,2F,6B,6B,4F,2F,3F,1E,2D,12",
4:"B,,4B,,2B,,6B,,4B,,2B,,6F,4F,2F,6B,,4B,,2B,,6E,4E,2E,3E,1E,2B,,3B,,1B,,2B,,6E,4E,2E,3E,1E,2B,,3B,,1B,,2B,,6F,4F,2F,3F,1F,2B,,8-B,,4F,4F,2F,,3F,,1F,,2B,,12"}
KEY={'B':-1,'E':-1}  # Bb major
STEP={'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}
def parse(s):
  out=[];tie=False
  for m in re.finditer(r"([A-Ga-g])([,']*)(\d+)(-?)",s):
    L,o,d,t=m.groups();base=60 if L.isupper() else 72
    p=base+STEP[L.upper()]+KEY.get(L.upper(),0)-12*o.count(',')+12*o.count("'")+2  # +2: to C
    if tie and out and out[-1][0]==p: out[-1][1]+=int(d)
    else: out.append([p,int(d)])
    tie=bool(t)
  return out
S16=60/52/6; INTRO=2*12*S16
parts={k:parse(v) for k,v in V.items()}
for k,v in parts.items(): assert sum(d for _,d in v)==144,(k,sum(d for _,d in v))
def at(part,t16):
  s=0
  for p,d in part:
    if s<=t16<s+d: return p
    s+=d
NAME=['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
nm=lambda m:f"{NAME[m%12]}{m//12-1}"
hz=lambda m:440*2**((m-69)/12)
# flute
fl=[f"# Silent Night band flute: V:1 of Mutopia stille_nacht.ly (PD), +2 semitones; see arrange_sn.py",f"R 0 {INTRO:.4f} 0 0 p"]
s=0
for i,(p,d) in enumerate(parts[1]):
  dyn='pp' if s>=120 else 'p'
  fl.append(f"{nm(p)} {hz(p):.2f} {d*S16:.4f} 1 {1 if d>=6 else 0} {dyn}");s+=d
open('sn_flute.txt','w').write('\n'.join(fl)+'\n')
rows=lambda L:'\n'.join(f"{a:.4f},{b:.4f},{m},{v}" for a,b,m,v in L)+'\n'
vc=[];s=0
for p,d in parts[3]: vc.append((INTRO+s*S16,d*S16,p,34 if s<120 else 28));s+=d
gt=[]
for b in range(4): 
  for k,m in enumerate([48,55,64]): gt.append((b*6*S16+k*2*S16,6*S16 if k==0 else 4*S16,m,52 if k==0 else 42))
for beat in range(24):
  t16=beat*6;t=INTRO+t16*S16
  if beat==23: gt.append((t,6*S16,48,44));gt.append((t,6*S16,55,36));gt.append((t,6*S16,64,36));continue
  b=at(parts[4],t16); b=b+12 if b<40 else b
  ch=[b,at(parts[3],t16),at(parts[2],t16)]
  for k,m in enumerate(ch): gt.append((t+k*2*S16,(6-2*k)*S16,m,(50 if k==0 else 40) if t16<120 else (42 if k==0 else 34)))
pn=[]
for bar in range(12):
  t16=bar*12;t=INTRO+t16*S16;d=(24 if bar==11 else 12)*S16
  if bar==12: break
  for m in {at(parts[4],t16),at(parts[3],t16)+12,at(parts[2],t16)}: pn.append((t,d,m,38 if bar<10 else 32))
  if bar==11: break
open('sn_cello.csv','w').write(rows(vc));open('sn_guitar.csv','w').write(rows(sorted(gt)));open('sn_piano.csv','w').write(rows(sorted(pn)))
print('intro %.3f s, verse %.3f s, end %.3f s'%(INTRO,144*S16,INTRO+144*S16))
