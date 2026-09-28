"""Regression checks on transformed copies, not additional real recordings."""
import json
from pathlib import Path
import subprocess
import numpy as np
import soundfile as sf
import mir_eval.chord as chord
from prepare import digest
from evaluate import save

root=Path('build_real_audio');source=root/'public-domain-chords';out=root/'native-chords-v2/stress';out.mkdir(exist_ok=True)
probe=Path('build_math_gcc/native_chord_probe.exe').resolve()
selection=json.loads((root/'native-chords-v2/selection.json').read_text());assert digest(probe)==selection['probeSha256']
rng=np.random.default_rng(20260923);results=[]
for t in json.loads((source/'manifest.json').read_text())['tracks']:
    assert digest(source/t['audio'])==t['audioSha256']
    x,rate=sf.read(source/t['audio']);rms=np.sqrt(np.mean(x*x))
    for mode in ('quiet','noise20','noise10'):
        if mode=='quiet':changed=x*.1
        else:
            noise=rng.standard_normal(len(x));noise/=np.sqrt(np.mean(noise*noise))
            changed=x+noise*rms*10**(-int(mode[5:])/20)
        wav=out/(t['id']+'-'+mode+'.wav');path=wav.with_suffix('.json')
        sf.write(wav,changed,rate,subtype='FLOAT')
        subprocess.run([str(probe),str(wav),str(path)],check=True,timeout=120)
        rows=json.loads(path.read_text())['chords'];result=dict(id=t['id'],mode=mode,expected=t['expected'])
        for name,index in [('baseline',3),('improved',2)]:
            labels=[r[index] for r in rows];dominant=max(sorted(set(labels)),key=labels.count)
            result[name]=dict(majority=dominant,clipCorrect=bool(chord.tetrads([t['expected']],[dominant])[0]==1),
                frameQualityAgreement=float(np.mean(chord.tetrads([t['expected']]*len(labels),labels))))
        results.append(result)
save(out/'report.json',dict(probeSha256=digest(probe),seed=20260923,
    limitation='Same seven clips, not independent examples. White noise uses whole-clip RMS SNR; it is not a real room recording.',tracks=results))
for mode in ('quiet','noise20','noise10'):
    items=[r for r in results if r['mode']==mode]
    print(mode,{name:sum(r[name]['clipCorrect'] for r in items) for name in ('baseline','improved')},flush=True)
