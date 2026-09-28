"""Freeze unused GuitarSet recordings, tune on the old set, then evaluate once."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import statistics
import subprocess
import zipfile

from prepare import digest, KNOWN_ISSUES
from evaluate import parse_guitar, score_chords, chord_cells, save
from compare_tuners import reference_at, error


def freeze(root):
    target = root / 'accuracy'
    target.mkdir(exist_ok=True)
    dest = target / 'holdout.json'
    if dest.exists():
        return json.loads(dest.read_text())
    old = {t['id'] for t in json.loads((root / 'manifest.json').read_text())['tracks']}
    tracks = []
    with zipfile.ZipFile(root / 'annotation.zip') as az, zipfile.ZipFile(root / 'audio_mono-mic.zip') as wz:
        names = sorted(Path(n).stem for n in az.namelist() if n.endswith('.jams') and '/' not in n)
        wavs = {Path(n).name: n for n in wz.namelist() if not n.startswith('__MACOSX')}
        for player in range(6):
            for style in ('BN', 'Funk', 'Jazz', 'Rock', 'SS'):
                for suffix, mode in (('_comp','chords'), ('_solo','pitch')):
                    name = next(n for n in names if n.startswith(f'{player:02d}_{style}')
                                and n.endswith(suffix) and n not in old and n not in KNOWN_ISSUES)
                    audio, ref = target / (name+'.wav'), target / (name+'.jams')
                    audio.write_bytes(wz.read(wavs[name+'_mic.wav']))
                    ref.write_bytes(az.read(name+'.jams'))
                    tracks.append(dict(id=name,mode=mode,audio=audio.relative_to(root).as_posix(),
                                       reference=ref.relative_to(root).as_posix(),
                                       audioSha256=digest(audio),referenceSha256=digest(ref)))
    result = dict(selection='Frozen before development predictions: first unused lexicographic comp and solo for every player/style, excluding documented issues. Same six performers; unseen recordings, not unseen players.',tracks=tracks)
    save(dest,result)
    return result


def score_track(root, track, executable, selected):
    audio, refpath = root / track['audio'], root / track['reference']
    if digest(audio)!=track['audioSha256'] or digest(refpath)!=track['referenceSha256']:
        raise ValueError('Changed input '+track['id'])
    folder = root / 'accuracy' / ('holdout' if selected else 'development')
    folder.mkdir(exist_ok=True)
    path = folder / (track['id']+'.json')
    args = [str(executable),str(audio),str(path),track['mode']]
    if selected: args.append(selected[track['mode']])
    subprocess.run(args,check=True)
    raw = json.loads(path.read_text())
    ref = parse_guitar(refpath)
    metrics = {}
    for name, rows in raw['variants'].items():
        if track['mode']=='chords':
            data = dict(raw,config=dict(hop=2048),chords=rows)
            metrics[name] = score_chords(ref['performed'],chord_cells(data,2))[0]
        else:
            points=[]
            for row in rows:
                # Same conservative 4096-sample input window for every variant.
                truth=reference_at(ref,row[0],max(0,row[0]-4096/raw['sampleRate']))
                if truth: points.append(error(row,truth[1]))
            errors=[e for e in points if e is not None]
            metrics[name] = dict(eligible=len(points),voiced=len(errors),errors=errors,
                                 correct5=sum(abs(e)<=5 for e in errors),
                                 correct10=sum(abs(e)<=10 for e in errors),
                                 correct50=sum(abs(e)<=50 for e in errors))
    print(track['id'],flush=True)
    return dict(id=track['id'],mode=track['mode'],metrics=metrics)


def run(root, executable, selected=None):
    holdout=freeze(root)
    tracks=holdout['tracks'] if selected else [t for t in json.loads((root/'manifest.json').read_text())['tracks'] if t['kind']=='guitar']
    with ThreadPoolExecutor(max_workers=3) as pool:
        results=list(pool.map(lambda t:score_track(root,t,executable,selected),tracks))
    summary={}
    for mode in ('chords','pitch'):
        subset=[t for t in results if t['mode']==mode]
        summary[mode]={}
        for variant in subset[0]['metrics']:
            ms=[t['metrics'][variant] for t in subset]
            if mode=='chords':
                totals={key:sum(m[key] for m in ms) for key in ('correctSeconds','scoredSeconds','annotatedSeconds','unsupportedSeconds')}
                totals['accuracy']=totals['correctSeconds']/totals['scoredSeconds']
            else:
                totals={key:sum(m[key] for m in ms) for key in ('eligible','voiced','correct5','correct10','correct50')}
                for c in (5,10,50): totals[f'accuracy{c}']=totals[f'correct{c}']/totals['eligible']
                totals['medianAbsoluteCentsWhenVoiced']=statistics.median(abs(e) for m in ms for e in m['errors'])
            summary[mode][variant]=totals
    target=root/'accuracy'/('holdout-report.json' if selected else 'development-report.json')
    save(target,dict(probeSha256=digest(executable),selected=selected,summary=summary,tracks=results))
    print(json.dumps(summary,indent=2),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path('build_real_audio'))
    p.add_argument('--probe',type=Path,default=Path('build_real_audio/cpp/Release/accuracy_probe.exe'))
    p.add_argument('--pitch')
    p.add_argument('--chords')
    a=p.parse_args()
    if bool(a.pitch)!=bool(a.chords): p.error('Select both settings before holdout evaluation')
    selected=dict(pitch=a.pitch,chords=a.chords) if a.pitch else None
    run(a.root.resolve(),a.probe.resolve(),selected)
