"""Download pinned public corpora; freeze a small, score-independent sample.

Python 3.11+. Large archives and generated files stay in build_real_audio/.
Nothing is sent to an external service. Downloads are checked before use.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import tarfile
from urllib.request import urlopen
import zipfile

ARCHIVES = {
    "annotation.zip": ("https://zenodo.org/api/records/3371780/files/annotation.zip/content",
                       "b39b78e63d3446f2e54ddb7a54df9b10"),
    "audio_mono-mic.zip": ("https://zenodo.org/api/records/3371780/files/audio_mono-mic.zip/content",
                           "275966d6610ac34999b58426beb119c3"),
    "test-clean.tar.gz": ("https://openslr.trmal.net/resources/12/test-clean.tar.gz",
                          "32fa31d27d2e1cad72775fee3f4849a9"),
}
KNOWN_ISSUES = {"04_BN3-154-E_comp", "04_Jazz1-200-B_comp", "02_Funk2-119-G_comp"}


def digest(path, algorithm="sha256"):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, algorithm).hexdigest()


def download(path, url, md5):
    if path.exists() and digest(path, "md5") == md5:
        return
    print(f"Downloading {path.name}", flush=True)
    part = path.with_suffix(".part")
    with urlopen(url, timeout=60) as src, part.open("wb") as dst:
        shutil.copyfileobj(src, dst, 1024 * 1024)
    if digest(part, "md5") != md5:
        raise ValueError(f"Checksum mismatch: {path.name}")
    part.replace(path)


def select_guitar(names):
    names = sorted(Path(n).stem for n in names if n.endswith(".jams") and "/" not in n)
    selected = []
    for player in range(6):
        # Rotate progressions across players: all 5 styles, all 3 progressions.
        for style in ("BN", "Funk", "Jazz", "Rock", "SS"):
            prefix = f"{player:02d}_{style}{player % 3 + 1}-"
            choices = [n for n in names if n.startswith(prefix) and n.endswith("_comp")
                       and n not in KNOWN_ISSUES]
            selected.append((choices[0], "chords"))
        prefix = f"{player:02d}_Rock1-"
        selected.append((next(n for n in names if n.startswith(prefix) and n.endswith("_solo")), "pitch"))
    return selected


def prepare(root, fetch=False):
    import numpy as np
    import soundfile as sf
    root.mkdir(parents=True, exist_ok=True)
    for name, (url, md5) in ARCHIVES.items():
        path = root / name
        if fetch:
            download(path, url, md5)
        if not path.exists() or digest(path, "md5") != md5:
            raise ValueError(f"Missing/invalid {path}; run with --download")
    for folder in ("audio", "reference", "raw"):
        (root / folder).mkdir(exist_ok=True)
    tracks = []
    with zipfile.ZipFile(root / "annotation.zip") as az, zipfile.ZipFile(root / "audio_mono-mic.zip") as wz:
        wav_names = {Path(n).name: n for n in wz.namelist() if not n.startswith("__MACOSX")}
        for name, mode in select_guitar(az.namelist()):
            ref = root / "reference" / (name + ".jams")
            ref.write_bytes(az.read(name + ".jams"))
            audio = root / "audio" / (name + ".wav")
            audio.write_bytes(wz.read(wav_names[name + "_mic.wav"]))
            info = sf.info(audio)
            if info.channels != 1 or info.samplerate != 44100:
                raise ValueError(f"Unexpected GuitarSet format: {info}")
            tracks.append({"id": name, "kind": "guitar", "mode": mode,
                           "audio": audio.relative_to(root).as_posix(),
                           "reference": ref.relative_to(root).as_posix(),
                           "duration": info.duration, "audioSha256": digest(audio),
                           "referenceSha256": digest(ref)})
    with tarfile.open(root / "test-clean.tar.gz", "r:gz") as tar:
        files = sorted(n for n in tar.getnames() if n.endswith(".flac"))
        speakers = sorted({Path(n).parts[-3] for n in files}, key=int)[:6]
        for speaker in speakers:
            member = next(n for n in files if Path(n).parts[-3] == speaker)
            name = Path(member).stem
            chapter = Path(member).parent
            text_name = str(chapter / ("-".join(name.split("-")[:2]) + ".trans.txt")).replace("\\", "/")
            with tar.extractfile(text_name) as src:
                transcripts = dict(line.split(" ", 1) for line in src.read().decode().splitlines())
            with tar.extractfile(member) as src:
                original = src.read()
            # Lossless FLAC -> PCM16 WAV: verify the integer samples after writing.
            samples, rate = sf.read(io.BytesIO(original), dtype="int16")
            audio = root / "audio" / (name + ".wav")
            sf.write(audio, samples, rate, subtype="PCM_16")
            restored, restored_rate = sf.read(audio, dtype="int16")
            if rate != 16000 or samples.ndim != 1 or restored_rate != rate or not np.array_equal(samples, restored):
                raise ValueError("Speech conversion did not preserve samples")
            ref = root / "reference" / (name + ".txt")
            ref.write_text(transcripts[name] + "\n", encoding="utf-8")
            tracks.append({"id": name, "kind": "speech", "mode": "pitch",
                           "audio": audio.relative_to(root).as_posix(),
                           "reference": ref.relative_to(root).as_posix(),
                           "duration": len(samples) / rate, "audioSha256": digest(audio),
                           "sourceFlacSha256": hashlib.sha256(original).hexdigest(),
                           "referenceSha256": digest(ref)})
        for name in ("LICENSE.TXT", "README.TXT", "SPEAKERS.TXT"):
            with tar.extractfile("LibriSpeech/" + name) as src:
                (root / "reference" / name).write_bytes(src.read())
    manifest = {
        "schema": 1,
        "selection": "Before scoring: six GuitarSet players x five styles; progression 1+(player modulo 3), first lexicographic comp after documented issue exclusions. One first Rock1 solo per player. LibriSpeech test-clean: first utterance of each of the six numerically lowest speaker IDs.",
        "excludedKnownIssues": sorted(KNOWN_ISSUES),
        "sources": [
            {"name": "GuitarSet 1.1.0", "url": "https://zenodo.org/records/3371780", "license": "CC BY 4.0", "credit": "Qingyang Xi, Rachel Bittner, Johan Pauwels, Xuzhou Ye, Juan Pablo Bello (2018)"},
            {"name": "LibriSpeech test-clean", "url": "https://www.openslr.org/12", "license": "CC BY 4.0", "credit": "Vassil Panayotov, Guoguo Chen, Daniel Povey, Sanjeev Khudanpur (2015); LibriVox readers"}],
        "archives": {name: {"url": url, "md5": md5} for name, (url, md5) in ARCHIVES.items()},
        "tracks": tracks,
    }
    target = root / "manifest.json"
    # Freeze before any recognizer sees the audio; do not silently change a run.
    if target.exists() and json.loads(target.read_text()) != manifest:
        raise ValueError("Manifest changed; choose a new output directory for a new selection")
    target.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Frozen {len(tracks)} recordings in {target}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("build_real_audio"))
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    prepare(args.root.resolve(), args.download)
