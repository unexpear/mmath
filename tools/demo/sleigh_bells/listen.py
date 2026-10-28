import sys,numpy as n,soundfile as sf,librosa,librosa.display
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
ref,mod,outp=sys.argv[1],sys.argv[2],sys.argv[3]
X={}
for k,p in [('real',ref),('model',mod)]:
    x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x;X[k]=x/n.abs(x).max()
fs=44100
fig=plt.figure(figsize=(18,16))
for i,k in enumerate(X):
    ax=fig.add_subplot(5,2,1+i);S=librosa.amplitude_to_db(n.abs(librosa.stft(X[k].astype(n.float32),n_fft=1024,hop_length=128)),ref=n.max)
    librosa.display.specshow(S,sr=fs,hop_length=128,x_axis='time',y_axis='linear',ax=ax,vmin=-80);ax.set_title(k+' spectrogram')
# accent close-up 6.577
for i,k in enumerate(X):
    x=X[k];a=int(6.45*fs);seg=x[a:a+int(0.3*fs)]
    ax=fig.add_subplot(5,2,3+i);ax.plot(n.arange(len(seg))/fs*1000-127,seg,lw=.4);ax.set_title(k+' strike at 6.577 s (ms re onset)');ax.set_xlim(-127,173)
# impact micro close-up: 6 ms around the strongest sample of the accent
for i,k in enumerate(X):
    x=X[k];a=int(6.55*fs);seg=x[a:a+int(0.06*fs)];j=n.argmax(n.abs(seg));s2=seg[max(0,j-90):j+180]
    ax=fig.add_subplot(5,2,5+i);ax.plot(n.arange(len(s2))/fs*1000,s2,lw=.8);ax.set_title(k+' single impact, 6 ms')
# ring spectrum 120-500 ms after accent vs attack 0-20 ms
for i,k in enumerate(X):
    x=X[k];ax=fig.add_subplot(5,2,7+i)
    for (t0,t1,lab) in [(6.577,6.597,'0-20 ms'),(6.70,7.08,'120-500 ms ring')]:
        s=x[int(t0*fs):int(t1*fs)];f=n.fft.rfftfreq(16384,1/fs);P=20*n.log10(n.abs(n.fft.rfft(s*n.hanning(len(s)),16384))+1e-9)
        ax.plot(f,P-P.max(),lw=.6,label=lab)
    ax.set_ylim(-70,2);ax.set_xlim(0,22050);ax.legend();ax.set_title(k+' spectrum: attack vs ring')
# rattle texture: envelope 0.1-1.1 s at 2 ms
for i,k in enumerate(X):
    x=X[k];s=x[int(.1*fs):int(1.15*fs)];w=88;e=n.sqrt(n.convolve(s**2,n.ones(w)/w,'same'))[::w]
    ax=fig.add_subplot(5,2,9+i);ax.plot(n.arange(len(e))*w/fs+.1,20*n.log10(e+1e-9),lw=.6);ax.set_ylim(-70,0);ax.set_title(k+' rattle texture 0.1-1.15 s, 2 ms envelope')
plt.tight_layout();plt.savefig(outp,dpi=60)
