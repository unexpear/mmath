"""Check-history table: one run of audiodiag/diag.py + vt36 trend/cycle (tr) + vt37 nn note-to-note, all clips vs one real ref.
Usage: hist.py violin|guitar"""
import sys,subprocess,numpy as np,soundfile as sf,librosa
sys.path.insert(0,'/workspace/vt32');from per import feats,ac
from rep import lu
from scipy.ndimage import uniform_filter1d
import matplotlib;matplotlib.use('Agg');import matplotlib.pyplot as plt
inst=sys.argv[1]
if inst=='violin':
    C=[('v5','v5'),('v10','v10'),('v19','v19'),('v20','v20'),('v21','v21'),('v29','v29'),('v37/38','v38'),('v41','v41'),('v42','v42'),('v43','v43'),('v44','v44'),('v45','v45')]
    cols=[(f'{n} dry',f'{k}_dry') for n,k in C]+[(f'{n} studio',f'{k}_room') for n,k in C]
    real='vreal.wav';score='/workspace/scores/caprice24v.csv';tempo='1.5';dur=15;lag=1.95;out='/workspace/violin-history45.png'
    TG={('/workspace/scores/caprice24v.csv','1.5'):['v29_dry','v38_dry','v29_room','v38_room'],('/workspace/scores/caprice24v.csv','1.61745'):['v41_dry','v42_dry','v43_dry','v44_dry','v45_dry','v41_room','v42_room','v43_room','v44_room','v45_room']}
    title='Violin check history: ONE diag run for all metrics + trend/cycle + note-to-note, ref vt6/real.wav. Timing row: per-version tempo runs (v29/v38 x1.5, v41-v44 x1.617); v5-v21 tempo unknown -> n/a'
else:
    cols=[('old solo q=90 (Oct 14)','g_old'),('solo new q=90','g_new'),('pro1 dry q=72','gp1_dry'),('pro1 studio q=72','gp1_room'),('pro2 dry q=72','gp2_dry'),('pro2 studio q=72','gp2_room'),('pro3 dry q=72','gp3_dry'),('pro3 studio q=72','gp3_room'),('pro4 dry q=72','gp4_dry'),('pro4 studio q=72','gp4_room'),('pro5 dry q=72','gp5_dry'),('pro5 studio q=72','gp5_room')]
    real='greal.wav';score='/workspace/gp/guitar72.csv';tempo='1';dur=13;lag=2.5;out='/workspace/guitar-history5.png'
    TG={('/workspace/scores/guitar.csv','1'):['g_old','g_new'],('/workspace/gp/guitar72.csv','1'):['gp1_dry','gp1_room','gp2_dry','gp2_room','gp3_dry','gp3_room','gp4_dry','gp4_room','gp5_dry','gp5_room']}
    title='Guitar check history: ONE diag run + trend/cycle + note-to-note, ref Greeninger CC0 (Romance, 1.9-14.9 s). Timing row: per-version grid (old q=90, pro q=72)'
args=['python3','/workspace/audiodiag/diag.py','--clip',f'real={real}']+sum([['--clip',f'{k}={k}.wav'] for _,k in cols],[])+['--ref','real','--dur',str(dur),'--score',score,'--tempo',tempo,'--out',f'/tmp/{inst}_hist.pdf','--png-dir',f'/tmp/{inst}_hist']
o=subprocess.run(args,capture_output=True,text=True).stdout
D={}
for l in o.splitlines():
    s=l.split(' | ')
    if len(s)>=6: D[(s[0],s[1])]=(s[2],s[3],s[5])
M=['Momentary loudness range p95-p10 (LU)','Short-term loudness std (LU)','RMS envelope swing p90-p10 (dB)','Band 400-500 Hz re 0.5-1.5k (dB)','Band 2-3 kHz re 0.5-1.5k (dB)','Band 5-10 kHz re 0.5-1.5k (dB)','Band 200-320 Hz re 0.5-1.5k (dB)','Max |1/3-oct diff| 100 Hz-10 kHz (dB)','Pitch mean |cents| vs ET (pyin)','Pitch spread std cents','Vibrato rate Hz (median, measurable notes)',
   'Harmonic slope partials 1-12 (dB/partial)','Partial 2 re strongest (dB)','Spectral centroid median','Attack rise 10-90% median (ms)','Note peak-to-gap depth median (dB)','In-note decay slope median (dB/s)','Decay T60-equiv from falling envelopes (s)','Onset deviation sd vs model timing (ms)']
if inst=='guitar': M=[m for m in M if not m.startswith('Vibrato')]
def extra(p):
    x,fs=sf.read(p);x=x.mean(1) if x.ndim>1 else x;x=x[:int(dur*fs)]
    L,Cn=feats(x,fs);Ls=uniform_filter1d(L,80);k=slice(40,-40)
    n=3*fs;h=int(.1*fs);s=np.array([10*np.log10(np.mean(x[i:i+n]**2)+1e-20) for i in range(0,len(x)-n,h)])
    a=ac(L);on=librosa.onset.onset_detect(y=x.astype(np.float32),sr=fs,units='samples',backtrack=True)
    Lv=[10*np.log10(np.mean(x[a_:b_]**2)+1e-20) for a_,b_ in zip(on[:-1],on[1:]) if b_-a_>=1024]
    seg=x[:];P=np.abs(librosa.stft(seg.astype(np.float32),n_fft=4096))**2;f=librosa.fft_frequencies(sr=fs,n_fft=4096);P=P.sum(1)
    share=100*P[(f>=400)&(f<500)].sum()/P[(f>=100)&(f<10000)].sum()
    return {'LU range (rep.lu, 0.4 s)':lu(x,fs),'3 s loudness std (dB)':s.std(),f'Loudness cycle ac @{lag} s':a[int(round(lag/0.05))],'Trend span, 4 s-smoothed (dB)':np.ptp(Ls[k]),'Note-to-note level std (dB)':np.std(np.diff(Lv)),'400-500 Hz energy share (%)':share}
TM='Onset deviation sd vs model timing (ms)'
for (sc,tp),ks in TG.items():
    a2=['python3','/workspace/audiodiag/diag.py','--clip',f'real={real}']+sum([['--clip',f'{k}={k}.wav'] for k in ks],[])+['--ref','real','--dur',str(dur),'--score',sc,'--tempo',tp,'--out',f'/tmp/t_{inst}.pdf','--png-dir',f'/tmp/t_{inst}']
    for l in subprocess.run(a2,capture_output=True,text=True).stdout.splitlines():
        q=l.split(' | ')
        if len(q)>=6 and q[0]==TM and q[1] in ks: D[(q[0],q[1])]=(q[2],q[3],q[5])
for _,k in cols:
    if not any(k in v for v in TG.values()): D.pop((TM,k),None)
E={k:extra(f'{k}.wav') for k in ['real']+[k for _,k in cols] if k!='real'} ;E['real']=extra(real)
def band(v,r,m=''):
    if 'cycle' in m and np.isfinite(v): d=abs(v-r); return '#9be59b' if d<=0.1 else '#ffe08a' if d<=0.25 else '#ff9b9b'
    if not np.isfinite(v) or not np.isfinite(r): return '#ddd'
    d=abs(v-r)/max(abs(r),1e-9); return '#9be59b' if d<=0.15 else '#ffe08a' if d<=0.5 else '#ff9b9b'
col={'match':'#9be59b','close':'#ffe08a','wrong':'#ff9b9b'}
cells=[];colors=[];worst=[]
for m in M:
    row=[m[:44]];cr=['w'];rv=next((D[(m,k)][1] for _,k in cols if (m,k) in D),'-')
    for _,k in cols:
        v=D.get((m,k));row.append(v[0] if v else '-');cr.append(col.get(v[2],'#ddd') if v else '#ddd')
    row.append(rv);cr.append('w');cells.append(row);colors.append(cr)
for m in E['real']:
    r=E['real'][m];row=[m+' *'];cr=['w']
    for _,k in cols: v=E[k][m];row.append(f'{v:.3g}');cr.append(band(v,r,m))
    row.append(f'{r:.3g}');cr.append('w');cells.append(row);colors.append(cr)
f,ax=plt.subplots(figsize=(4+1.7*len(cols),0.42*len(cells)+1.5));ax.axis('off')
t=ax.table(cellText=cells,cellColours=colors,colLabels=['metric']+[n for n,_ in cols]+['real'],loc='center',cellLoc='center');t.auto_set_font_size(False);t.set_fontsize(9);t.scale(1,1.5)
for (i,j),c in t.get_celld().items():
    if j==0: c.set_width(0.24*10/(len(cols)+3))
ax.set_title(title+'\ngreen match / yellow close / red off. Rows *: new metrics, bands |diff|/|real| <=15% green, <=50% yellow, else red; cycle ac absolute |diff| <=0.10 / <=0.25',fontsize=10)
f.savefig(out,dpi=100,bbox_inches='tight')
for r in cells: print(' | '.join(r))
