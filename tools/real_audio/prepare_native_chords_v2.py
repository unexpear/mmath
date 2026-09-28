"""Freeze a second, disjoint evaluation after the unanchored model failed.

No model refit: keep the 90-recording weights. The 30 failed-test recordings
are now development only. A new set is selected without inspecting labels.
"""
import hashlib
import json
from pathlib import Path
import zipfile
from native_chord_model import development
from evaluate import save
from prepare import digest, KNOWN_ISSUES

root=Path('build_real_audio');old=root/'native-chords';target=root/'native-chords-v2';target.mkdir(exist_ok=True)
consumed=development(root)+json.loads((old/'reserved.json').read_text())['tracks']
save(target/'development-tracks.json',dict(tracks=consumed))
path=target/'reserved.json'
if not path.exists():
    used={t['id'] for t in consumed};tracks=[]
    with zipfile.ZipFile(root/'annotation.zip') as az, zipfile.ZipFile(root/'audio_mono-mic.zip') as wz:
        names=[Path(n).stem for n in az.namelist() if n.endswith('.jams') and '/' not in n]
        wavs={Path(n).name:n for n in wz.namelist() if not n.startswith('__MACOSX')}
        for player in range(6):
            for style in ('BN','Funk','Jazz','Rock','SS'):
                pool=[n for n in names if n.startswith(f'{player:02d}_{style}') and n.endswith('_comp') and n not in used and n not in KNOWN_ISSUES]
                name=min(pool,key=lambda n:hashlib.sha256(('native-v2-2026-09-23:'+n).encode()).hexdigest())
                audio,ref=target/(name+'.wav'),target/(name+'.jams')
                audio.write_bytes(wz.read(wavs[name+'_mic.wav']));ref.write_bytes(az.read(name+'.jams'))
                tracks.append(dict(id=name,audio=audio.relative_to(root).as_posix(),reference=ref.relative_to(root).as_posix(),audioSha256=digest(audio),referenceSha256=digest(ref)))
    save(path,dict(selection='One unused comp per player/style, deterministic SHA256 ordering with seed native-v2-2026-09-23. Excludes all 120 consumed recordings and known issues. Same six players and shared progressions.',tracks=tracks))
candidates=json.loads((old/'anchored-candidates.json').read_text())
eligible=[c for c in candidates if c['previous90']['majmin']>=.665135397187049-1e-10 and c['consumed30']['majmin']>=.5784756767560452-1e-10]
best=max(eligible,key=lambda c:c['scores']['tetrads_inv'])
model=json.loads((old/'model-logistic.json').read_text());model.update(anchored=True,detailPenalty=0,baseStay=best['stay'])
if (target/'selection.json').exists():raise ValueError('Already frozen')
save(target/'model.json',model)
save(target/'development-selection.json',dict(candidates=candidates,selected=best,weightsRefit=False,
    notes='Penalty 0,1,2,4 examined at original persistence .9; zero improved detail agreement most in both consumed subsets. Persistence .9,.98,.995 compared without changing acoustic weights.'))
print(json.dumps(best,indent=2))
