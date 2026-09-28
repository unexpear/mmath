"""Export a deliberately simple, strict TSV bundle for the native desktop app."""
from pathlib import Path


def write_rows(path, rows):
    def field(value):
        text = str(value)
        if any(c in text for c in '\t\r\n'):
            raise ValueError('Reference bundle fields cannot contain tabs or newlines')
        return text
    path.write_text('\n'.join('\t'.join(field(v) for v in row) for row in rows) + '\n', encoding='utf-8')


def percent(v):
    return 'not scored' if v is None else f'{100*v:.1f}%'


def export_desktop(root, report):
    from evaluate import LABELS
    folder = root / 'reference_demo'
    folder.mkdir(exist_ok=True)
    s = report['summary']
    catalog = [['MMATHEN_SUITE_V1'], ['SUMMARY',
        f"Chords {percent(s['chordAccuracy'])} ({s['chordScoredSeconds']:.1f}/{s['chordAnnotatedSeconds']:.1f} s scored) | Pitch {percent(s['pitchAccuracy50c'])} ({s['pitchEligibleFrames']}/{s['pitchTotalFrames']} frames) | Words {percent(s['wordErrorRate'])} error ({s['wordErrors']}/{s['referenceWords']})"]]
    for track in report['tracks']:
        rows = [['MMATHEN_REFERENCE_V1'], ['META', track['id'], track['kind'], track['mode'], track['audio'], track['duration']]]
        if track['mode'] == 'chords':
            m = track['metrics']
            rows.append(['STATS', f"Saved baseline: {percent(m['accuracy'])} agreement; {m['scoredSeconds']:.2f} s scored, {m['unsupportedSeconds']:.2f} s unsupported, {m['uncoveredSeconds']:.2f} s uncovered."])
            for a, b, label in track['performed']:
                reduced = track['reduced'][label]
                rows.append(['CHORD', a, b, LABELS.index(reduced) if reduced else -1, label])
        elif track['kind'] == 'guitar':
            m = track['pitchMetrics']
            rows.append(['STATS', f"Saved baseline: {percent(m['accuracy50c'])} within 50 cents; {m['eligibleFrames']}/{m['totalFrames']} frames eligible."])
        else:
            m = track['wordMetrics']
            rows += [['STATS', f"Whisper base.en: {percent(m['wer'])} word error; {m['substitutions']} substitutions, {m['deletions']} missing, {m['insertions']} extra / {m['referenceWords']} words."],
                     ['TRANSCRIPT', track['transcript']], ['ESTIMATE', track['speech']['text']]]
            rows.extend(['WORD', *w[:3]] for w in track['speech']['words'])
            rows.extend(['EDIT', *a] for a in m['alignment'])
        rows.extend(['NOTE', *n] for n in track.get('notes', []))
        rows.extend(['PITCH', p[0], p[3] or 0] for p in track.get('pitch', []))
        path = folder / (track['id'] + '.tsv')
        write_rows(path, rows)
        catalog.append(['TRACK', track['id'], track['kind'], track['mode'], path.relative_to(root).as_posix()])
    write_rows(root / 'reference-suite.tsv', catalog)
