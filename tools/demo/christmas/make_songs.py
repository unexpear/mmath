#!/usr/bin/env python3
"""Writes the public-domain Christmas scores used by the winds harness
(tools/demo/winds_pro1/wp1, WP_SONGFILE). Each note line:
  name hertz seconds tongued vibrato dynamic   (hertz 0 = rest)

Silent Night: F. X. Gruber, "Stille Nacht" (1818). Source: Mutopia Project
  GruberFX/stille_nacht (hymntune, Voice SATB), typeset by Steve Dunlop from
  www.cyberhymnal.org, Mutopia-2008/02/19-1295, Public Domain
  (https://www.mutopiaproject.org/ftp/GruberFX/stille_nacht/stille_nacht.ly).
  Soprano (melody) and alto lines copied from the ABC V:1 / V:2 in that file
  (Bb major, 6/8, unit 1/16). Transposed up a whole tone to C so the
  melody's Bb3 sits on the flute's C4 floor.
Jingle Bells: J. Pierpont.
  Verse: "The One Horse Open Sleigh" (1857), Project Gutenberg eBook #10535,
  prepared by Dave Maddock from the facsimile sheet music, Public Domain
  (https://www.gutenberg.org/files/10535/10535-ly/10535.ly), versemelody,
  Ab major 2/4, transposed down a semitone to G.
  Refrain: the familiar "Jingle bells, jingle bells, jingle all the way"
  tune, which replaced Pierpont's 1857 chorus in later use. Source:
  Wikimedia Commons File:Jingle_Bells_refrain_vector.svg, LilyPond source
  (soprano voice), G major, public domain
  (https://commons.wikimedia.org/wiki/File:Jingle_Bells_refrain_vector.svg).
Tempos are performance choices, UNSOURCED except the refrain's \\tempo 4=100.
"""
import math, sys
NAMES=['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
def hz(m): return 440.0*2**((m-69)/12)
def nm(m): return NAMES[m%12]+str(m//12-1)
def midi(n):  # 'Bb4', 'C#5', 'F4'
    base={'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}[n[0]]; i=1
    while n[i] in 'b#': base+= -1 if n[i]=='b' else 1; i+=1
    return base+12*(int(n[i:])+1)
def abc(s, key_flats):  # ABC body, unit 1/16; uppercase octave 4, lowercase 5, ',' down
    out=[];i=0
    while i<len(s):
        c=s[i]; i+=1
        if c.upper() not in 'ABCDEFG': continue
        o=4 if c.isupper() else 5
        while i<len(s) and s[i]==',': o-=1; i+=1
        d=''
        while i<len(s) and s[i].isdigit(): d+=s[i]; i+=1
        p=c.upper()+('b' if c.upper() in key_flats else '')+str(o)
        out.append((midi(p),int(d or 1)))
    return out
def write(path, notes, header, dyn):
    with open(path,'w') as f:
        f.write(''.join('# '+l+'\n' for l in header.strip().splitlines()))
        for m,sec,t,v in notes:
            if m is None: f.write('rest 0 %.4f 0 0 %s\n'%(sec,dyn))
            else: f.write('%s %.2f %.4f %d %d %s\n'%(nm(m),hz(m),sec,t,v,dyn))
# Silent Night
SOP='F3G1F2D6F3G1F2D6c4c2A6B4B2F6G4G2B3A1G2F3G1F2D6G4G2B3A1G2F3G1F2D6c4c2e3c1A2B6d6B3F1D2F3E1C2B,12'
ALT='D3E1D2B,6D3E1D2B,6E4E2C6D4D2D6E4E2G3F1E2D3E1D2B,6E4E2G3F1E2D3E1D2B,6E4E2A3E1C2D6F6D3F1B,2D3C1A,2B,12'
unit=(60.0/52.0)/6.0   # dotted quarter = 52
def sn(body):
    ev=abc(body,'BE'); res=[]
    for k,(m,u) in enumerate(ev):
        res.append((m+2,u*unit,1,1 if u>=6 else 0))
    return res
hdr=__doc__.split('Jingle Bells:')[0]
write('silent_night_melody.txt', sn(SOP), hdr+'Part: soprano (melody), flute. dotted quarter = 52.', 'mp')
write('silent_night_alto.txt', sn(ALT), hdr+'Part: alto, clarinet. dotted quarter = 52.', 'p')
# Jingle Bells
q=60.0/100.0; e=q/2; s16=q/4
V=[('Eb4',1)]+[('Eb4',1),('C5',1),('Bb4',1),('Ab4',1), ('Eb4',2),(None,1),('Eb4',.5),('Eb4',.5),
   ('Eb4',1),('C5',1),('Bb4',1),('Ab4',1), ('Eb4',4),
   ('Eb4',1),('Db5',1),('C5',1),('Bb4',1), ('G4',4),
   ('G4',1),('F5',1),('Eb5',1),('Db5',1), ('C5',3),('Eb4',1),
   ('Eb4',1),('C5',1),('Bb4',1),('Ab4',1), ('Eb4',4),
   ('Eb4',1),('C5',1),('Bb4',1),('Ab4',1), ('F4',4),
   ('F4',1),('Db5',1),('C5',1),('Bb4',1), ('G4',1),('Eb5',1),('D5',1),('Eb5',1),
   ('F5',1),('Eb5',1),('Db5',1),('Bb4',1), ('Ab4',4)]   # units: eighths; (x4) = slurred half
R=[('B4',1),('B4',1),('B4',2),('B4',1),('B4',1),('B4',2),
   ('B4',1),('D5',1),('G4',1.5),('A4',.5),('B4',2),(None,2),
   ('C5',1),('C5',1),('C5',1.5),('C5',.5),('C5',1),('B4',1),('B4',1),('B4',.5),('B4',.5),
   ('B4',1),('A4',1),('A4',1),('B4',1),('A4',2),('D5',2),
   ('B4',1),('B4',1),('B4',2),('B4',1),('B4',1),('B4',2),
   ('B4',1),('D5',1),('G4',1.5),('A4',.5),('B4',2),(None,2),
   ('C5',1),('C5',1),('C5',1.5),('C5',.5),('C5',1),('B4',1),('B4',1),('B4',.5),('B4',.5),
   ('D5',1),('D5',1),('C5',1),('A4',1),('G4',3),(None,1)]
jb=[]
for n,u in V: jb.append((None if n is None else midi(n)-1, u*e, 1, 1 if u>=4 else 0))
for n,u in R: jb.append((None if n is None else midi(n), u*e, 1, 1 if u>=3 else 0))
write('jingle_bells_melody.txt', jb, 'Jingle Bells:'+__doc__.split('Jingle Bells:')[1]+'Part: melody, trumpet. quarter = 100. Verse then refrain.', 'mf')
print('ok', sum(x[1] for x in sn(SOP)), sum(x[1] for x in sn(ALT)), sum(x[1] for x in jb))
