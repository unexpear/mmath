"""Write the gesture timeline for a sleigh-bell recording (text form read by
mmathen::parseSleighBellTimeline). usage: gentl.py ref.wav out.txt [shakeAmpM] [refSpeed]"""
import sys, numpy as n, librosa, soundfile as sf
sys.path.insert(0, __file__.rsplit('/', 1)[0] if '/' in __file__ else '.')
from sbpattern import transcribe, QUIET_DB
ref = sys.argv[1]; out = sys.argv[2]
A = float(sys.argv[3]) if len(sys.argv) > 3 else 0.012
# Strike speed for a level: inverse of the model's own single-strike level
# curve (6 bells, seeds 3/5/9, 50 ms peak RMS, dB re the loudest), made
# monotone. TUNED calibration, measured with sbpat on cal.txt.
CAL_V = [0.35, 0.413, 0.487, 0.943, 1.113, 1.313, 1.549, 2.543, 3.0]
CAL_DB = [-58.4, -31.6, -12.7, -10.2, -6.6, -2.2, -0.9, -0.3, 0.0]
def speed(db): return float(n.interp(db, CAL_DB, CAL_V))
strikes, bg, dur, oe, fs, hop = transcribe(ref)
# hand-still from the first background window below -28 dB that stays low
stop = next(a for a, v in bg if v < -28.0)
# fade: hand slows until the background is 40 dB down
fade = next((a for a, v in bg if a > stop and v < -40.0), stop + 0.5) - stop
# soft late hits: onsets after the stop within 30 dB of the loudest
x, _ = sf.read(ref); x = x.mean(1) if x.ndim > 1 else x
fr = librosa.onset.onset_detect(onset_envelope=oe, sr=fs, hop_length=hop, delta=0.2)
t = librosa.frames_to_time(fr, sr=fs, hop_length=hop)
lv = n.array([20*n.log10(n.sqrt(n.mean(x[int(a*fs):int(a*fs)+int(.03*fs)]**2))+1e-12) for a in t])
lv -= lv.max()
late = [(float(a), float(l)) for a, l in zip(t, lv) if a > stop + 0.3 and l > -30.0]
L = ['# sleigh-bell gesture timeline transcribed from Wikimedia Commons File:Sleigh_bells.wav (CC0, The Midnite Wolf) by tools/demo/sleigh_bells/gentl.py + sbpattern.py',
     '# strike <t s> <hand speed m/s> <lateral>; shake <t s> <dur s> <amp m> <end amp m> <lateral>',
     '# strike speed from the onset level (30 ms RMS, dB re the loudest) via the model level curve in gentl.py',
     'shake 0.000 %.3f %.4f %.4f 0.3' % (stop, A, A),
     'shake %.3f %.3f %.4f 0.0000 0.3' % (stop, fade, A)]
for k, (a, l) in enumerate(strikes + late):
    L.append('strike %.3f %.3f %+.1f   # %+.1f dB' % (a, speed(l), 0.3 if k % 2 == 0 else -0.3, l))
open(out, 'w').write('\n'.join(L) + '\n')
print('\n'.join(L))
