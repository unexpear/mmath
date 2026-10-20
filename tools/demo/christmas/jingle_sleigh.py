#!/usr/bin/env python3
"""Sleigh-bell gesture timeline for jingle_bells_melody.txt (format: see
parseSleighBellTimeline, include/mmathen/instruments/sleigh_bell.h).
Grid from the score: 0.56 s lead-in + eighth pickup, quarter = 0.6 s;
verse 16 bars of 2/4, refrain 8 bars of 4/4. A strike on every quarter,
the bar downbeat harder; a light shake under the whole song. Hand speeds
and shake amplitude are performance choices (UNSOURCED), inside the range
transcribed from the Commons sleigh-bell recording (ref_timeline.txt:
strikes 0.42-3.0 m/s, shake 0.017 m)."""
q=0.6; t0=0.56+0.3; verse_end=t0+16*2*q; end=verse_end+8*4*q
L=['# '+l for l in __doc__.strip().splitlines()]
L.append('shake 0.300 %.3f 0.0100 0.0100 0.30'%(end-0.3-1.0))
L.append('shake %.3f 1.000 0.0100 0.0000 0.30'%(end-1.0))
t=t0;k=0
while t<end-0.01:
    inverse = t<verse_end
    beats=2 if inverse else 4
    b=k%beats if inverse else (k-32)%4
    v=1.6 if b==0 else (1.1 if (not inverse and b==2) else 0.9)
    L.append('strike %.3f %.3f %+.1f'%(t,v,0.3 if k%2==0 else -0.3)); t+=q; k+=1
L.append('strike %.3f 2.000 +0.3'%end)
open('jingle_sleigh_timeline.txt','w').write('\n'.join(L)+'\n'); print(len(L), end)
