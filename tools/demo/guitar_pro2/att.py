"""Attack recheck: 10-90% rise of the >2 kHz band envelope (1 ms hop) at each onset,
measured above the pre-onset floor, so ringing earlier notes (mostly <2 kHz) do not smear it."""
import numpy as np,librosa,scipy.signal as s,sys
for p in sys.argv[1:]:
    y,sr=librosa.load(p,sr=48000,duration=13);b,a=s.butter(4,2000,'high',fs=sr);h=s.filtfilt(b,a,y)
    e=np.sqrt(s.filtfilt(*s.butter(2,200,fs=sr),h**2).clip(0));hop=48;e=e[::hop]
    on=librosa.onset.onset_detect(y=y,sr=sr,units='samples',hop_length=256)//hop;R=[]
    for o in on:
        seg=e[max(0,o-20):o+80]
        if len(seg)<100: continue
        lo=seg[:15].min();pk=seg.max();
        if pk<3*lo: continue
        n=(seg-lo)/(pk-lo);i10=np.argmax(n>=.1);i90=np.argmax(n>=.9);R.append(i90-i10)
    print(p,'n',len(R),'median rise ms',np.median(R) if R else None)
