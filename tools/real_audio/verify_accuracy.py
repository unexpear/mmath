"""Check that the shipped desktop helpers reproduce the frozen experiments."""
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import numpy as np
from prepare import digest
from evaluate import save

root=Path(__file__).resolve().parents[2]
data=root/'build_real_audio'
exe=data/'cpp/Release/accuracy_probe.exe'
selected={'pitch':'2048-60','chords':'2500-10'}
jobs=[]
for group,manifest in [('development',data/'manifest.json'),('holdout',data/'accuracy/holdout.json')]:
    for t in json.loads(manifest.read_text())['tracks']:
        if t.get('kind','guitar')=='guitar': jobs.append((group,t))
folder=data/'accuracy/production'
folder.mkdir(exist_ok=True)

def verify(job):
    group,t=job
    key=selected[t['mode']]
    path=folder/(t['id']+'.json')
    if digest(data/t['audio'])!=t['audioSha256']: raise ValueError('Changed audio')
    subprocess.run([str(exe),str(data/t['audio']),str(path),t['mode'],key,'production'],check=True)
    actual=json.loads(path.read_text())['variants'][key]
    expected=json.loads((data/'accuracy'/group/(t['id']+'.json')).read_text())['variants'][key]
    np.testing.assert_allclose(actual,expected,rtol=0,atol=1e-10)
    return dict(id=t['id'],group=group,rows=len(actual),matches=True)

with ThreadPoolExecutor(max_workers=3) as pool: results=list(pool.map(verify,jobs))
save(data/'accuracy/production-verification.json',dict(probeSha256=digest(exe),tracks=results))
print(f'PASS: shipped helpers match frozen predictions on {len(results)} real recordings')
