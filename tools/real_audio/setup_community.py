"""Prepare the optional Windows helper locally; downloads are SHA-256 pinned."""
import argparse
import json
from pathlib import Path
import shutil
from urllib.request import urlopen
import zipfile
from prepare import digest


def setup(target, pitch=False):
    source = Path(__file__).resolve().parent
    target = target.resolve()
    target.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((source / 'community-downloads.json').read_text())
    for item in manifest['downloads']:
        if item['group'] == 'pitch' and not pitch:
            continue
        path = target / item['path']
        if not path.resolve().is_relative_to(target):
            raise ValueError('Download path leaves the target')
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists() or digest(path) != item['sha256']:
            part = path.with_suffix(path.suffix + '.part')
            print('Downloading', item['path'], flush=True)
            with urlopen(item['url'], timeout=60) as response, part.open('wb') as out:
                shutil.copyfileobj(response, out)
            if digest(part) != item['sha256']:
                raise ValueError('Checksum mismatch: ' + item['path'])
            part.replace(path)
    # Preserve the host's licenses and dependency files from the verified archive.
    folder = target / 'sonic'
    with zipfile.ZipFile(target / 'sonic-annotator.zip') as archive:
        for member in archive.infolist():
            path = folder / member.filename
            if not path.resolve().is_relative_to(folder.resolve()):
                raise ValueError('Archive path leaves the target')
        archive.extractall(folder)
    shutil.copyfile(source / 'chordino-harte.ttl', target / 'chordino-harte.ttl')
    shutil.copyfile(source / 'community-downloads.json', target / 'downloads.json')
    print('Verified optional community tools at', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', type=Path, default=Path(__file__).resolve().parents[2] / 'build_real_audio/community')
    parser.add_argument('--pitch', action='store_true', help='Also prepare CREPE models for offline comparison')
    args = parser.parse_args()
    setup(args.target, args.pitch)
