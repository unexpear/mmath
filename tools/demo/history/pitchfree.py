"""Score-free intonation check: pyin on voiced frames, deviation from the nearest
ET semitone (A440), only frames inside steady notes (|d cents/frame| < 5 for 3 frames)."""
import numpy as np,librosa,sys
for p in sys.argv[1:]:
    y,sr=librosa.load(p,sr=22050,duration=15)
    f,v,_=librosa.pyin(y,fmin=180,fmax=2000,sr=sr,frame_length=2048,hop_length=256)
    c=1200*np.log2(f/440.0);ok=v&np.isfinite(c);d=np.abs(np.diff(c,prepend=np.nan))
    st=ok&(d<5)&(np.roll(d,1)<5)&(np.roll(d,-1)<5);dev=c[st]-np.round(c[st]/100)*100
    print(f'{p:22s} frames {st.sum():5d} mean|dev| {np.mean(np.abs(dev)):5.1f} c  sd {np.std(dev):5.1f} c  median offset {np.median(dev):+5.1f} c')
