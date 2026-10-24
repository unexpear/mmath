"""Patapan band arrangement: flute (melody), deep cello, classical guitar, snare.

Tune: Bernard de La Monnoye, "Guillo, pran ton tamborin" (Noei borguignon,
1720, PD). Harmony: Charles Wood (1866-1926, PD) SATB, as encoded at
hymnsandcarolsofchristmas.com/Hymns_and_Carols/MIDI/Guillo_pran_ton_tamborin-da.mid
(page cites Sandys 1833). A minor, 4/4, quarter = 140 (the MIDI's tempo).
Read with mido from ../../../pat/patapan.mid (not committed; the note
lists written here are the arrangement's whole input).

Form: 1 bar snare "pa-ta-pa-ta-pan" intro, then the 12-bar verse twice.
Parts: flute = soprano, tongued, mf (vibrato on notes >= a half);
cello = bass at pitch (A2-A3, the C and G strings), detached;
guitar = alto + tenor dyad per note (verse 2 adds the bass an octave up
on beats 1 and 3); snare = a tap per quarter, accented on 1, eighths
where the soprano moves in eighths, a roll-free "patapan" figure at
the cadences. Velocities are UNSOURCED arranging choices.
Writes pat_flute.txt (wp1 WP_SONGFILE), pat_cello.csv, pat_guitar.csv,
pat_snare.csv (start,dur,midi,vel)."""
import mido, sys
SRC = sys.argv[1] if len(sys.argv) > 1 else '../../../pat/patapan.mid'
m = mido.MidiFile(SRC); tpb = m.ticks_per_beat
Q = 60.0 / 140.0
V = {}
for t in m.tracks[1:]:
    now = 0; on = {}; L = []
    for x in t:
        now += x.time
        if x.type == 'note_on' and x.velocity > 0: on[x.note] = now
        elif x.type in ('note_off', 'note_on') and x.note in on:
            L.append((round(on.pop(x.note) / tpb * 4) / 4, x.note))
    L.sort(); V[t.name] = L
BEATS = 48
def durs(L):
    out = []
    for i, (s, n) in enumerate(L):
        e = L[i + 1][0] if i + 1 < len(L) else BEATS
        out.append((s, e - s, n))
    return out
S, A, T, B = (durs(V[k]) for k in ('Soprano', 'Alto', 'Tenor', 'Bass'))
INTRO = 4  # beats
NAME = ['C', 'C#', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
nm = lambda p: f"{NAME[p % 12]}{p // 12 - 1}"
hz = lambda p: 440 * 2 ** ((p - 69) / 12)
fl = ["# Patapan band flute: soprano of Charles Wood SATB (PD); see arrange_pat.py", f"R 0 {INTRO * Q:.4f} 0 0 mf"]
for v in range(2):
    for s, d, p in S:
        fl.append(f"{nm(p)} {hz(p):.2f} {d * Q:.4f} 1 {1 if d >= 2 else 0} mf")
fl.append("R 0 1.5 0 0 mf")
open('pat_flute.txt', 'w').write('\n'.join(fl) + '\n')
vc, gt, sd = [], [], []
for v in range(2):
    o = INTRO + v * BEATS
    for s, d, p in B:
        vc.append(((o + s) * Q, min(d, 1.0) * Q * 0.9 if d < 1.5 else d * Q * 0.95, p, 46 if v == 0 else 52))
    for L, dl in ((T, 0.0), (A, 0.012)):
        for s, d, p in L:
            gt.append(((o + s) * Q + dl, d * Q, p, 40 if v == 0 else 46))
    if v == 1:
        for s, d, p in B:
            if s % 2 == 0: gt.append(((o + s) * Q - 0.012, 2 * Q, p + 12 if p < 52 else p, 50))
    eighth = {s for s, d, p in S if d == 0.5}
    for b in range(BEATS):
        bar_end = (b % 4 == 3) and (b // 4) in (3, 7, 11)
        sd.append(((o + b) * Q, 0.3, 38, 88 if b % 4 == 0 else 60))
        if b + 0.5 in eighth or (b in eighth and b + 0.5 < BEATS):
            sd.append(((o + b + 0.5) * Q, 0.3, 38, 52))
# intro: pa-ta-pa-ta-pan
for k, (b, vel) in enumerate(((0, 70), (0.5, 55), (1, 70), (1.5, 55), (2, 100))):
    sd.append((b * Q, 0.3, 38, vel))
sd.sort()
rows = lambda L: '\n'.join(f"{a:.4f},{b:.4f},{m},{v}" for a, b, m, v in L) + '\n'
open('pat_cello.csv', 'w').write(rows(vc)); open('pat_guitar.csv', 'w').write(rows(sorted(gt)))
open('pat_snare.csv', 'w').write(rows(sd))
print('flute', len(fl), 'cello', len(vc), 'guitar', len(gt), 'snare', len(sd), 'length s', (INTRO + 2 * BEATS) * Q + 1.5)
