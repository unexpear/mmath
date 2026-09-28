"""Blind audio measurements, reference scoring, and a local listening report."""
import argparse
from bisect import bisect_left
from collections import Counter
import hashlib
import importlib.metadata
import json
import math
from pathlib import Path
import platform
import re
import subprocess

import mir_eval.chord as chord
import numpy as np

from prepare import digest

MODEL_ID = "Systran/faster-whisper-base.en"
MODEL_REVISION = "3d3d5dee26484f91867d81cb899cfcf72b96be6c"
ASR_OPTIONS = dict(language="en", beam_size=5, temperature=0.0,
                   condition_on_previous_text=False, vad_filter=False,
                   word_timestamps=True, initial_prompt=None, hotwords=None)
LABELS = [f"{n}:{q}" for q in ("maj", "min")
          for n in ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")] + ["N"]


def save(path, data):
    Path(path).write_text(json.dumps(data, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def ratio(numerator, denominator):
    return numerator / denominator if denominator else None


def chord_cells(raw, column):
    half_hop = raw["config"]["hop"] / raw["sampleRate"] / 2
    duration = raw["samples"] / raw["sampleRate"]
    return [[max(0, f[0] - half_hop), min(duration, f[0] + half_hop), LABELS[f[column]]]
            for f in raw["chords"]]


def validate_intervals(intervals):
    previous = -math.inf
    for start, end, _ in intervals:
        if not math.isfinite(start) or not math.isfinite(end) or start < 0 or end <= start or start < previous - 1e-8:
            raise ValueError("Invalid or overlapping chord intervals")
        previous = end


def score_chords(reference, estimates):
    """Exact overlap duration, no invented N labels or edge extrapolation."""
    validate_intervals(reference)
    validate_intervals(estimates)
    overlaps = []
    i = j = 0
    while i < len(reference) and j < len(estimates):
        a, b, label = reference[i]
        c, d, estimated = estimates[j]
        if max(a, c) < min(b, d):
            overlaps.append([max(a, c), min(b, d), label, estimated])
        if b <= d:
            i += 1
        else:
            j += 1
    if not overlaps:
        raise ValueError("No reference/estimate overlap")
    ref = [s[2] for s in overlaps]
    est = [s[3] for s in overlaps]
    weights = np.array([s[1] - s[0] for s in overlaps])
    scores = chord.majmin(ref, est)
    roots = chord.root(ref, est)
    valid = scores >= 0
    root_valid = roots >= 0
    compared = float(weights.sum())
    supported = float(weights[valid].sum())
    correct = float((weights[valid] * scores[valid]).sum())
    annotated = sum(b - a for a, b, _ in reference)
    metric = {"accuracy": ratio(correct, supported), "correctSeconds": correct,
              "scoredSeconds": supported, "comparedSeconds": compared,
              "annotatedSeconds": annotated, "coverage": ratio(supported, annotated),
              "unsupportedSeconds": compared - supported,
              "uncoveredSeconds": max(0.0, annotated - compared),
              "rootAccuracy": ratio(float((weights[root_valid] * roots[root_valid]).sum()),
                                    float(weights[root_valid].sum()))}
    # Cross-check the aggregate with the established evaluator, including exclusions.
    if supported and not math.isclose(metric["accuracy"], chord.weighted_accuracy(scores, weights), abs_tol=1e-12):
        raise AssertionError("Scorer disagrees with mir_eval")
    for s, score in zip(overlaps, scores):
        s.append(None if score < 0 else bool(score))
    return metric, overlaps


def reduce_label(label):
    matches = chord.majmin([label] * len(LABELS), LABELS)
    indices = np.flatnonzero(matches == 1)
    return LABELS[int(indices[0])] if len(indices) else None


def parse_guitar(path):
    jam = json.loads(Path(path).read_text(encoding="utf-8"))
    annotations = jam["annotations"]
    ca = [a for a in annotations if a["namespace"] == "chord"]
    performed = [a for a in ca if a["annotation_metadata"]["data_source"] ==
                 "Semi-automatic chord transcription with manual verification"]
    instructed = [a for a in ca if not a["annotation_metadata"]["data_source"]]
    if len(performed) != 1 or len(instructed) != 1:
        raise ValueError("Ambiguous GuitarSet chord provenance")
    def intervals(a):
        return [[d["time"], d["time"] + d["duration"], d["value"]] for d in a["data"]]
    notes, contours = [], {}
    for a in annotations:
        if a["namespace"] == "note_midi":
            string = int(a["annotation_metadata"]["data_source"])
            notes.extend([d["time"], d["time"] + d["duration"], d["value"], string] for d in a["data"])
        elif a["namespace"] == "pitch_contour":
            data = a["data"]
            points = list(zip(data["time"], data["value"])) if isinstance(data, dict) else [(d["time"], d["value"]) for d in data]
            contours[int(a["annotation_metadata"]["data_source"])] = sorted(points, key=lambda d: d[0])
    if len(contours) != 6:
        raise ValueError("Expected six string contours")
    return {"performed": intervals(performed[0]), "instructed": intervals(instructed[0]),
            "notes": sorted(notes), "contours": contours}


def pitch_reference(t, window_seconds, notes, contours):
    left, right = t - window_seconds / 2, t + window_seconds / 2
    active = [n for n in notes if n[0] < right and n[1] > left]
    # The entire analysis window must contain just one fully spanning note.
    if len(active) != 1 or active[0][0] > left or active[0][1] < right:
        return None
    points = contours[active[0][3]]
    pos = bisect_left(points, t, key=lambda p: p[0])
    neighbors = points[max(0, pos - 1):pos + 1]
    if not neighbors:
        return None
    time, value = min(neighbors, key=lambda p: abs(p[0] - t))
    # GuitarSet contour spacing: 256/44100 s. Never interpolate across gaps.
    if abs(time - t) > 256 / 44100 / 2 + 1e-8 or not value["voiced"] or value["frequency"] <= 0:
        return None
    return value["frequency"]


def score_pitch(raw, reference):
    points, errors = [], []
    eligible = correct = voiced = 0
    for t, hz, is_voiced, confidence in raw["pitch"]:
        expected = pitch_reference(t, raw["config"]["frameSize"] / raw["sampleRate"],
                                   reference["notes"], reference["contours"])
        cents = None
        if expected is not None:
            eligible += 1
            if is_voiced:
                voiced += 1
                cents = 1200 * math.log2(hz / expected)
                errors.append(abs(cents))
                correct += abs(cents) <= 50
        points.append([t, hz, is_voiced, expected, cents, confidence])
    return {"accuracy50c": ratio(correct, eligible), "correctFrames": correct,
            "eligibleFrames": eligible, "totalFrames": len(points),
            "coverage": ratio(eligible, len(points)), "voicedFrames": voiced,
            "voicedRecall": ratio(voiced, eligible),
            "medianAbsoluteCentsWhenVoiced": float(np.median(errors)) if errors else None}, points


def words(text):
    # Case/punctuation normalization only; retain internal apostrophes.
    return re.findall(r"[A-Z0-9]+(?:'[A-Z0-9]+)*", text.upper().replace("\u2019", "'"))


def score_words(reference, estimate):
    ref, est = words(reference), words(estimate)
    dp = [[0] * (len(est) + 1) for _ in range(len(ref) + 1)]
    for i in range(len(ref) + 1): dp[i][0] = i
    for j in range(len(est) + 1): dp[0][j] = j
    for i in range(1, len(ref) + 1):
        for j in range(1, len(est) + 1):
            dp[i][j] = min(dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                           dp[i - 1][j - 1] + (ref[i - 1] != est[j - 1]))
    i, j = len(ref), len(est)
    alignment = []
    while i or j:
        if i and j and dp[i][j] == dp[i - 1][j - 1] + (ref[i - 1] != est[j - 1]):
            alignment.append(["match" if ref[i - 1] == est[j - 1] else "substitution", ref[i - 1], est[j - 1]])
            i -= 1; j -= 1
        elif i and dp[i][j] == dp[i - 1][j] + 1:
            alignment.append(["deletion", ref[i - 1], ""]); i -= 1
        else:
            alignment.append(["insertion", "", est[j - 1]]); j -= 1
    alignment.reverse()
    counts = Counter(row[0] for row in alignment)
    return {"wer": ratio(dp[-1][-1], len(ref)), "errors": dp[-1][-1], "referenceWords": len(ref),
            "substitutions": counts["substitution"], "deletions": counts["deletion"],
            "insertions": counts["insertion"], "alignment": alignment}


def measure(root, executable, manifest, refresh=False):
    model = None
    provenance = {"executableSha256": digest(executable), "python": platform.python_version(),
                  "model": MODEL_ID, "modelRevision": MODEL_REVISION,
                  "asrOptions": ASR_OPTIONS, "device": "cpu", "computeType": "int8",
                  "packages": {p: importlib.metadata.version(p) for p in
                               ("mir_eval", "numpy", "soundfile", "faster-whisper", "ctranslate2", "av")}}
    # Predictions happen before scoring and never receive annotation content.
    for track in manifest["tracks"]:
        audio = root / track["audio"]
        if digest(audio) != track["audioSha256"]:
            raise ValueError(f"Changed audio: {track['id']}")
        target = root / "raw" / (track["id"] + ".json")
        cache_key = hashlib.sha256(json.dumps([track["audioSha256"], track["mode"], provenance], sort_keys=True).encode()).hexdigest()
        if target.exists() and not refresh and json.loads(target.read_text()).get("cacheKey") == cache_key:
            continue
        subprocess.run([str(executable), str(audio), str(target), track["mode"]], check=True)
        raw = json.loads(target.read_text())
        if track["kind"] == "speech":
            if model is None:
                from huggingface_hub import snapshot_download
                from faster_whisper import WhisperModel
                model_path = snapshot_download(MODEL_ID, revision=MODEL_REVISION, local_dir=root / "model",
                    allow_patterns=["config.json", "model.bin", "tokenizer.json", "vocabulary.txt", "README.md"])
                model = WhisperModel(model_path, device="cpu", compute_type="int8", cpu_threads=4)
            segments, _ = model.transcribe(str(audio), **ASR_OPTIONS)
            segments = list(segments)
            raw["speech"] = {"text": "".join(s.text for s in segments).strip(),
                             "words": [[w.start, w.end, w.word, w.probability]
                                       for s in segments for w in (s.words or [])]}
        raw["cacheKey"] = cache_key
        save(target, raw)
        print(f"Measured {track['id']}", flush=True)
    provenance["modelFilesSha256"] = {p.name: digest(p) for p in (root / "model").glob("*") if p.is_file()}
    project = Path(__file__).resolve().parents[2]
    provenance["sourceFilesSha256"] = {p.resolve().relative_to(project).as_posix(): digest(p)
        for p in [*Path(__file__).parent.glob("*.py"), Path(__file__).parent / "probe.cpp",
                  *(project / "include/mmathen").rglob("*.h")]}
    return provenance


def evaluate(root, executable, refresh=False):
    manifest = json.loads((root / "manifest.json").read_text())
    provenance = measure(root, executable, manifest, refresh)
    tracks = []
    for source in manifest["tracks"]:
        refpath = root / source["reference"]
        if digest(refpath) != source["referenceSha256"]:
            raise ValueError(f"Changed reference: {source['id']}")
        rawpath = root / "raw" / (source["id"] + ".json")
        raw = json.loads(rawpath.read_text())
        track = dict(source, config=raw["config"], rawSha256=digest(rawpath))
        if source["kind"] == "guitar":
            ref = parse_guitar(refpath)
            track["notes"] = ref["notes"]
            track["performed"] = ref["performed"]
            track["instructed"] = ref["instructed"]
            if source["mode"] == "chords":
                track["smoothed"] = chord_cells(raw, 2)
                track["framewise"] = chord_cells(raw, 1)
                track["metrics"], track["comparison"] = score_chords(ref["performed"], track["smoothed"])
                track["framewiseMetrics"], _ = score_chords(ref["performed"], track["framewise"])
                track["instructedMetrics"], _ = score_chords(ref["instructed"], track["smoothed"])
                track["reduced"] = {label: reduce_label(label) for _, _, label in ref["performed"]}
            else:
                track["pitchMetrics"], track["pitch"] = score_pitch(raw, ref)
        else:
            track["transcript"] = refpath.read_text().strip()
            track["speech"] = raw["speech"]
            track["wordMetrics"] = score_words(track["transcript"], raw["speech"]["text"])
            track["pitch"] = [[t, hz, voiced, None, None, confidence] for t, hz, voiced, confidence in raw["pitch"]]
        tracks.append(track)
    musical = [t for t in tracks if t["mode"] == "chords"]
    pitch = [t for t in tracks if "pitchMetrics" in t]
    speech = [t for t in tracks if t["kind"] == "speech"]
    def total(group, metric, key): return sum(t[metric][key] for t in group)
    summary = {
        "chordTracks": len(musical), "pitchTracks": len(pitch), "speechTracks": len(speech),
        "durationSeconds": sum(t["duration"] for t in tracks),
        "chordCorrectSeconds": total(musical, "metrics", "correctSeconds"),
        "chordScoredSeconds": total(musical, "metrics", "scoredSeconds"),
        "chordAnnotatedSeconds": total(musical, "metrics", "annotatedSeconds"),
        "chordUnsupportedSeconds": total(musical, "metrics", "unsupportedSeconds"),
        "chordUncoveredSeconds": total(musical, "metrics", "uncoveredSeconds"),
        "chordAccuracy": ratio(total(musical, "metrics", "correctSeconds"), total(musical, "metrics", "scoredSeconds")),
        "framewiseAccuracy": ratio(total(musical, "framewiseMetrics", "correctSeconds"), total(musical, "framewiseMetrics", "scoredSeconds")),
        "instructedAccuracy": ratio(total(musical, "instructedMetrics", "correctSeconds"), total(musical, "instructedMetrics", "scoredSeconds")),
        "pitchEligibleFrames": total(pitch, "pitchMetrics", "eligibleFrames"),
        "pitchTotalFrames": total(pitch, "pitchMetrics", "totalFrames"),
        "pitchCorrectFrames": total(pitch, "pitchMetrics", "correctFrames"),
        "pitchAccuracy50c": ratio(total(pitch, "pitchMetrics", "correctFrames"), total(pitch, "pitchMetrics", "eligibleFrames")),
        "wordErrors": total(speech, "wordMetrics", "errors"),
        "referenceWords": total(speech, "wordMetrics", "referenceWords"),
        "wordErrorRate": ratio(total(speech, "wordMetrics", "errors"), total(speech, "wordMetrics", "referenceWords")),
    }
    report = {"manifest": manifest, "provenance": provenance, "summary": summary, "tracks": tracks}
    save(root / "report.json", report)
    save(root / "summary.json", dict(summary=summary, provenance=provenance,
         tracks=[{k: v for k, v in t.items() if k in ("id", "kind", "mode", "metrics", "pitchMetrics", "wordMetrics", "framewiseMetrics", "instructedMetrics", "rawSha256", "audioSha256", "referenceSha256")} for t in tracks]))
    from export_desktop import export_desktop
    export_desktop(root, report)
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("build_real_audio"))
    parser.add_argument("--probe", type=Path, default=Path("build_real_audio/cpp/Release/real_audio_probe.exe"))
    parser.add_argument("--refresh", action="store_true")
    args = parser.parse_args()
    evaluate(args.root.resolve(), args.probe.resolve(), args.refresh)
