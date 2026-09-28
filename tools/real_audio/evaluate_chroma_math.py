"""Separate arithmetic precision from spectral modelling on real recordings."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import subprocess
import zipfile
import mir_eval.chord as chord
import numpy as np
from evaluate import chord_cells, parse_guitar, score_chords, save
from prepare import digest, KNOWN_ISSUES


def freeze(root, target):
    path = target / 'holdout.json'
    if path.exists(): return json.loads(path.read_text())
    used = {t['id'] for name in ('manifest.json', 'accuracy/holdout.json')
            for t in json.loads((root / name).read_text())['tracks']}
    tracks = []
    with zipfile.ZipFile(root / 'annotation.zip') as az, zipfile.ZipFile(root / 'audio_mono-mic.zip') as wz:
        names = sorted(Path(n).stem for n in az.namelist() if n.endswith('.jams') and '/' not in n)
        wavs = {Path(n).name: n for n in wz.namelist() if not n.startswith('__MACOSX')}
        for player in range(6):
            for style in ('BN', 'Funk', 'Jazz', 'Rock', 'SS'):
                name = next(n for n in names if n.startswith(f'{player:02d}_{style}') and n.endswith('_comp')
                            and n not in used and n not in KNOWN_ISSUES)
                audio, ref = target / (name + '.wav'), target / (name + '.jams')
                audio.write_bytes(wz.read(wavs[name + '_mic.wav']))
                ref.write_bytes(az.read(name + '.jams'))
                tracks.append(dict(id=name, audio=audio.relative_to(root).as_posix(), reference=ref.relative_to(root).as_posix(),
                    audioSha256=digest(audio), referenceSha256=digest(ref)))
    result = dict(selection='First unused lexicographic accompaniment per player/style, excluding both previous sets and documented issues. Frozen before new predictions. Same players, new recordings.', tracks=tracks)
    save(path, result)
    return result


def run(root, probe, heldout=False):
    target = root / 'math-improvement'
    target.mkdir(exist_ok=True)
    unseen = freeze(root, target)
    selected = None
    if heldout:
        selection = json.loads((target / 'selection.json').read_text())
        selected = selection['variant']
        if selected not in ('peaks', 'peaks2'):
            raise ValueError('No development candidate passed; keep the holdout reserved')
        if digest(probe) != selection['probeSha256']:
            raise ValueError('Probe changed after development selection')
    tracks = unseen['tracks'] if heldout else [t for t in json.loads((root / 'manifest.json').read_text())['tracks'] if t['mode'] == 'chords']
    folder = target / ('holdout' if heldout else 'development')
    folder.mkdir(exist_ok=True)
    def one(t):
        audio, refpath = root / t['audio'], root / t['reference']
        if digest(audio) != t['audioSha256'] or digest(refpath) != t['referenceSha256']: raise ValueError('Input changed')
        path = folder / (t['id'] + '.json')
        cmd = [str(probe), str(audio), str(path)] + ([selected] if selected else [])
        subprocess.run(cmd, check=True, timeout=90)
        raw = json.loads(path.read_text())
        if not heldout and raw['extendedMantissaBits'] <= 53: raise ValueError('Compiler has no extra long-double precision')
        ref = parse_guitar(refpath)['performed']
        metrics = {}
        for name, v in raw['variants'].items():
            cells = chord_cells(dict(raw, config=dict(hop=2048), chords=v['chords']), 2)
            _, overlaps = score_chords(ref, cells)
            weights = np.array([b-a for a,b,*_ in overlaps])
            labels = [r[2] for r in overlaps], [r[3] for r in overlaps]
            scores = {}
            for rule in ('majmin', 'tetrads', 'tetrads_inv'):
                values = getattr(chord, rule)(*labels); valid = values >= 0
                scores[rule] = dict(correctSeconds=float(weights[valid] @ values[valid]), scoredSeconds=float(weights[valid].sum()))
            metrics[name] = dict(scores=scores, maxFeatureDelta=v['maxFeatureDelta'], changedFrames=v['changedFrames'], frames=len(v['chords']))
        control = metrics.get('double-control')
        if control and (control['changedFrames'] or control['maxFeatureDelta'] > 1e-12): raise ValueError('Audit does not reproduce production')
        print(t['id'], flush=True)
        return dict(id=t['id'], metrics=metrics)
    with ThreadPoolExecutor(max_workers=3) as pool: results = list(pool.map(one, tracks))
    summary = {}
    for name in results[0]['metrics']:
        ms = [r['metrics'][name] for r in results]
        summary[name] = dict(frames=sum(m['frames'] for m in ms), changedFrames=sum(m['changedFrames'] for m in ms),
                            maxFeatureDelta=max(m['maxFeatureDelta'] for m in ms), scores={})
        for rule in ('majmin', 'tetrads', 'tetrads_inv'):
            totals = {k: sum(m['scores'][rule][k] for m in ms) for k in ('correctSeconds', 'scoredSeconds')}
            totals['accuracy'] = totals['correctSeconds'] / totals['scoredSeconds']
            summary[name]['scores'][rule] = totals
    save(target / ('holdout-report.json' if heldout else 'development-report.json'), dict(probeSha256=digest(probe), tracks=results, summary=summary))
    if not heldout:
        baseline = summary['baseline']['scores']
        candidates = [v for v in ('peaks', 'peaks2') if summary[v]['scores']['majmin']['accuracy'] >= baseline['majmin']['accuracy']]
        best = max(['baseline'] + candidates, key=lambda v: summary[v]['scores']['tetrads_inv']['accuracy'])
        # Selection is written once, before any held-out prediction.
        selection = target / 'selection.json'
        if not selection.exists(): save(selection, dict(variant=best, criterion='Highest development full-quality-plus-bass agreement, without reducing major/minor agreement.', probeSha256=digest(probe)))
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, default=Path('build_real_audio'))
    p.add_argument('--probe', type=Path, default=Path('build_math_gcc/chroma_math_probe.exe'))
    p.add_argument('--heldout', action='store_true')
    a = p.parse_args()
    run(a.root.resolve(), a.probe.resolve(), a.heldout)
