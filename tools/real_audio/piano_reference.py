"""CC0 recorded piano notes: provenance, reproducible exports, audio-only checks.

The SFZ key is a note identity, NOT calibrated frequency ground truth. Windows
are fixed before recognition (.2--1.5 s); no detector output chooses eligibility.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import subprocess
import urllib.request
import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'build_real_audio/piano'
SOURCE = 'https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html'
ARCHIVE = 'https://freepats.zenvoid.org/Piano/UprightPianoKW/UprightPianoKW-SFZ+FLAC-20220221.7z'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def prepare():
    import py7zr
    OUT.mkdir(parents=True, exist_ok=True)
    archive = OUT / 'UprightPianoKW.7z'
    if not archive.exists():
        urllib.request.urlretrieve(ARCHIVE, archive)
    if not (OUT / 'source.html').exists():
        urllib.request.urlretrieve(SOURCE, OUT / 'source.html')
    raw = (OUT / 'raw').resolve()
    if not list(raw.glob('*/*.sfz')):
        with py7zr.SevenZipFile(archive) as z:
            for n in z.getnames():
                if not (raw / n).resolve().is_relative_to(raw):
                    raise ValueError('Archive path outside output')
            z.extractall(raw)
    mapping = next(raw.glob('*/*.sfz'))
    entries = []
    suite = ['MMATHEN_SUITE_V1', 'SUMMARY\tCC0 acoustic upright piano samples, one instrument. SFZ note labels; no calibrated frequency or independent onset/offset reference.']
    for block in mapping.read_text().split('<region>')[1:]:
        note = int(re.search(r'(?:pitch_keycenter|\bkey)=(\d+)', block)[1])
        filename = re.search(r'sample=(\S+)', block)[1]
        path = mapping.parent / filename
        audio, rate = sf.read(path, always_2d=True)
        # Play the recorded attack once; do not apply SFZ loops or transposition.
        audio = audio[:3*rate].mean(axis=1)
        ident = 'Piano-KW-' + path.stem.replace('#', 'sharp')
        wav = OUT / (ident + '.wav')
        sf.write(wav, audio, rate, subtype='FLOAT')
        duration = len(audio) / rate
        start, end = .2, min(1.5, duration-.05)
        entries.append(dict(id=ident, midi=note, source=str(path.relative_to(OUT)),
                            sourceSha256=digest(path), wav=wav.name, wavSha256=digest(wav),
                            rate=rate, duration=duration, start=start, end=end))
        lines = ['MMATHEN_REFERENCE_V1', f'META\t{ident}\tpiano\tpitch\tpiano/{wav.name}\t{duration:.17g}',
                 'STATS\tReal Kawai upright sample, CC0; mono downmix, first 3 seconds, no pitch shifting or looping. Scored against the published note name, not a calibrated tone.',
                 f'NOTE\t{start}\t{end}\t{note}\t-1']
        for tick in range(1, int(duration*15)+1):
            t = round(tick*rate/15)/rate
            # Require the complete 25-Hz acquisition window inside the fixed interval.
            hz = 440*2**((note-69)/12) if t-(2*np.ceil(rate/25)+4)/rate >= start and t <= end else 0
            lines.append(f'PITCH\t{t:.17g}\t{hz:.17g}')
        bundle = OUT / (ident+'.tsv')
        bundle.write_text('\n'.join(lines)+'\n')
        suite.append(f'TRACK\t{ident}\tpiano\tpitch\tpiano/{bundle.name}')
    manifest = dict(source=SOURCE, archive=ARCHIVE, archiveSha256=digest(archive),
                    license='CC0-1.0', authors='Gonzalo and Roberto / FreePats',
                    recording='Kawai upright in a living room, January 2017; edited sample release 2022-02-21',
                    limitation='One piano, sparse sampled keys, two velocity layers; note identity only, no absolute-cent accuracy claim.',
                    criteria='All mapped original samples; first 3s; fixed early interval .2 to 1.5s; <=50c from nominal note, missed voices count as wrong. No fitting.',
                    entries=entries)
    (OUT/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (OUT.parent/'piano-suite.tsv').write_text('\n'.join(suite)+'\n')
    default = OUT.parent/'reference-suite.tsv'
    if default.exists():
        old = [s for s in default.read_text().splitlines() if not s.startswith('TRACK\tPiano-KW-')]
        default.write_text('\n'.join(old+suite[2:])+'\n')
    print(f'Prepared {len(entries)} recorded notes, MIDI {min(e["midi"] for e in entries)}--{max(e["midi"] for e in entries)}', flush=True)

def evaluate(exe):
    manifest=json.loads((OUT/'manifest.json').read_text())
    results=OUT/'checks'; results.mkdir(exist_ok=True)
    def run(e):
        row=dict(id=e['id'], midi=e['midi'])
        for mode in ['pitch','music-pitch']:
            output=results/(e['id']+'-'+mode+'.json')
            subprocess.run([str(exe),str(OUT/e['wav']),str(output),mode], check=True, capture_output=True, timeout=120)
            frames=json.loads(output.read_text())['pitch']
            # Same eligibility for both ranges; includes missed/quiet frames.
            n=2*np.ceil(e['rate']/25)+4
            scored=[p for p in frames if p[0]-n/e['rate']>=e['start'] and p[0]<=e['end']]
            expected=440*2**((e['midi']-69)/12)
            row[mode]=dict(correct=sum(p[3] and abs(1200*np.log2(p[2]/expected))<=50 for p in scored),
                           eligible=len(scored), maxFrameMs=max(p[6] for p in frames))
        return row
    with ThreadPoolExecutor(max_workers=2) as pool:
        rows=list(pool.map(run,manifest['entries']))
    totals={}
    for mode in ['pitch','music-pitch']:
        eligible=sum(r[mode]['eligible'] for r in rows)
        correct=sum(r[mode]['correct'] for r in rows)
        totals[mode]=dict(correct=correct, eligible=eligible, agreement=correct/eligible,
                          majorityCorrectClips=sum(r[mode]['correct']>r[mode]['eligible']/2 for r in rows),
                          clips=len(rows), maxFrameMs=max(r[mode]['maxFrameMs'] for r in rows))
    report=dict(probeSha256=digest(exe), manifestSha256=digest(OUT/'manifest.json'), totals=totals, rows=rows,
                limitation=manifest['limitation'], criteria=manifest['criteria'])
    (OUT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(totals,indent=2),flush=True)

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--prepare',action='store_true')
    parser.add_argument('--probe',type=Path)
    args=parser.parse_args()
    if args.prepare: prepare()
    if args.probe: evaluate(args.probe.resolve())
