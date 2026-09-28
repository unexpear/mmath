"""Independently compare the desktop C++ adapter with the frozen Python run."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
from evaluate import LABELS, parse_guitar, reduce_label, save
from prepare import digest

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=Path, default=Path('build_app_msvc/verified/chordino_check.exe'))
parser.add_argument('--reserved', action='store_true', help='Check the September 23 reserved comparison')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2] / 'build_real_audio'
target = root / 'community'
check = target / ('native-check-reserved' if args.reserved else 'native-check')
check.mkdir(exist_ok=True)
manifest_path = root / ('math-improvement/holdout.json' if args.reserved else 'accuracy/holdout.json')
tracks = [t for t in json.loads(manifest_path.read_text())['tracks'] if t.get('mode','chords') == 'chords']
manifest, expected = [], {}
for t in tracks:
    refpath, audio = root / t['reference'], root / t['audio']
    if digest(refpath) != t['referenceSha256'] or digest(audio) != t['audioSha256']:
        raise ValueError('Changed recording or annotation')
    baseline_path = root / 'recognition-improvement/native-holdout' / (t['id'] + '-baseline.json') if args.reserved else root / 'accuracy/production' / (t['id'] + '.json')
    raw = json.loads(baseline_path.read_text())
    duration = raw['samples'] / raw['sampleRate']
    truth = [f'META\t{t["id"]}\tguitar\tchords\taudio.wav\t{duration}']
    for a, b, label in parse_guitar(refpath)['performed']:
        reduced = reduce_label(label)
        state = LABELS.index(reduced) if reduced else -1
        truth.append(f'CHORD\t{a}\t{b}\t{state}\t{label}')
    path = check / (t['id'] + '.tsv')
    path.write_text('MMATHEN_REFERENCE_V1\n' + '\n'.join(truth) + '\n')
    prediction_path = root / 'recognition-improvement/native-holdout' / (t['id'] + '-chordino.json') if args.reserved else target / (t['id'] + '-chords.json')
    frozen = json.loads(prediction_path.read_text())
    m = frozen['metrics']['chordino']
    if args.reserved: m=dict(correctSeconds=m['majmin']['correct'],scoredSeconds=m['majmin']['eligible'])
    manifest.append(f'{audio}\t{path}\t{m["correctSeconds"]}\t{m["scoredSeconds"]}')
    expected[t['id']] = frozen['estimates']
(check / 'manifest.tsv').write_text('\n'.join(manifest) + '\n')
exe = args.exe.resolve()
subprocess.run([str(exe), str(target), str(check / 'manifest.tsv'), str(check / 'predictions.tsv')], check=True, timeout=120)
actual = {key: [] for key in expected}
for line in (check / 'predictions.tsv').read_text().splitlines():
    key, a, b, state, label = line.split('\t')
    reduced = reduce_label(label)
    if int(state) != (LABELS.index(reduced) if reduced else -2):
        raise ValueError('Native label projection disagrees with mir_eval')
    actual[key].append([float(a), float(b), label])
for key, rows in expected.items():
    if len(rows) != len(actual[key]): raise ValueError('Interval count mismatch')
    for a, b in zip(rows, actual[key]):
        if a[2] != b[2] or max(abs(a[0]-b[0]), abs(a[1]-b[1])) > 1e-8:
            raise ValueError('Native output differs from frozen prediction')
fixture = check / 'process-fixture'
fake = fixture / 'sonic/sonic-annotator-win64/sonic-annotator.exe'
fake.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(exe, fake)
(fixture / 'chordino').mkdir(exist_ok=True)
(fixture / 'chordino/nnls-chroma.dll').write_text('unused by test helper')
(fixture / 'chordino-harte.ttl').write_text('unused by test helper')
subprocess.run([str(exe), '--failures', str(fixture), str(root / tracks[0]['audio'])], check=True, timeout=10)
save(check / 'verification.json', dict(tracks=len(tracks), allLabelsAndIntervalsMatch=True,
    scoresMatchMirEval=True, failedProcessRejected=True, cancellationPassed=True, executableSha256=digest(exe)))
print('PASS: native full labels, projection, intervals, scores, failure and cancellation')
