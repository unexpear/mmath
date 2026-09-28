"""Native/oracle agreement, then one frozen evaluation of reserved recordings."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import numpy as np
import mir_eval.chord as chord
from evaluate import parse_guitar, score_chords, save, chord_cells
from compare_tuners import reference_at, error
from prepare import digest

def pitch_metrics(rows,ref,rate):
    groups={k:dict(eligible=0,errors=[],correct5=0,correct10=0,correct50=0) for k in ('all','sustain','early','transition')}
    for row in rows:
        truth=reference_at(ref,row[0],max(0,row[0]-4096/rate))
        if truth:
            age=row[0]-ref['notes'][truth[0]][0]
            kinds=('all','sustain' if age>=.25 else 'early')
        else:
            truth=reference_at(ref,row[0],max(0,row[0]-1e-6))
            if not truth: continue
            kinds=('transition',)
        err=error(row,truth[1])
        for kind in kinds:
            m=groups[kind]; m['eligible']+=1
            if err is not None:
                m['errors'].append(err)
                for c in (5,10,50): m['correct'+str(c)]+=int(abs(err)<=c)
    return groups

def chord_metrics(rows,ref):
    _,overlaps=score_chords(ref['performed'],rows)
    weights=np.array([b-a for a,b,*_ in overlaps]); labels=[r[2] for r in overlaps],[r[3] for r in overlaps]
    result={}
    for rule in ('majmin','tetrads','tetrads_inv'):
        values=getattr(chord,rule)(*labels); valid=values>=0
        result[rule]=dict(correct=float(weights[valid]@values[valid]),eligible=float(weights[valid].sum()),
                          annotated=sum(b-a for a,b,_ in ref['performed']))
    return result

def run(root,probe,heldout):
    target=root/'recognition-improvement'; selection=target/'selection.json'
    if not heldout:
        tracks=[t for t in json.loads((root/'manifest.json').read_text())['tracks'] if t['kind']=='guitar']
        tracks += [dict(t,previous=True) for t in json.loads((root/'accuracy/holdout.json').read_text())['tracks'] if t['mode']=='pitch']
    else:
        chosen=json.loads(selection.read_text())
        if chosen['probeSha256']!=digest(probe): raise ValueError('Probe changed after selection')
        tracks=[dict(t,mode='chords') for t in json.loads((root/'math-improvement/holdout.json').read_text())['tracks']]
        tracks += json.loads((target/'pitch-holdout.json').read_text())['tracks']
        if (target/'native-holdout-report.json').exists(): raise ValueError('Holdout already scored; do not retune and repeat')
    folder=target/('native-holdout' if heldout else 'native-development'); folder.mkdir(exist_ok=True)
    def one(t):
        audio=root/t['audio']; reference=root/t['reference']
        assert digest(audio)==t['audioSha256'] and digest(reference)==t['referenceSha256']
        path=folder/(t['id']+'.json')
        subprocess.run([str(probe),str(audio),str(path),t['mode']],check=True,timeout=180)
        raw=json.loads(path.read_text()); ref=parse_guitar(reference)
        agreement=None
        if t['mode']=='pitch':
            improved=raw['pitch']; baseline=[[*r[:2],r[7],*r[3:]] for r in improved]
            metrics={name:pitch_metrics(rows,ref,raw['sampleRate']) for name,rows in [('baseline',baseline),('improved',improved)]}
            cpu=[r[6] for r in improved]
            extra=dict(processingMs=raw['processingMs'],cpuMedianMs=float(np.median(cpu)),cpu95Ms=float(np.percentile(cpu,95)),refined=sum(r[8] for r in improved))
            if not heldout:
                oracle=json.loads((target/(t['id']+'-pitch.json')).read_text())['variants']
                assert len(oracle['harmonics'])==len(improved)
                delta=max(abs(a[2]-b[2]) for a,b in zip(oracle['harmonics'],improved))
                assert delta<1e-6, (t['id'],delta)
                assert all(a[3]==b[3] for a,b in zip(oracle['harmonics'],improved))
                agreement=delta
        else:
            improved=raw['chords']
            # Original baseline executable is frozen, audio-only, and unchanged.
            basepath=folder/(t['id']+'-baseline.json')
            subprocess.run([str(root/'cpp/Release/accuracy_probe.exe'),str(audio),str(basepath),'chords','2500-10','production'],check=True,timeout=180)
            original=json.loads(basepath.read_text())
            baseline=chord_cells(dict(original,config=dict(hop=2048),chords=original['variants']['2500-10']),2)
            metrics={name:chord_metrics(rows,ref) for name,rows in [('baseline',baseline),('improved',improved)]}
            extra=dict(processingMs=raw['processingMs'])
            if not heldout:
                oracle=json.loads((target/(t['id']+'-chords.json')).read_text())['variants']['sparse-0.5']
                assert len(oracle)==len(improved)
                differences=sum(a[2]!=b[2] for a,b in zip(oracle,improved))
                assert differences==0, (t['id'],differences)
                agreement=differences
        print(t['id'],flush=True)
        return dict(id=t['id'],mode=t['mode'],metrics=metrics,oracleDelta=agreement,**extra)
    with ThreadPoolExecutor(max_workers=3) as pool: results=list(pool.map(one,tracks))
    summary={}
    for mode in ('chords','pitch'):
        subset=[r for r in results if r['mode']==mode]; summary[mode]={}
        for name in ('baseline','improved'):
            summary[mode][name]={}
            for group in subset[0]['metrics'][name]:
                ms=[r['metrics'][name][group] for r in subset]; eligible=sum(m['eligible'] for m in ms)
                if mode=='chords':
                    correct=sum(m['correct'] for m in ms)
                    summary[mode][name][group]=dict(accuracy=correct/eligible,correct=correct,eligible=eligible,annotated=sum(m['annotated'] for m in ms))
                else:
                    errors=[abs(e) for m in ms for e in m['errors']]
                    summary[mode][name][group]=dict(eligible=eligible,voiced=len(errors),median=float(np.median(errors)) if errors else None,
                        p90=float(np.percentile(errors,90)) if errors else None,
                        **{'accuracy'+str(c):sum(m['correct'+str(c)] for m in ms)/eligible if eligible else None for c in (5,10,50)})
    save(target/('native-holdout-report.json' if heldout else 'native-development-report.json'),dict(probeSha256=digest(probe),summary=summary,tracks=results))
    if not heldout:
        c=summary['chords']; p=summary['pitch']
        assert c['improved']['tetrads_inv']['accuracy']>c['baseline']['tetrads_inv']['accuracy']
        assert c['improved']['majmin']['accuracy']+1e-10>=c['baseline']['majmin']['accuracy']
        assert p['improved']['all']['accuracy5']>p['baseline']['all']['accuracy5']
        assert p['improved']['sustain']['median']<p['baseline']['sustain']['median']
        assert p['improved']['all']['accuracy50']>=p['baseline']['all']['accuracy50']-.005
        if not selection.exists(): save(selection,dict(probeSha256=digest(probe),prototypeSha256=digest(Path(__file__).with_name('recognition_experiment.py')),
            chords='anchored harmonic notes, extensions penalty 0.5, bass weight 3',pitch='short-window harmonic phase with endpoint extrapolation',
            acceptance='Chord quality+bass must improve with no major/minor loss; pitch within-5c must improve and sustain median error fall, with at most 0.5 percentage point loss within-50c. No settings change after this freeze.'))
    print(json.dumps(summary,indent=2),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('--root',type=Path,default=Path('build_real_audio'))
    p.add_argument('--probe',type=Path,default=Path('build_app_msvc/verified/recognition_probe.exe'))
    p.add_argument('--heldout',action='store_true'); a=p.parse_args()
    run(a.root.resolve(),a.probe.resolve(),a.heldout)
