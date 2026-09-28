"""Train a small transposition-shared guitar model; no Chordino predictions.

Previously consumed sets are development data. A new set is frozen before
predictions, and external public-domain recordings are never used for training.
"""
import argparse
import json
from pathlib import Path
import zipfile
import numpy as np
import soundfile as sf
import mir_eval.chord as chord
from scipy.optimize import minimize
from scipy.special import logsumexp
from evaluate import parse_guitar, save
from prepare import digest, KNOWN_ISSUES
from recognition_experiment import decode, ROOTS, DEGREES
from verify_recognition import chord_metrics

QUALITIES = [('maj',[0,4,7]),('min',[0,3,7]),('7',[0,4,7,10]),
             ('maj7',[0,4,7,11]),('min7',[0,3,7,10]),('maj6',[0,4,7,9]),
             ('min6',[0,3,7,9]),('hdim7',[0,3,6,10]),('dim',[0,3,6]),
             ('dim7',[0,3,6,9]),('aug',[0,4,8]),('sus2',[0,2,7]),('sus4',[0,5,7])]
TYPES=[(q,b) for q,pcs in QUALITIES for b in pcs]
LABELS=[r+':'+q+('/'+DEGREES[b] if b else '') for r in ROOTS for q,b in TYPES]

def development(root):
    return [t for name in ('manifest.json','accuracy/holdout.json','math-improvement/holdout.json')
            for t in json.loads((root/name).read_text())['tracks'] if t.get('mode','chords')=='chords']

def freeze(root,target):
    path=target/'reserved.json'
    if path.exists(): return json.loads(path.read_text())['tracks']
    used={t['id'] for t in development(root)}; tracks=[]
    with zipfile.ZipFile(root/'annotation.zip') as az, zipfile.ZipFile(root/'audio_mono-mic.zip') as wz:
        names=sorted(Path(n).stem for n in az.namelist() if n.endswith('.jams') and '/' not in n)
        wavs={Path(n).name:n for n in wz.namelist() if not n.startswith('__MACOSX')}
        for player in range(6):
            for style in ('BN','Funk','Jazz','Rock','SS'):
                name=next(n for n in names if n.startswith(f'{player:02d}_{style}') and n.endswith('_comp')
                          and n not in used and n not in KNOWN_ISSUES)
                audio,ref=target/(name+'.wav'),target/(name+'.jams')
                audio.write_bytes(wz.read(wavs[name+'_mic.wav']));ref.write_bytes(az.read(name+'.jams'))
                tracks.append(dict(id=name,audio=audio.relative_to(root).as_posix(),reference=ref.relative_to(root).as_posix(),
                    audioSha256=digest(audio),referenceSha256=digest(ref)))
    save(path,dict(selection='First unused comp per player/style after all three previous chord sets. Same players/progressions, new recordings.',tracks=tracks))
    return tracks

def features(audio,rate):
    n=8192;hop=2048
    if len(audio)<n: audio=np.pad(audio,(0,n-len(audio)))
    frames=np.lib.stride_tricks.sliding_window_view(audio,n)[::hop]
    win=.5-.5*np.cos(2*np.pi*np.arange(n)/n)
    power=abs(np.fft.rfft(frames*win,axis=1))**2/(n*np.sum(win*win)/4)
    hz=np.arange(power.shape[1])*rate/n
    pc=np.floor(69+12*np.log2(np.maximum(hz,1)/440)+.5).astype(int)%12
    outputs=[]
    for lo,hi in [(80,2500),(80,300),(300,1000),(1000,2500)]:
        c=np.stack([power[:,(hz>=lo)&(hz<=hi)&(pc==k)].sum(axis=1) for k in range(12)],axis=1)
        c=np.log1p(10*c/np.maximum(c.max(axis=1,keepdims=True),1e-20))
        outputs.append(c/np.maximum(np.linalg.norm(c,axis=1,keepdims=True),1e-20))
    gate=power[:,(hz>=80)&(hz<=2500)].sum(axis=1)<1e-6
    return np.concatenate(outputs,axis=1),gate

def rotations(x):
    return np.stack([np.roll(x.reshape(-1,4,12),-r,axis=2).reshape(-1,48) for r in range(12)],axis=1)

def targets(ref,times):
    out=np.full(len(times),-1)
    canonical={}
    for i,label in enumerate(LABELS):
        r,bits,b=chord.encode(label,reduce_extended_chords=True);canonical[(r,tuple(bits),b)]=i
    for a,b,label in ref['performed']:
        if label in ('N','X'):continue
        r,bits,bass=chord.encode(label,reduce_extended_chords=True)
        key=(r,tuple(bits),bass)
        if key in canonical:
            # Only stable interiors train the acoustic model. Scoring includes boundaries.
            out[(times>=a+.1)&(times<b-.1)]=canonical[key]
    return out

def load_track(root,target,t):
    audio,refpath=root/t['audio'],root/t['reference']
    assert digest(audio)==t['audioSha256'] and digest(refpath)==t['referenceSha256']
    cache=target/(t['id']+'.npz')
    if cache.exists():
        z=np.load(cache); x,gate,rate,samples=z['x'],z['gate'],int(z['rate']),int(z['samples'])
    else:
        audio,rate=sf.read(audio);samples=len(audio);x,gate=features(audio,rate)
        np.savez(cache,x=x,gate=gate,rate=rate,samples=samples)
    ref=parse_guitar(refpath); times=(np.arange(len(x))*2048+4096)/rate
    return dict(id=t['id'],x=x,gate=gate,rate=rate,samples=samples,ref=ref,y=targets(ref,times))

def fit(tracks,regularization):
    xs=[];ys=[]
    for t in tracks:
        ids=np.flatnonzero((t['y']>=0)&~t['gate'])[::3]
        y=t['y'][ids];x=rotations(t['x'][ids])[np.arange(len(ids)),y//len(TYPES)]
        xs.append(x);ys.append(y%len(TYPES))
    x=np.concatenate(xs);y=np.concatenate(ys)
    # Root-relative Gaussian templates with one shared, regularized covariance.
    # Shrink poorly represented inversions towards their root-position template.
    global_mean=x.mean(axis=0);means=[];counts=[]
    for i,(q,b) in enumerate(TYPES):
        values=x[y==i];base=x[y==TYPES.index((q,0))]
        prior=base.mean(axis=0) if len(base) else global_mean
        means.append((values.sum(axis=0)+20*prior)/(len(values)+20));counts.append(len(values))
    means=np.array(means);residual=x-means[y]
    covariance=residual.T@residual/len(x)+regularization*np.eye(x.shape[1])
    precision=np.linalg.inv(covariance)
    return dict(means=means.tolist(),precision=precision.tolist(),counts=counts,
                prior=np.log((np.array(counts)+1)/(len(x)+len(counts))).tolist())

def baseline_states(t,stay=.9):
    c=t['x'][:,:12]
    templates=np.zeros((25,12))
    for i in range(24):templates[i,[i%12,(i%12+(3 if i>=12 else 4))%12,(i%12+7)%12]]=1/np.sqrt(3)
    templates[24]=1/np.sqrt(12)
    emission=10*(c@templates.T);emission[:,24]-=2
    emission[t['gate'],:]=0;emission[t['gate'],24]=10
    return np.array(decode(emission,np.log(stay/((1-stay)/24))))

def predict(t,model,switch):
    x=rotations(t['x'])
    if 'weights' in model:
        w=np.array(model['weights']);emission=x@w[:,:48].T+w[:,48]
    else:
        mu=np.array(model['means']);p=np.array(model['precision'])
        # Gaussian quadratic forms; constants common to all states cancel.
        emission=x@(p@mu.T)-.5*np.sum((mu@p)*mu,axis=1)+np.array(model['prior'])
        emission-=.5*np.einsum('nri,ij,nrj->nr',x,p,x)[:,:,None]
    emission=emission.reshape(len(x),-1)
    emission=np.column_stack([emission,np.full(len(x),-1e6)])
    emission[t['gate'],:]=-1e6;emission[t['gate'],-1]=0
    if model.get('anchored'):
        states=baseline_states(t,model.get('baseStay',.9))
        triads=[]
        for root in range(12):
            for q,b in TYPES:
                triads.append(root if q in ('maj','7','maj7','maj6') else root+12 if q in ('min','min7','min6') else -1)
        triads.append(24)
        emission[np.array(triads)[None,:]!=states[:,None]]=-1e6
        for i,label in enumerate(LABELS):
            if label.split(':')[1] not in ('maj','min'):emission[:,i]-=model.get('detailPenalty',0)
        emission[states==24,-1]=0
    labels=LABELS+['N'];states=decode(emission,switch)
    return [[(i*2048+3072)/t['rate'],min(t['samples']/t['rate'],(i*2048+5120)/t['rate']),labels[s]] for i,s in enumerate(states)]

def summary(results):
    return {rule:sum(r[rule]['correct'] for r in results)/sum(r[rule]['eligible'] for r in results)
            for rule in ('majmin','tetrads','tetrads_inv')}

def fit_logistic(tracks,regularization):
    xs=[];ys=[]
    for t in tracks:
        ids=np.flatnonzero((t['y']>=0)&~t['gate'])[::6]
        xs.append(t['x'][ids]);ys.append(t['y'][ids])
    x=rotations(np.concatenate(xs));y=np.concatenate(ys);n=len(y)
    x=np.concatenate([x,np.ones((*x.shape[:2],1))],axis=2).reshape(-1,49)
    def loss(flat):
        w=flat.reshape(len(TYPES),49)
        logits=(x@w.T).reshape(n,-1)
        z=logsumexp(logits,axis=1)
        loss=float(np.mean(z-logits[np.arange(n),y])+.5*regularization*(np.sum(w[:,:48]**2)+.1*np.sum(w[:,48]**2)))
        prob=np.exp(logits-z[:,None]);prob[np.arange(n),y]-=1
        grad=prob.reshape(-1,len(TYPES)).T@x/n
        grad[:,:48]+=regularization*w[:,:48]
        grad[:,48]+=.1*regularization*w[:,48]
        return loss,grad.ravel()
    result=minimize(loss,np.zeros(len(TYPES)*49),jac=True,method='L-BFGS-B',options=dict(maxiter=500,ftol=1e-7,gtol=1e-4))
    if not result.success:raise ValueError(result.message)
    return dict(weights=result.x.reshape(len(TYPES),49).tolist(),fitLoss=result.fun,iterations=result.nit)

def run(root,logistic=False):
    target=root/'native-chords';target.mkdir(exist_ok=True);freeze(root,target)
    tracks=[load_track(root,target,t) for t in development(root)]
    candidates=[]
    for reg in ((.001,.01) if logistic else (.003,.01,.03)):
        predictions={s:[] for s in (3,8,16)}
        for player in range(6):
            model=(fit_logistic if logistic else fit)([t for t in tracks if int(t['id'][:2])!=player],reg)
            print('fold',reg,player,flush=True)
            for t in tracks:
                if int(t['id'][:2])!=player:continue
                for s in predictions:predictions[s].append(chord_metrics(predict(t,model,s),t['ref']))
        for switch,results in predictions.items():
            scores=summary(results);candidate=dict(regularization=reg,switch=switch,scores=scores)
            candidates.append(candidate);print(json.dumps(candidate),flush=True)
    best=max(candidates,key=lambda c:c['scores']['tetrads_inv'])
    model=(fit_logistic if logistic else fit)(tracks,best['regularization']);model.update(best)
    suffix='-logistic' if logistic else ''
    save(target/('development'+suffix+'.json'),dict(protocol='Leave one of six players out of fitting on 90 previously consumed recordings',candidates=candidates,selected=best))
    save(target/('model'+suffix+'.json'),model)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path('build_real_audio'));p.add_argument('--logistic',action='store_true');a=p.parse_args();run(a.root,a.logistic)
