"""Transcribe a sleigh-bell recording into a shake/strike gesture timeline.
Onsets: librosa onset strength (hop 256), level = 30 ms RMS after onset, dB re
loudest. Strike = onset >= STRIKE_DB that is not a pre-hit (an onset within
80 ms before a louder one: the cap hit on the down-swing). Background shake:
median 50 ms RMS (dB re loudest onset) per 0.5 s window, skipping 150 ms after
each strike; windows with background below QUIET_DB are hand-still."""
import sys, numpy as n, soundfile as sf, librosa
STRIKE_DB, QUIET_DB = -17.0, -38.0
def transcribe(path):
    x, fs = sf.read(path); x = x.mean(1) if x.ndim > 1 else x
    hop = 256
    oe = librosa.onset.onset_strength(y=x.astype(n.float32), sr=fs, hop_length=hop)
    fr = librosa.onset.onset_detect(onset_envelope=oe, sr=fs, hop_length=hop, delta=0.2)
    t = librosa.frames_to_time(fr, sr=fs, hop_length=hop)
    lv = n.array([20*n.log10(n.sqrt(n.mean(x[int(a*fs):int(a*fs)+int(.03*fs)]**2))+1e-12) for a in t])
    ref = lv.max(); lv -= ref
    strikes = []
    for i, (a, l) in enumerate(zip(t, lv)):
        if l < STRIKE_DB: continue
        if any(0 < t[j]-a < 0.08 and lv[j] > l for j in range(len(t))): continue
        strikes.append((float(a), float(l)))
    w = int(.05*fs); e = n.sqrt(n.convolve(x**2, n.ones(w)/w, 'same'))
    bg = []
    for k in range(int(len(x)/fs/0.5)):
        a, b = k*0.5, (k+1)*0.5
        idx = n.arange(int(a*fs), int(b*fs), w//2)
        keep = [i for i in idx if not any(0 <= i/fs - s < 0.15 for s, _ in strikes)]
        v = 20*n.log10(n.median(e[keep])+1e-12)-ref if keep else -99
        bg.append((a, float(v)))
    return strikes, bg, len(x)/fs, oe, fs, hop
if __name__ == '__main__':
    s, bg, dur, oe, fs, hop = transcribe(sys.argv[1])
    tempo = librosa.beat.beat_track(onset_envelope=oe, sr=fs, hop_length=hop)[0]
    print('dur %.3f tempo %s' % (dur, tempo))
    for a, l in s: print('strike %.3f %+.1f' % (a, l))
    for a, v in bg: print('bg %.1f %+.1f' % (a, v))
    ioi = n.diff([a for a, _ in s]); print('IOI', n.round(ioi, 3))
