"""Jingle Bells band arrangement: double bass (pizz) and piano parts.
Grid (jingle_sleigh_timeline.txt): 0.56 s lead-in + eighth pickup, quarter
0.6 s; verse 16 bars of 2/4 from 0.86 s, refrain 8 bars of 4/4 from 20.06 s.
G major (Pierpont's Ab transposed down a semitone, as the melody).
Verse harmony and piano figures: J. Pierpont, "The One Horse Open Sleigh"
(1857), Project Gutenberg eBook #10535 (10535.ly, lower and upper staves,
verse), public domain. Refrain harmony and bass line: Wikimedia Commons
File:Jingle_Bells_refrain_vector.svg LilyPond SATB (Pierpont 1857 refrain,
public domain; engraving released PD by Niamh O'C). Refrain piano pattern:
Pierpont's 1857 chorus accompaniment (10535.ly lower staff: octave bass
eighth, chord, chord, rest). Double bass: the piano's root an octave lower
in the verse (Pierpont writes half-note roots), the SATB bass voice an octave
lower in the refrain. Voicings in the piano right hand are choices
(UNSOURCED) inside those chords. Velocities: performance choices."""
Q=0.6;E=Q/2;V0=0.86;R0=V0+16*2*Q
G,C,D=43,48,50
piano=[];bass=[]
def P(t,d,m,v=60):piano.append((t,d,m,v))
def B(t,d,m,v=96):bass.append((t,d,m,v))
# Verse: chord per 2/4 bar (Pierpont, -1 semitone), LH figure intervals.
verse=[('G',[0,12,7,12]),('G',[0,12,7,12]),('G',[0,12,7,12]),('C',[0,7,4,7]),('C',[0,9,4,9]),('D',[0,7,4,7]),
       ('D','q'),('G','q'),('G',[0,12,7,12]),('G',[0,12,7,12]),('G',[0,12,7,12]),('C',[0,7,4,7]),('C',[0,9,4,9]),
       ('D','e'),('D7','e'),('G','h')]
root={'G':G,'C':C,'D':D,'D7':D}
rh={'G':[59,62],'C':[60,64],'D':[62,66]}          # Pierpont's RH dyads <c ees> <des f> <ees g>, -1 semitone
rh3={'D':[62,66,69],'G':[59,62,67],'D7':[57,60,62,66]}  # bars 7-8, 14-16: <ees g bes> <c ees aes> <bes des ees g>
for i,(ch,fig) in enumerate(verse):
  t=V0+i*2*Q;r=root[ch]
  if isinstance(fig,list):
    for k,iv in enumerate(fig):P(t+k*E,E,r+iv,58)
    for m in rh[ch]:P(t,2*Q,m,52)
    B(t,2*Q,r-12)
  elif fig=='q':
    P(t,Q,r,58);B(t,Q,r-12)
    for k in (1,2,3):
      for m in rh3[ch]:P(t+k*E,E,m,50)
  elif fig=='e':
    for k in (0,2):P(t+k*E,E,r,58);B(t+k*E,E,r-12)
    for k in (1,3):
      for m in rh3[ch]:P(t+k*E,E,m,50)
  else:  # final half: octave G + chord
    P(t,2*Q,r,58);P(t,2*Q,r+12,58)
    for m in rh3['G']:P(t,2*Q,m,52)
    B(t,2*Q,r-12)
# Refrain: SATB bass voice (sounding octave lower on the double bass) and
# chords per half bar.
sb=[ # (start in eighths within the 4/4 bar, length in eighths, voice midi)
 [(0,1,55),(1,1,55),(2,2,55),(4,1,55),(5,1,55),(6,2,55)],
 [(0,1,55),(1,1,55),(2,1.5,55),(3.5,.5,55),(4,2,55)],
 [(0,1,48),(1,1,48),(2,1.5,48),(3.5,.5,48),(4,1,43),(5,1,43),(6,1,43),(7,.5,55),(7.5,.5,55)],
 [(0,1,52),(1,1,52),(2,1,45),(3,1,45),(4,2,50),(6,2,50)],
]
sb=sb+sb[:3]+[[(0,1,50),(1,1,50),(2,1,50),(3,1,50),(4,3,55)]]
halves=['G','G','G','G','C','G','A7','D','G','G','G','G','C','G','D7','G']
ch3={'G':[59,62,67],'C':[60,64,67],'A7':[61,64,67],'D':[57,62,66],'D7':[57,60,66]}
lowr={'G':43,'C':48,'A7':45,'D':50,'D7':50}
for b in range(8):
  t0=R0+b*4*Q
  for s,l,m in sb[b]:B(t0+s*E,l*E,m-12)
  for h in range(2):
    t=t0+h*2*Q;ch=halves[2*b+h];r=lowr[ch]
    if b==3 and h==0: r=40   # SATB bass E under the A7 half (e8 e a, a)
    last=(b==7 and h==1)
    if last:
      P(t,3*Q,r,58);P(t,3*Q,r+12,58)
      for m in ch3['G']:P(t,3*Q,m,52)
      continue
    P(t,E,r,58);P(t,E,r+12,58)
    for k in (1,2):
      for m in ch3[ch]:P(t+k*E,E,m,50)
for nm,ev in (('jb_piano.csv',piano),('jb_bass.csv',bass)):
  ev.sort();open(nm,'w').write(''.join(f'{a:.4f},{d:.4f},{m},{v}\n' for a,d,m,v in ev))
print(len(piano),len(bass),'end',R0+8*4*Q)
