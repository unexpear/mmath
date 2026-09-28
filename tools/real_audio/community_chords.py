"""Compare Chordino with the desktop on identical annotated time coverage."""
from concurrent.futures import ThreadPoolExecutor
import csv
import io
import json
import os
from pathlib import Path
import subprocess
import time
from evaluate import parse_guitar, score_chords, chord_cells, save
from prepare import digest

root=Path(__file__).resolve().parents[2]/'build_real_audio'
target=root/'community'
exe=target/'sonic/sonic-annotator-win64/sonic-annotator.exe'
env=dict(os.environ,VAMP_PATH=str(target/'chordino'))
transform=target/'chordino-harte.ttl'
# Only the label syntax differs from the plugin defaults; freeze all settings.
if digest(transform) != digest(Path(__file__).with_name('chordino-harte.ttl')):
    raise ValueError('Changed Chordino settings; run setup_community.py')
tracks=[t for t in json.loads((root/'accuracy/holdout.json').read_text())['tracks'] if t['mode']=='chords']

def run(t):
    audio=root/t['audio']; refpath=root/t['reference']
    if digest(audio)!=t['audioSha256'] or digest(refpath)!=t['referenceSha256']: raise ValueError('Changed data')
    start=time.perf_counter()
    p=subprocess.run([str(exe),'-t',str(transform),'-w','csv','--csv-stdout','--csv-omit-filename','--csv-digits','12',str(audio)],env=env,capture_output=True,text=True,check=True,timeout=60)
    elapsed=time.perf_counter()-start
    (target/(t['id']+'-chords.csv')).write_text(p.stdout)
    (target/(t['id']+'-chords.log')).write_text(p.stderr)
    changes=[[float(r[0]),r[1]] for r in csv.reader(io.StringIO(p.stdout)) if r]
    raw=json.loads((root/'accuracy/production'/(t['id']+'.json')).read_text())
    desktop=chord_cells(dict(raw,config=dict(hop=2048),chords=raw['variants']['2500-10']),2)
    lo,hi=desktop[0][0],desktop[-1][1]
    estimate=[]
    for i,(start,label) in enumerate(changes):
        end=changes[i+1][0] if i+1<len(changes) else raw['samples']/raw['sampleRate']
        if max(lo,start)<min(hi,end): estimate.append([max(lo,start),min(hi,end),label])
    ref=parse_guitar(refpath)['performed']
    metrics={name:score_chords(ref,rows)[0] for name,rows in [('desktop',desktop),('chordino',estimate)]}
    if abs(metrics['desktop']['scoredSeconds']-metrics['chordino']['scoredSeconds'])>1e-6: raise ValueError('Unequal coverage')
    save(target/(t['id']+'-chords.json'),dict(changes=changes,estimates=estimate,metrics=metrics,processingSeconds=elapsed))
    print(t['id'],flush=True)
    return dict(id=t['id'],metrics=metrics,processingSeconds=elapsed)

with ThreadPoolExecutor(max_workers=3) as pool: results=list(pool.map(run,tracks))
summary={}
for model in ('desktop','chordino'):
    totals={k:sum(t['metrics'][model][k] for t in results) for k in ('correctSeconds','scoredSeconds','annotatedSeconds','unsupportedSeconds','uncoveredSeconds')}
    totals['accuracy']=totals['correctSeconds']/totals['scoredSeconds']
    summary[model]=totals
save(target/'chord-report.json',dict(hostSha256=digest(exe),pluginSha256=digest(target/'chordino/nnls-chroma.dll'),transformSha256=digest(transform),summary=summary,tracks=results))
print(json.dumps(summary,indent=2),flush=True)
