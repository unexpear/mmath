"""Score frozen predictions at richer chord vocabularies, without retuning."""
import json
from pathlib import Path
import mir_eval.chord as chord
import numpy as np
from evaluate import parse_guitar, score_chords, chord_cells, save
from prepare import digest

root = Path(__file__).resolve().parents[2] / 'build_real_audio'
target = root / 'community'
tracks = [t for t in json.loads((root / 'accuracy/holdout.json').read_text())['tracks']
          if t['mode'] == 'chords']
rules = ('majmin', 'triads', 'tetrads', 'tetrads_inv')
results = []
for t in tracks:
    reference = root / t['reference']
    if digest(reference) != t['referenceSha256']:
        raise ValueError('Reference changed')
    ref = parse_guitar(reference)['performed']
    raw = json.loads((root / 'accuracy/production' / (t['id'] + '.json')).read_text())
    desktop = chord_cells(dict(raw, config=dict(hop=2048), chords=raw['variants']['2500-10']), 2)
    community = json.loads((target / (t['id'] + '-chords.json')).read_text())['estimates']
    metrics = {}
    for model, estimates in [('desktop', desktop), ('chordino', community)]:
        _, overlaps = score_chords(ref, estimates)
        weights = np.array([b - a for a, b, *_ in overlaps])
        labels = [r[2] for r in overlaps], [r[3] for r in overlaps]
        metrics[model] = {}
        for name in rules:
            scores = getattr(chord, name)(*labels)
            valid = scores >= 0
            correct = float(weights[valid] @ scores[valid])
            supported = float(weights[valid].sum())
            accuracy = correct / supported if supported else None
            if supported and abs(accuracy - chord.weighted_accuracy(scores, weights)) > 1e-12:
                raise ValueError('Evaluator mismatch')
            metrics[model][name] = dict(correctSeconds=correct, scoredSeconds=supported,
                annotatedSeconds=sum(b-a for a, b, _ in ref), comparedSeconds=float(weights.sum()),
                accuracy=accuracy)
    for name in rules:
        if abs(metrics['desktop'][name]['scoredSeconds'] - metrics['chordino'][name]['scoredSeconds']) > 1e-7:
            raise ValueError('Unequal comparison coverage')
    results.append(dict(id=t['id'], metrics=metrics))
summary = {}
for model in ('desktop', 'chordino'):
    summary[model] = {}
    for name in rules:
        total = {key: sum(t['metrics'][model][name][key] for t in results)
                 for key in ('correctSeconds', 'scoredSeconds', 'annotatedSeconds', 'comparedSeconds')}
        total['accuracy'] = total['correctSeconds'] / total['scoredSeconds']
        total['coverage'] = total['scoredSeconds'] / total['annotatedSeconds']
        summary[model][name] = total
save(target / 'quality-report.json', dict(summary=summary, tracks=results))
print(json.dumps(summary, indent=2))
