"""Caprice 24 demo metronome overlay (harness, listening aid only).
Usage: metronome.py in.raw(f64 mono 48k) out.wav [bpm=115] [beats_per_bar=2]
Click grid = score beat grid: quarter period 60/bpm s, starting at score t=0.
Bar downbeat 1800 Hz / beat 1200 Hz, 25 ms decaying sine (listening aid, not physics)."""
import sys, numpy as np, soundfile as sf
sr = 48000; bpm = float(sys.argv[3]) if len(sys.argv) > 3 else 115; bpb = int(sys.argv[4]) if len(sys.argv) > 4 else 2
x = np.fromfile(sys.argv[1], '<f8'); x = x / np.max(np.abs(x)) * 0.5
q = 60 / bpm; L = int(0.025 * sr); t = np.arange(L) / sr; c = np.zeros_like(x)
for k in range(int(len(x) / sr / q) + 1):
    f, a = (1800, 0.35) if k % bpb == 0 else (1200, 0.22)
    i = int(k * q * sr); seg = c[i:i + L]; seg += (a * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.006))[:len(seg)]
sf.write(sys.argv[2], x + c, sr, subtype='FLOAT')
