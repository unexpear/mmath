"""Fetch seven independently published acoustic chord recordings for evaluation.

The audio is PD-self, not merely a page whose metadata happens to be CC0.
No inference or tuning occurs during acquisition. Uploader chord labels are
clip-level references; bass, precise notes and onsets are not certified.
"""
import html
import json
from pathlib import Path
import re
from urllib.request import Request, urlopen
from urllib.error import HTTPError
import numpy as np
import soundfile as sf
from evaluate import save
from prepare import digest

def fetch(url):
    with urlopen(Request(url,headers={'User-Agent':'MMathEnAudioValidation/1.0 (local research; source provenance)'}),timeout=60) as r:
        return r.read()

def prepare(root=Path('build_real_audio/public-domain-chords')):
    root.mkdir(exist_ok=True)
    manifest=root/'manifest.json'
    if manifest.exists():return
    tracks=[];unavailable=[]
    for note in 'ABCDEFG':
        name=note+'-major';page='https://commons.wikimedia.org/wiki/File:'+name+'.ogg'
        path=root/(name+'.html')
        if not path.exists():path.write_bytes(fetch(page))
        text=path.read_text(encoding='utf-8')
        if 'I, the copyright holder of this work' not in text or 'public domain' not in text or 'WBOSITG' not in text:
            raise ValueError('Public-domain dedication/author missing: '+page)
        urls=re.findall(r'href="(https://upload.wikimedia.org/[^"?]+/'+name+r'\.ogg)(?:\?[^\"]*)?"',text)
        if not urls:raise ValueError('Original download not found: '+page)
        source=html.unescape(urls[0]);original=root/(name+'.ogg')
        if not original.exists():
            try:original.write_bytes(fetch(source))
            except HTTPError as e:
                unavailable.append(dict(id=name,sourcePage=page,sourceUrl=source,error=str(e)))
                print(name,str(e),flush=True)
                continue
        x,rate=sf.read(original,always_2d=True);mono=x.mean(axis=1)
        wav=root/(name+'.wav');sf.write(wav,mono,rate,subtype='FLOAT')
        reread,sr=sf.read(wav)
        assert sr==rate and np.max(abs(reread-mono))<1e-7
        tracks.append(dict(id=name,audio=wav.name,audioSha256=digest(wav),original=original.name,
            originalSha256=digest(original),sourcePage=page,sourceUrl=source,sourceSnapshot=path.name,
            sourceSnapshotSha256=digest(path),author='WBOSITG',license='Public-domain dedication by author (PD-self)',
            expected=note+':maj',referenceStrength='Uploader-declared acoustic chord; no frame or bass annotation',
            sampleRate=rate,duration=len(x)/rate))
        print(name,flush=True)
    save(manifest,dict(purpose='External evaluation only; never training or parameter selection',tracks=tracks,unavailable=unavailable))

def export_suite(root=Path('build_real_audio/public-domain-chords')):
    manifest=json.loads((root/'manifest.json').read_text());base=root.parent
    rows=['MMATHEN_SUITE_V1','SUMMARY\tSeven public-domain acoustic chords by WBOSITG. Clip labels from uploader; exact note timing and bass are unverified.']
    for t in manifest['tracks']:
        identity='PD-'+t['id'];name=root/(identity+'.tsv')
        pc='C D EF G A B'.index(t['expected'][0])
        name.write_text('\n'.join(['MMATHEN_REFERENCE_V1',
            f"META\t{identity}\tguitar\tchords\tpublic-domain-chords/{t['audio']}\t{t['duration']:.17g}",
            'STATS\tPublic-domain acoustic recording by WBOSITG. Uploader chord label applies to the clip; precise note timing and bass are not annotated.',
            f"CHORD\t0\t{t['duration']:.17g}\t{pc}\t{t['expected']}",'']))
        rows.append(f'TRACK\t{identity}\tguitar\tchords\tpublic-domain-chords/{name.name}')
    (base/'public-domain-suite.tsv').write_text('\n'.join(rows)+'\n')
    default=base/'reference-suite.tsv'
    if default.exists():
        current=[line for line in default.read_text().splitlines() if not line.startswith('TRACK\tPD-')]
        default.write_text('\n'.join(current+rows[2:])+'\n')

if __name__=='__main__':
    prepare();export_suite()
