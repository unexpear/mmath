"""Blind, causal replay of the desktop pYIN and copied f3 v1.1.3 native core."""
import argparse
from bisect import bisect_right
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess

from evaluate import parse_guitar, pitch_reference

ENGINES = ("desktop", "f3")


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def median(values):
    return statistics.median(values) if values else None


def latest(rows, time):
    i = bisect_right(rows, time + 1e-10, key=lambda r: r[0]) - 1
    return rows[i] if i >= 0 else None


def reference_at(ref, time, left):
    """Both engines' current input windows must lie inside ONE annotated note."""
    active = [(i, n) for i, n in enumerate(ref["notes"]) if n[0] < time and n[1] > left]
    if len(active) != 1:
        return None
    i, note = active[0]
    if note[0] > left or note[1] < time:
        return None
    hz = pitch_reference(time, 1e-9, ref["notes"], ref["contours"])
    return (i, hz) if hz is not None else None


def error(row, hz):
    return 1200 * math.log2(row[2] / hz) if row and row[3] and row[2] > 0 else None


def summarize(points, raw, duration):
    result = {}
    for engine in ENGINES:
        errors = [p[engine] for p in points if p[engine] is not None]
        changes = [abs(b[engine] - a[engine]) for a, b in zip(points, points[1:])
                   if a["note"] == b["note"] and b["time"] - a["time"] < .1
                   and a[engine] is not None and b[engine] is not None]
        n = len(points)
        ratio = lambda x: x / n if n else None
        result[engine] = {
            "eligibleFrames": n, "voicedFrames": len(errors),
            "correct5c": sum(abs(e) <= 5 for e in errors),
            "correct10c": sum(abs(e) <= 10 for e in errors),
            "correct50c": sum(abs(e) <= 50 for e in errors),
            "accuracy5c": ratio(sum(abs(e) <= 5 for e in errors)),
            "accuracy10c": ratio(sum(abs(e) <= 10 for e in errors)),
            "accuracy50c": ratio(sum(abs(e) <= 50 for e in errors)),
            "missedVoicingRate": ratio(n - len(errors)),
            "octaveErrors": sum(abs(e) >= 1150 and abs(e - 1200 * round(e / 1200)) <= 50 for e in errors),
            "medianAbsoluteCentsWhenVoiced": median([abs(e) for e in errors]),
            "medianResidualStepCents": median(changes),
            "processingMs": sum(r[6] for r in raw[engine]),
            "medianProcessingMsPerUpdate": median([r[6] for r in raw[engine]]),
            "processingSecondsPerAudioSecond": sum(r[6] for r in raw[engine]) / 1000 / duration,
        }
    return result


def score(raw, ref=None, nominal=None):
    points = []
    # Compare the latest available outputs on a common 15 Hz desktop display grid.
    # A slower detector never gets future samples or interpolation of predictions.
    for d in raw["desktop"]:
        f = latest(raw["f3"], d[0])
        if f is None:
            continue
        left = min(d[1], f[1])
        if nominal is not None:
            if left < .1 or d[0] > raw["samples"] / raw["sampleRate"] - .1:
                continue
            expected = (0, 440 * 2 ** ((nominal - 69) / 12))
        else:
            expected = reference_at(ref, d[0], left)
        if expected is None:
            continue
        note, hz = expected
        points.append({"time": d[0], "note": note, "referenceHz": hz,
                       "desktop": error(d, hz), "f3": error(f, hz)})
    metrics = summarize(points, raw, raw["samples"] / raw["sampleRate"])
    response = {e: [] for e in ENGINES}
    if ref is not None:
        # First run of three correct display ticks in an isolated note >=250ms.
        # Include failures; same-pitch repetitions cannot establish reacquisition.
        for i, n in enumerate(ref["notes"]):
            start, end, midi, _ = n
            if end - start < .25 or any(j != i and m[0] < end and m[1] > start
                                        for j, m in enumerate(ref["notes"])):
                continue
            previous = [m for m in ref["notes"] if m[1] <= start]
            if previous and round(max(previous, key=lambda m: m[1])[2]) == round(midi):
                continue
            ticks = [d[0] for d in raw["desktop"] if start <= d[0] <= end]
            for engine in ENGINES:
                streak = 0
                delay = None
                for t in ticks:
                    expected = pitch_reference(t, 1e-9, ref["notes"], ref["contours"])
                    e = error(latest(raw[engine], t), expected) if expected else None
                    streak = streak + 1 if e is not None and abs(e) <= 50 else 0
                    if streak == 3:
                        delay = (t - start) * 1000
                        break
                response[engine].append({"note": i, "delayMs": delay})
    for engine in ENGINES:
        delays = [r["delayMs"] for r in response[engine] if r["delayMs"] is not None]
        metrics[engine].update(responseEligibleNotes=len(response[engine]),
                               responseSuccessfulNotes=len(delays),
                               medianStableResponseMs=median(delays))
    return {"metrics": metrics, "points": points, "response": response}


def run(root, executable):
    target = root / "tuner-comparison"
    target.mkdir(exist_ok=True)
    manifest = json.loads((root / "manifest.json").read_text())
    tracks = [dict(t, group="GuitarSet") for t in manifest["tracks"]
              if t["kind"] == "guitar" and t["mode"] == "pitch"]
    samples = root / "reference_sources/f3-emily-guitar"
    for s in json.loads((samples / "manifest.json").read_text())["samples"]:
        tracks.append({"id": "emily-" + s["name"], "group": "Nominal notes",
                       "audio": str((samples / (s["name"] + ".wav")).relative_to(root)),
                       "audioSha256": s["sha256"], "nominal": s["midi"]})
    report = {"schema": 1, "probeSha256": digest(executable), "tracks": []}
    for track in tracks:
        audio = root / track["audio"]
        if digest(audio) != track["audioSha256"]:
            raise ValueError("Changed audio " + track["id"])
        raw_path = target / (track["id"] + ".json")
        subprocess.run([str(executable), str(audio), str(raw_path)], check=True)
        raw = json.loads(raw_path.read_text())
        ref = None
        if "reference" in track:
            path = root / track["reference"]
            if digest(path) != track["referenceSha256"]:
                raise ValueError("Changed reference " + track["id"])
            ref = parse_guitar(path)
        scored = score(raw, ref, track.get("nominal"))
        report["tracks"].append(dict(track, **scored, duration=raw["samples"] / raw["sampleRate"],
                                      rawSha256=digest(raw_path)))
        print(track["id"], {e: scored["metrics"][e]["accuracy50c"] for e in ENGINES}, flush=True)
    report["summary"] = {}
    for group in ("GuitarSet", "Nominal notes"):
        selected = [t for t in report["tracks"] if t["group"] == group]
        points, raw = [], {e: [] for e in ENGINES}
        for track in selected:
            points.extend(dict(p, note=(track["id"], p["note"])) for p in track["points"])
            r = json.loads((target / (track["id"] + ".json")).read_text())
            for e in ENGINES:
                raw[e].extend(r[e])
        summary = summarize(points, raw, sum(t["duration"] for t in selected))
        for e in ENGINES:
            responses = [r for t in selected for r in t["response"][e]]
            delays = [r["delayMs"] for r in responses if r["delayMs"] is not None]
            summary[e].update(responseEligibleNotes=len(responses), responseSuccessfulNotes=len(delays),
                              medianStableResponseMs=median(delays))
        report["summary"][group] = summary
    project = Path(__file__).resolve().parents[2]
    report["sourceSha256"] = {str(p.relative_to(project)): digest(p) for p in
        [Path(__file__), Path(__file__).with_name("tuner_probe.cpp"), project / "app/streaming_tuner.h",
         *Path(__file__).with_name("vendor").rglob("*.cpp"), *Path(__file__).with_name("vendor").rglob("*.hpp"),
         *(project / "include/mmathen").rglob("*.h")]}
    (target / "report.json").write_text(json.dumps(report, indent=2, allow_nan=False))
    (target / "summary.json").write_text(json.dumps(report["summary"], indent=2, allow_nan=False))
    print(json.dumps(report["summary"], indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("build_real_audio"))
    parser.add_argument("--probe", type=Path, default=Path("build_real_audio/cpp/Release/tuner_probe.exe"))
    args = parser.parse_args()
    run(args.root.resolve(), args.probe.resolve())
