"""Blind causal CREPE comparison. Models run locally through ONNX Runtime."""
import json
import math
from pathlib import Path
import time
import numpy as np
import onnxruntime as ort
from scipy.signal import resample_poly
import soundfile as sf
from evaluate import parse_guitar, save
from compare_tuners import reference_at, error
from prepare import digest

root=Path(__file__).resolve().parents[2]/'build_real_audio'
target=root/'community'
tracks=[t for t in json.loads((root/'accuracy/holdout.json').read_text())['tracks'] if t['mode']=='pitch']
# These fixed settings precede any predictions. No fitting or model training.
config=dict(decoder='weighted argmax over +/-4 bins',confidence=.5,gateDb=-55,
            frameSamples=1024,modelRate=16000,displayHz=15,
            resampler='scipy.resample_poly independently on each past-only window; zero boundary padding')
sessions={}
for model in ('tiny','full'):
    options=ort.SessionOptions(); options.intra_op_num_threads=2
    sessions[model]=ort.InferenceSession(str(target/(model+'.onnx')),sess_options=options,providers=['CPUExecutionProvider'])
bins=np.arange(360)
cents=20*bins+1997.3794084376191
allowed=(bins>=math.floor((1200*math.log2(50/10)-1997.3794084376191)/20)) & (bins<math.ceil((1200*math.log2(1500/10)-1997.3794084376191)/20))

def decode(p):
    p=np.maximum(p,0).copy(); p[:,~allowed]=0
    k=p.argmax(axis=1)
    weights=np.where(abs(bins[None,:]-k[:,None])<=4,p,0)
    hz=10*2**((weights@cents/np.maximum(weights.sum(axis=1),1e-12))/1200)
    return hz,p[np.arange(len(p)),k]

checks=[]
for model,s in sessions.items():
    signals=np.array([.3*np.sin(2*np.pi*hz*np.arange(1024)/16000) for hz in (110,220,440,880)],dtype=np.float32)
    hz,conf=decode(s.run(None,{'frames':signals})[0])
    if np.any(abs(1200*np.log2(hz/np.array([110,220,440,880])))>50): raise ValueError('Model contract check failed')
    checks.append(dict(model=model,hz=hz.tolist(),confidence=conf.tolist()))
results=[]
for t in tracks:
    audio=root/t['audio']; refpath=root/t['reference']
    if digest(audio)!=t['audioSha256'] or digest(refpath)!=t['referenceSha256']: raise ValueError('Changed data')
    x,rate=sf.read(audio)
    baseline=json.loads((root/'accuracy/production'/(t['id']+'.json')).read_text())['variants']['2048-60']
    ends=np.array([round(r[0]*rate) for r in baseline])
    count=round(1024*rate/16000)
    inputs=[]; levels=[]
    for end in ends:
        frame=x[max(0,end-count):end]
        if len(frame)<count: frame=np.pad(frame,(count-len(frame),0))
        levels.append(10*np.log10(max(float(np.mean(frame**2)),1e-12)))
        g=math.gcd(rate,16000)
        resampled=resample_poly(frame,16000//g,rate//g)
        if len(resampled)!=1024: raise ValueError('Unexpected resampled length')
        inputs.append(resampled)
    inputs=np.asarray(inputs,dtype=np.float32)
    ref=parse_guitar(refpath)
    raw={'desktop':baseline}; times={}
    for model,s in sessions.items():
        start=time.perf_counter()
        p=np.concatenate([s.run(None,{'frames':inputs[i:i+64]})[0] for i in range(0,len(inputs),64)])
        hz,conf=decode(p)
        times[model]=time.perf_counter()-start
        raw[model]=[[r[0],max(0,(int(end)-count)/rate),float(f),bool(c>=.5 and db>-55),float(c),float(db),0]
                    for r,end,f,c,db in zip(baseline,ends,hz,conf,levels)]
    metrics={}
    for model,rows in raw.items():
        errors=[]
        for row in rows:
            truth=reference_at(ref,row[0],max(0,row[0]-4096/rate))
            if truth: errors.append(error(row,truth[1]))
        voiced=[e for e in errors if e is not None]
        metrics[model]=dict(eligible=len(errors),voiced=len(voiced),errors=errors,
            correct5=sum(abs(e)<=5 for e in voiced),correct10=sum(abs(e)<=10 for e in voiced),correct50=sum(abs(e)<=50 for e in voiced))
    save(target/(t['id']+'-pitch.json'),dict(config=config,raw=raw,metrics=metrics,inferenceSeconds=times))
    results.append(dict(id=t['id'],metrics=metrics,inferenceSeconds=times))
    print(t['id'],flush=True)
summary={}
for model in ('desktop','tiny','full'):
    ms=[t['metrics'][model] for t in results]
    total={k:sum(m[k] for m in ms) for k in ('eligible','voiced','correct5','correct10','correct50')}
    for c in (5,10,50): total[f'accuracy{c}']=total[f'correct{c}']/total['eligible']
    total['medianAbsoluteCentsWhenVoiced']=float(np.median([abs(e) for m in ms for e in m['errors'] if e is not None]))
    summary[model]=total
save(target/'pitch-report.json',dict(config=config,modelHashes={m:digest(target/(m+'.onnx')) for m in sessions},checks=checks,summary=summary,tracks=results))
print(json.dumps(summary,indent=2),flush=True)
