"""Development only: audio-only harmonic and stable-pitch candidates.

Independent NumPy reference for a later native implementation. No label is
passed to either recognizer. Reports use full chord quality and identical pitch
eligibility, including misses. The reserved recordings are never scored here.
"""
import argparse
import json
from pathlib import Path
import time
import zipfile
import numpy as np
import soundfile as sf
from scipy.optimize import nnls
import mir_eval.chord as chord
from evaluate import parse_guitar, score_chords, save, chord_cells
from compare_tuners import reference_at, error
from prepare import digest, KNOWN_ISSUES

QUALITIES = [('maj', [0,4,7]), ('min',[0,3,7]), ('7',[0,4,7,10]),
             ('maj7',[0,4,7,11]), ('min7',[0,3,7,10]), ('dim',[0,3,6]),
             ('aug',[0,4,8]), ('sus2',[0,2,7]), ('sus4',[0,5,7]),
             ('maj6',[0,4,7,9]), ('min6',[0,3,7,9]), ('hdim7',[0,3,6,10]),
             ('dim7',[0,3,6,9]), ('9',[0,2,4,7,10]), ('min9',[0,2,3,7,10])]
ROOTS = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
DEGREES = ['1','b2','2','b3','3','4','b5','5','b6','6','b7','7']

def freeze_pitch(root, target):
    path=target/'pitch-holdout.json'
    if path.exists(): return
    used={t['id'] for name in ('manifest.json','accuracy/holdout.json')
          for t in json.loads((root/name).read_text())['tracks']}
    tracks=[]
    with zipfile.ZipFile(root/'annotation.zip') as az, zipfile.ZipFile(root/'audio_mono-mic.zip') as wz:
        names=sorted(Path(n).stem for n in az.namelist() if n.endswith('.jams') and '/' not in n)
        wavs={Path(n).name:n for n in wz.namelist() if not n.startswith('__MACOSX')}
        for player in range(6):
            for style in ('BN','Funk','Jazz','Rock','SS'):
                name=next(n for n in names if n.startswith(f'{player:02d}_{style}') and n.endswith('_solo')
                          and n not in used and n not in KNOWN_ISSUES)
                audio, ref=target/(name+'.wav'), target/(name+'.jams')
                audio.write_bytes(wz.read(wavs[name+'_mic.wav'])); ref.write_bytes(az.read(name+'.jams'))
                tracks.append(dict(id=name,mode='pitch',audio=audio.relative_to(root).as_posix(),
                    reference=ref.relative_to(root).as_posix(),audioSha256=digest(audio),referenceSha256=digest(ref)))
    save(path,dict(selection='First unused solo per player/style; excludes both previous sets and documented issues; frozen before predictions.',tracks=tracks))

def unit(x):
    return x/np.maximum(np.linalg.norm(x,axis=-1,keepdims=True),1e-20)

def decode(emissions, switch=3.0):
    # Viterbi with equal transition cost to each other state, O(frames*states).
    score=emissions[0].copy(); back=[]; states=np.arange(len(score))
    for row in emissions[1:]:
        best=int(np.argmax(score)); changed=score[best]-switch
        stay=score >= changed
        back.append(np.where(stay,states,best))
        score=np.where(stay,score,changed)+row
        score-=np.max(score)
    path=[int(np.argmax(score))]
    for row in reversed(back): path.append(int(row[path[-1]]))
    return path[::-1]

def templates(bass):
    labels=[]; chroma=[]; low=[]
    for root in range(12):
        for quality, intervals in QUALITIES:
            for inversion in (intervals if bass else [0]):
                v=np.zeros(12); v[(np.array(intervals)+root)%12]=1
                b=np.zeros(12); b[(root+inversion)%12]=1
                labels.append(ROOTS[root]+':'+quality+('/'+DEGREES[inversion] if inversion else ''))
                chroma.append(v); low.append(b)
    labels.append('N'); chroma.append(np.ones(12)); low.append(np.zeros(12))
    return labels,unit(np.array(chroma)),np.array(low)

def harmonic_features(audio, rate):
    n=8192; hop=2048
    if len(audio)<n: audio=np.pad(audio,(0,n-len(audio)))
    frames=np.lib.stride_tricks.sliding_window_view(audio,n)[::hop]
    win=.5-.5*np.cos(2*np.pi*np.arange(n)/n)
    spectrum=np.abs(np.fft.rfft(frames*win,axis=1))
    hz=np.arange(spectrum.shape[1])*rate/n
    mask=(hz>=55)&(hz<=5000); bins=hz[mask]; spectrum=spectrum[:,mask]
    rms=np.sqrt(np.mean(frames*frames,axis=1))
    # Estimate global tuning from resolved upper spectral peaks, by circular mean.
    peak=(spectrum[:,1:-1]>spectrum[:,:-2])&(spectrum[:,1:-1]>=spectrum[:,2:])
    fi,ki=np.where(peak); ki+=1
    keep=(bins[ki]>=200)&(spectrum[fi,ki]>.1*np.max(spectrum,axis=1)[fi])
    fi,ki=fi[keep],ki[keep]
    a,b,c=np.log(np.maximum(np.stack([spectrum[fi,ki-1],spectrum[fi,ki],spectrum[fi,ki+1]]),1e-20))
    offsets=.5*(a-c)/(a-2*b+c)
    midi=69+12*np.log2((bins[ki]+offsets*rate/n)/440)
    z=np.sum(spectrum[fi,ki]*np.exp(2j*np.pi*midi))
    tuning=np.angle(z)*100/(2*np.pi) if abs(z)>0 else 0
    # Analytic periodic-Hann main lobe; tolerance also covers slight detuning.
    notes=np.arange(36,96); fundamental=440*2**((notes-69)/12+tuning/1200)
    dictionary=np.zeros((len(bins),len(notes)))
    for h in range(1,13):
        delta=(bins[:,None]-h*fundamental[None,:])/(rate/n)
        lobe=np.abs(np.sinc(delta)+.5*np.sinc(delta-1)+.5*np.sinc(delta+1))
        dictionary+=.7**(h-1)*np.where(abs(delta)<3,lobe,0)
    # Fixed per-recording whitening, applied identically to data and dictionary.
    whitening=1/np.maximum(np.mean(spectrum,axis=0),np.max(np.mean(spectrum,axis=0))*.01)**.5
    dictionary*=whitening[:,None]
    norms=np.linalg.norm(dictionary,axis=0); dictionary/=norms
    activation=np.array([nnls(dictionary,row*whitening,maxiter=300)[0]/norms for row in spectrum])
    features=np.zeros((len(frames),12)); bass=np.zeros_like(features)
    for i,note in enumerate(notes):
        features[:,note%12]+=activation[:,i]
        bass[:,note%12]+=activation[:,i]*2**(-(note-40)/12)
    features=np.sqrt(features); bass=np.sqrt(bass)
    return unit(features),unit(bass),rms,tuning

def chord_predictions(features,bass,rms,use_bass):
    labels,t,b=templates(use_bass)
    emission=10*(features@t.T)
    if use_bass: emission+=3*(bass@b.T)
    emission[:,-1]-=3
    gate=rms<10**(-60/20); emission[gate,:]=-100; emission[gate,-1]=0
    return [labels[i] for i in decode(emission)]

def anchored_predictions(features,bass,states,extensions=True,inversions=True,penalty=0):
    # Preserve the existing triad recognizer's roots/thirds. Harmonic features
    # supply only information the 25-state model cannot express.
    labels,t,b=templates(inversions)
    emission=10*(features@t.T)
    if inversions: emission+=3*(bass@b.T)
    if penalty:
        for i,label in enumerate(labels):
            if label!='N' and label.split('/')[0].split(':')[1] not in ('maj','min'): emission[:,i]-=penalty
    allowed=[]
    for state in range(25):
        permitted=[]
        for i,label in enumerate(labels):
            if state==24:
                if label=='N': permitted.append(i)
                continue
            root=ROOTS[state%12]; minor=state>=12
            qualities=['min','min7','min6'] if minor else ['maj','7','maj7','maj6']
            if not extensions: qualities=qualities[:1]
            if label.split('/')[0] in [root+':'+q for q in qualities]: permitted.append(i)
        allowed.append(permitted)
    mask=np.full_like(emission,-1e6)
    for i,state in enumerate(states): mask[i,allowed[state]]=emission[i,allowed[state]]
    return [labels[i] for i in decode(mask)]

def refine_pitch(audio,rate,row,history):
    # Two overlapping Hann windows, both wholly in the past.
    f=row[2]; end=round(row[0]*rate); n=round(rate*.0465); hop=n//4
    if not row[3] or end<n+2*hop or len(history)<4 or max(history)-min(history)>15:
        return f,f,False
    window=.5-.5*np.cos(2*np.pi*np.arange(n)/n)
    t=np.arange(n)/rate
    left=audio[end-n-hop:end-hop]*window; right=audio[end-n:end]*window
    older=audio[end-n-2*hop:end-2*hop]*window
    estimates=[]; amps=[]
    for h in range(1,5):
        kernel=np.exp(-2j*np.pi*f*h*t)
        a=left@kernel; b=right@kernel; old=older@kernel
        offset=np.angle(b*np.conj(a)*np.exp(-2j*np.pi*f*h*hop/rate))
        previous=np.angle(a*np.conj(old)*np.exp(-2j*np.pi*f*h*hop/rate))
        # Extrapolate a locally linear frequency trajectory to the output time.
        offset+=(n/(2*hop)+.5)*(offset-previous)
        estimates.append(f+offset*rate/(2*np.pi*hop*h)); amps.append(min(abs(a),abs(b),abs(old)))
    cents=1200*np.log2(np.maximum(estimates,1e-20)/f)
    valid=(np.array(amps)>.2*max(amps))&(abs(cents)<15)
    if valid.sum()<2 or np.ptp(cents[valid])>5: return f,f,False
    refined=float(np.average(np.array(estimates)[valid],weights=np.array(amps)[valid]))
    fundamental=estimates[0] if valid[0] else refined
    return fundamental,refined,True

def run(root,mode):
    target=root/'recognition-improvement'; target.mkdir(exist_ok=True)
    freeze_pitch(root,target)
    tracks=[t for t in json.loads((root/'manifest.json').read_text())['tracks'] if t['mode']==mode and t['kind']=='guitar']
    if mode=='pitch':
        tracks += [dict(t,previous=True) for t in json.loads((root/'accuracy/holdout.json').read_text())['tracks'] if t['mode']=='pitch']
    results=[]
    for t in tracks:
        audio= root/t['audio']; refpath=root/t['reference']
        assert digest(audio)==t['audioSha256'] and digest(refpath)==t['referenceSha256']
        x,rate=sf.read(audio); ref=parse_guitar(refpath); metrics={}
        start=time.perf_counter()
        raw=json.loads((root/('accuracy/production' if t.get('previous') else 'accuracy/development')/(t['id']+'.json')).read_text())
        if mode=='chords':
            cache=target/(t['id']+'-features.npz')
            if not cache.exists():
                f,b,rms,tuning=harmonic_features(x,rate)
                np.savez(cache,features=f,bass=b,rms=rms,tuning=tuning)
            data=np.load(cache); f,b,rms=(data[k] for k in ('features','bass','rms'))
            baseline=chord_cells(dict(raw,config=dict(hop=2048),chords=raw['variants']['2500-10']),2)
            variants={'baseline':baseline}
            for name,use_bass in [('harmonic',False),('harmonic-bass',True)]:
                labels=chord_predictions(f,b,rms,use_bass)
                variants[name]=[[(i*2048+3072)/rate,(i*2048+5120)/rate,label] for i,label in enumerate(labels)]
            for name,extensions,inversions in [('anchored-quality',True,False),('anchored-bass',False,True),('anchored-both',True,True)]:
                labels=anchored_predictions(f,b,[row[2] for row in raw['variants']['2500-10']],extensions,inversions)
                variants[name]=[[(i*2048+3072)/rate,(i*2048+5120)/rate,label] for i,label in enumerate(labels)]
            for penalty in (.5,1.,2.):
                labels=anchored_predictions(unit(f*f),unit(b*b),[row[2] for row in raw['variants']['2500-10']],True,True,penalty)
                variants['sparse-'+str(penalty)]=[[(i*2048+3072)/rate,(i*2048+5120)/rate,label] for i,label in enumerate(labels)]
            for name,rows in variants.items():
                _,overlaps=score_chords(ref['performed'],rows)
                weights=np.array([b-a for a,b,*_ in overlaps]); labels=[r[2] for r in overlaps],[r[3] for r in overlaps]
                metrics[name]={}
                for rule in ('majmin','tetrads','tetrads_inv'):
                    values=getattr(chord,rule)(*labels); valid=values>=0
                    metrics[name][rule]=dict(correct=float(weights[valid]@values[valid]),eligible=float(weights[valid].sum()))
        else:
            history=[]; variants={k:[] for k in ('baseline','fundamental','harmonics')}; used=0
            for row in raw['variants']['2048-60']:
                if row[3]: history.append(1200*np.log2(row[2])); history=history[-4:]
                else: history=[]
                a,b,active=refine_pitch(x,rate,row,history); used+=active
                for name,hz in [('baseline',row[2]),('fundamental',a),('harmonics',b)]:
                    variants[name].append([*row[:2],hz,*row[3:]])
            for name,rows in variants.items():
                metrics[name]={kind:dict(eligible=0,errors=[],correct5=0,correct10=0,correct50=0) for kind in ('all','stable','attack')}
                for row in rows:
                    truth=reference_at(ref,row[0],max(0,row[0]-4096/rate))
                    if truth is None: continue
                    note=ref['notes'][truth[0]]; stable=row[0]-note[0]>=.25
                    err=error(row,truth[1])
                    for kind in ('all','stable' if stable else 'attack'):
                        m=metrics[name][kind]; m['eligible']+=1
                        if err is not None:
                            m['errors'].append(err)
                            for limit in (5,10,50): m['correct'+str(limit)]+=abs(err)<=limit
            metrics['refinedUpdates']=used
        results.append(dict(id=t['id'],metrics=metrics))
        save(target/(t['id']+'-'+mode+'.json'),dict(variants=variants,metrics=metrics))
        print(t['id'],round(time.perf_counter()-start,2),flush=True)
    summary={}
    names=tuple(variants) if mode=='chords' else ('baseline','fundamental','harmonics')
    for name in names:
        summary[name]={}
        for rule in (('majmin','tetrads','tetrads_inv') if mode=='chords' else ('all','stable','attack')):
            ms=[r['metrics'][name][rule] for r in results]
            if mode=='chords':
                correct=sum(m['correct'] for m in ms); eligible=sum(m['eligible'] for m in ms)
                summary[name][rule]=dict(correct=correct,eligible=eligible,accuracy=correct/eligible)
            else:
                errors=[e for m in ms for e in m['errors']]; eligible=sum(m['eligible'] for m in ms)
                summary[name][rule]=dict(eligible=eligible,voiced=len(errors),median=float(np.median(np.abs(errors))),
                    **{'accuracy'+str(c):sum(m['correct'+str(c)] for m in ms)/eligible for c in (5,10,50)})
    save(target/(mode+'-development.json'),dict(summary=summary,tracks=results))
    print(json.dumps(summary,indent=2),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('mode',choices=['chords','pitch'])
    p.add_argument('--root',type=Path,default=Path('build_real_audio')); a=p.parse_args()
    run(a.root.resolve(),a.mode)
