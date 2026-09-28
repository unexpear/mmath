"""Evaluate the previously frozen Chordino configuration on new recordings.

No parameter search: plugin/host/transform hashes must equal the previous study.
"""
from concurrent.futures import ThreadPoolExecutor
import csv
import io
import json
import os
from pathlib import Path
import subprocess
from evaluate import parse_guitar, chord_cells, save
from verify_recognition import chord_metrics
from prepare import digest

root=Path(__file__).resolve().parents[2]/'build_real_audio'
community=root/'community'; target=root/'recognition-improvement'
provenance=json.loads((community/'chord-report.json').read_text())
exe=community/'sonic/sonic-annotator-win64/sonic-annotator.exe'
transform=community/'chordino-harte.ttl'
for path,key in [(exe,'hostSha256'),(community/'chordino/nnls-chroma.dll','pluginSha256'),(transform,'transformSha256')]:
    if digest(path)!=provenance[key]: raise ValueError('Previously frozen Chordino configuration changed')
report=target/'chordino-reserved-report.json'
if report.exists(): raise ValueError('Reserved comparison already recorded; do not tune and repeat')
tracks=json.loads((root/'math-improvement/holdout.json').read_text())['tracks']
def one(t):
    audio,refpath=root/t['audio'],root/t['reference']
    assert digest(audio)==t['audioSha256'] and digest(refpath)==t['referenceSha256']
    p=subprocess.run([str(exe),'-q','-t',str(transform),'-w','csv','--csv-stdout','--csv-omit-filename','--csv-digits','12',str(audio)],
        env=dict(os.environ,VAMP_PATH=str(community/'chordino')),capture_output=True,text=True,check=True,timeout=60)
    changes=[[float(r[0]),r[1]] for r in csv.reader(io.StringIO(p.stdout)) if r]
    raw=json.loads((target/'native-holdout'/(t['id']+'-baseline.json')).read_text())
    baseline=chord_cells(dict(raw,config=dict(hop=2048),chords=raw['variants']['2500-10']),2)
    lo,hi=baseline[0][0],baseline[-1][1]
    rows=[]
    for i,(a,label) in enumerate(changes):
        b=changes[i+1][0] if i+1<len(changes) else raw['samples']/raw['sampleRate']
        if max(a,lo)<min(b,hi): rows.append([max(a,lo),min(b,hi),label])
    ref=parse_guitar(refpath)
    metrics={name:chord_metrics(v,ref) for name,v in [('baseline',baseline),('chordino',rows)]}
    save(target/'native-holdout'/(t['id']+'-chordino.json'),dict(estimates=rows,metrics=metrics))
    print(t['id'],flush=True)
    return dict(id=t['id'],metrics=metrics)
with ThreadPoolExecutor(max_workers=3) as pool: results=list(pool.map(one,tracks))
summary={}
for name in ('baseline','chordino'):
    summary[name]={}
    for rule in ('majmin','tetrads','tetrads_inv'):
        ms=[r['metrics'][name][rule] for r in results]
        values={k:sum(m[k] for m in ms) for k in ('correct','eligible','annotated')}
        values['accuracy']=values['correct']/values['eligible']; summary[name][rule]=values
save(report,dict(summary=summary,tracks=results,provenance={k:provenance[k] for k in ('hostSha256','pluginSha256','transformSha256')}))
print(json.dumps(summary,indent=2),flush=True)
