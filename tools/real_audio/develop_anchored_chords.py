"""Development after the first candidate failed: no fresh evaluation input.

Weights remain fitted to the original 90 recordings. The first failed set is
explicitly consumed development data here; it is never reported as held out.
"""
import json
from pathlib import Path
from native_chord_model import development, load_track, predict, summary
from verify_recognition import chord_metrics
from evaluate import save

root=Path('build_real_audio');target=root/'native-chords'
if (root/'native-chords-v2/selection.json').exists():raise ValueError('Second candidate already frozen')
model=json.loads((target/'model-logistic.json').read_text())
tracks=development(root)+json.loads((target/'reserved.json').read_text())['tracks']
loaded=[load_track(root,target,t) for t in tracks]
penalties=[];candidates=[]
for penalty,stay in [(0,.9),(1,.9),(2,.9),(4,.9),(0,.98),(0,.995)]:
    candidate=dict(model,anchored=True,detailPenalty=penalty,baseStay=stay)
    ms=[chord_metrics(predict(t,candidate,candidate['switch']),t['ref']) for t in loaded]
    row=dict(stay=stay,detailPenalty=penalty,scores=summary(ms),previous90=summary(ms[:90]),consumed30=summary(ms[90:]))
    if stay==.9:penalties.append(row)
    if penalty==0:candidates.append(row)
    print(json.dumps(row),flush=True)
save(target/'anchored-penalties.json',penalties)
save(target/'anchored-candidates.json',candidates)
