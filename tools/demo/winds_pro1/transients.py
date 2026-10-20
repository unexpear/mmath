"""Note transients, real vs model, on the fundamental (f0 +-8 %, 5 ms RMS):
dip  = level drop into a tongued join (min in [a-80, a+30] ms re the previous note's median),
grow = exponential growth rate after the dip (dB/s, fit from min+3 dB to the first frame within 3 dB of the
       new note's median),
rel  = release decay rate at the phrase end / before rests (dB/s, fit over the first 20 dB below the note median).
Li, Almeida, Smith, Wolfe JASA 139, 825 (2016): normal attacks ~1000 dB/s, accents ~1300, tongue-stopped
staccato decay ~400 dB/s (C4 340, G4 400, C5 380, G5 520), breath-stopped notes decay about half as fast."""
import sys,os,numpy as n,soundfile as sf,librosa
from scipy.signal import butter,sosfiltfilt
HERE=os.path.dirname(os.path.abspath(__file__))

src=open(os.path.join(HERE,'wpscore.py')).read().split('out,tag,insts=')[0]
g={};exec(src,g);g['EXC']=os.path.join(HERE,'..','..','..','tests','wind_excerpts.h')
def band_env(x,fs,hz):
    y=sosfiltfilt(butter(2,[hz*0.92,hz*1.08],'band',fs=fs,output='sos'),x);h=int(0.005*fs)
    e=n.sqrt(n.convolve(y**2,n.ones(h)/h,'same'))[::h];return 20*n.log10(e+1e-9)
def analyse(path,inst,off):
    x,fs=g['load'](path);N=g['notes_of'](inst);D=[];G=[];R=[]
    allnotes=N
    for k,(a,b,hz,tg) in enumerate(N):
        a+=off;b+=off;E=band_env(x,fs,hz);t=n.arange(len(E))*0.005
        med=n.median(E[(t>a+0.3*(b-a))&(t<b-0.2*(b-a))]) if b-a>0.1 else E[(t>a)&(t<b)].max()
        if tg and k>0 and abs(N[k-1][1]+off-a)<0.02:
            pa,pb,phz,_=N[k-1];pa+=off;pb+=off;Ep=band_env(x,fs,phz)
            pm=n.median(Ep[(t>pa+0.3*(pb-pa))&(t<pb-0.2*(pb-pa))]) if pb-pa>0.1 else Ep[(t>pa)&(t<pb)].max()
            w=(t>=a-0.08)&(t<=a+0.03)
            lvl=n.minimum(E,Ep)  # either fundamental
            i=n.where(w)[0];j=i[n.argmin(E[i])];D.append(min(E[j],Ep[j])-pm if False else E[j]-med)
            up=n.where((t>t[j])&(E>=med-3))[0]
            if up.size:
                s=n.arange(j,up[0]+1);s=s[E[s]>=E[j]+3]
                if s.size>=3:G.append(n.polyfit(t[s],E[s],1)[0])
        last=k==len(N)-1 or N[k+1][0]-N[k][1]>0.05
        if last:
            after=n.where((t>b-0.05)&(E<med-3))[0]
            if after.size:
                s=n.arange(after[0],min(len(E),after[0]+200));s=s[(E[s]<med-3)&(E[s]>med-23)]
                if s.size>=4:R.append(n.polyfit(t[s],E[s],1)[0])
    return D,G,R
if __name__=='__main__':
    for inst in sys.argv[2].split(','):
        for nm,p,off in [('real','ref_%s.wav'%inst,0.0)]+[(tg,'%s_%s.f64'%(tg,inst),g['LEAD']) for tg in sys.argv[1].split('+')]:
            
            D,G,R=analyse(p,inst,off)
            f=lambda v:'%6.0f'%n.median(v) if len(v) else '    --'
            print('%-9s %-5s dip %s dB (n=%d)  grow %s dB/s (n=%d)  release %s dB/s (n=%d)'%(inst,nm,f(D),len(D),f(G),len(G),f(R),len(R)))
