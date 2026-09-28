"""Independent scoring, native/oracle agreement, and a once-only reserved test."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import numpy as np
import mir_eval.chord as chord
import soundfile as sf
from native_chord_model import development, load_track, predict, summary, features
from verify_recognition import chord_metrics
from evaluate import save
from prepare import digest

def run(root,probe,modelpath,heldout,study='native-chords'):
    target=root/study; model=json.loads(modelpath.read_text());selection=target/'selection.json'
    report=target/('reserved-report.json' if heldout else 'native-development.json')
    if heldout:
        frozen=json.loads(selection.read_text())
        if report.exists():raise ValueError('Reserved results already exist; do not retune and reuse')
        assert frozen['modelSha256']==digest(modelpath) and frozen['probeSha256']==digest(probe)
        assert frozen['reservedManifestSha256']==digest(target/'reserved.json')
        assert frozen['publicDomainManifestSha256']==digest(root/'public-domain-chords/manifest.json')
        tracks=json.loads((target/'reserved.json').read_text())['tracks']
    else:tracks=json.loads((target/'development-tracks.json').read_text())['tracks'] if (target/'development-tracks.json').exists() else development(root)
    folder=target/('native-reserved' if heldout else 'native-development');folder.mkdir(exist_ok=True)
    def one(t):
        data=load_track(root,target,t);path=folder/(t['id']+'.json')
        subprocess.run([str(probe),str(root/t['audio']),str(path)],check=True,timeout=120)
        raw=json.loads(path.read_text());rows=raw['chords'];oracle=predict(data,model,model['switch'])
        assert len(rows)==len(oracle)
        assert all(a[:3]==b for a,b in zip(rows,oracle)),t['id']
        # The native label's reduced state must agree with the independent evaluator.
        labels=[a[2] for a in rows]
        simple=[r+':'+q for q in ('maj','min') for r in ('C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B')]+['N']
        for a in rows:
            hits=np.flatnonzero(chord.majmin([a[2]]*25,simple)==1)
            expected=int(hits[0]) if len(hits) else -1
            assert expected==a[4],(a,expected)
        baseline=[[a[0],a[1],a[3]] for a in rows]
        result=dict(id=t['id'],baseline=chord_metrics(baseline,data['ref']),improved=chord_metrics(oracle,data['ref']),
                    oracleMatch=True,processingMs=raw['processingMs'])
        print(t['id'],flush=True);return result
    with ThreadPoolExecutor(max_workers=3) as pool:results=list(pool.map(one,tracks))
    scores={k:summary([r[k] for r in results]) for k in ('baseline','improved')}
    external=[]
    if heldout:
        source=root/'public-domain-chords';manifest=json.loads((source/'manifest.json').read_text())
        for t in manifest['tracks']:
            assert digest(source/t['audio'])==t['audioSha256']
            path=folder/(t['id']+'.json')
            subprocess.run([str(probe),str(source/t['audio']),str(path)],check=True,timeout=120)
            raw=json.loads(path.read_text());rows=raw['chords']
            audio,rate=sf.read(source/t['audio']);x,gate=features(audio,rate)
            data=dict(x=x,gate=gate,rate=rate,samples=len(audio))
            assert all(a[:3]==b for a,b in zip(rows,predict(data,model,model['switch'])))
            values={}
            for name,index in [('baseline',3),('improved',2)]:
                labels=[a[index] for a in rows];unique=sorted(set(labels))
                majority=max(unique,key=lambda label:labels.count(label))
                values[name]=dict(majority=majority,clipQualityCorrect=bool(chord.tetrads([t['expected']],[majority])[0]==1),
                    frameQualityAgreement=float(np.mean(chord.tetrads([t['expected']]*len(labels),labels))),
                    frameMajMinAgreement=float(np.mean(chord.majmin([t['expected']]*len(labels),labels))),
                    predictedNoChordFrames=labels.count('N'),frames=len(labels))
            external.append(dict(id=t['id'],expected=t['expected'],referenceStrength=t['referenceStrength'],**values))
    passed=(scores['improved']['majmin']+1e-10>=scores['baseline']['majmin'] and
            scores['improved']['tetrads']>scores['baseline']['tetrads'] and
            scores['improved']['tetrads_inv']>scores['baseline']['tetrads_inv'])
    if external:
        old=sum(t['baseline']['clipQualityCorrect'] for t in external);new=sum(t['improved']['clipQualityCorrect'] for t in external)
        passed=passed and new>=old
    save(report,dict(modelSha256=digest(modelpath),probeSha256=digest(probe),summary=scores,tracks=results,
                     publicDomain=external,accepted=passed))
    if not heldout:
        if not passed:raise ValueError('Development regression')
        if selection.exists():raise ValueError('Selection already frozen')
        save(selection,dict(modelSha256=digest(modelpath),probeSha256=digest(probe),
            headerSha256=digest(Path('app/guitar_chord_model.h')),
            criterion='Full quality and quality+bass duration scores must improve on 30 fresh recordings, without major/minor loss; public-domain clip-level exact quality correctness must not fall. No model changes after freeze.',
            trainingTracks=development(root),reservedManifestSha256=digest(target/'reserved.json'),
            publicDomainManifestSha256=digest(root/'public-domain-chords/manifest.json')))
    print(json.dumps(dict(summary=scores,publicDomain=external,accepted=passed),indent=2),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path('build_real_audio'))
    p.add_argument('--probe',type=Path,default=Path('build_math_gcc/native_chord_probe.exe'))
    p.add_argument('--model',type=Path,default=Path('build_real_audio/native-chords/model-logistic.json'))
    p.add_argument('--study',default='native-chords');p.add_argument('--heldout',action='store_true');a=p.parse_args();run(a.root.resolve(),a.probe.resolve(),a.model.resolve(),a.heldout,a.study)
