"""Compare Unreal ground truth with the analyzer's events from one record.py session.

Usage: py -3.12 compare_streams.py session.jsonl [--window 1.0] [--max-az-err 30]
Each ground-truth onset (first message with a new ue id) is paired with the first unused model onset that arrives
0..window s later within max-az-err degrees (for a folded stereo azimuth, fb < 0.5, its front/back mirror counts too).
Latency counts only pairs whose class also matches.
"""
import argparse
import json
import math
import statistics
from pathlib import Path


def _wrap(a):
    return (a + 180.0) % 360.0 - 180.0


def _front_back(pred, truth, lateral_margin=15.0):
    """Same rule as Saved/ModelEval/evaluate.py: closer to the front/back mirror than to the truth."""
    if abs(abs(_wrap(truth)) - 90.0) <= lateral_margin:
        return None
    mirror = _wrap(180.0 - truth)
    return abs(_wrap(pred - mirror)) < abs(_wrap(pred - truth))


def _onsets(records, src):
    seen, out = set(), []
    for r in records:
        m = r.get("msg")
        if not m or m.get("src") != src:
            continue
        for e in m.get("events", []):
            if e["id"] not in seen:
                seen.add(e["id"])
                out.append({"recv": r["recv"], "cls": e["cls"], "az": float(e["az"]), "fb": float(e.get("fb", 1.0))})
    return out


def compare(records: list[dict], window_s: float = 1.0, max_az_err: float = 30.0) -> dict:
    truth, model = _onsets(records, "ue"), _onsets(records, "model")
    used, pairs = set(), []
    for g in truth:
        for i, m in enumerate(model):
            # A folded (front/back unknown) azimuth also matches through its mirror; _front_back then counts it.
            near = min(abs(_wrap(m["az"] - g["az"])), abs(_wrap(180.0 - m["az"] - g["az"])) if m["fb"] < 0.5 else 360.0)
            if i not in used and 0.0 <= m["recv"] - g["recv"] <= window_s and near <= max_az_err:
                used.add(i)
                pairs.append((g, m))
                break
    nan = float("nan")
    latencies = [(m["recv"] - g["recv"]) * 1000.0 for g, m in pairs if m["cls"] == g["cls"]]
    flips = [f for f in (_front_back(m["az"], g["az"]) for g, m in pairs) if f is not None]
    return {
        "n_truth": len(truth), "n_model": len(model), "n_matched": len(pairs),
        "recall": len(pairs) / len(truth) if truth else nan,
        "class_acc": sum(m["cls"] == g["cls"] for g, m in pairs) / len(pairs) if pairs else nan,
        "az_mae": statistics.fmean(abs(_wrap(m["az"] - g["az"])) for g, m in pairs) if pairs else nan,
        "front_back_rate": sum(flips) / len(flips) if flips else nan,
        "latency_ms_median": statistics.median(latencies) if latencies else nan,
        "latency_ms_p90": sorted(latencies)[min(len(latencies) - 1, math.ceil(0.9 * len(latencies)) - 1)] if latencies else nan,
    }


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", type=Path)
    ap.add_argument("--window", type=float, default=1.0)
    ap.add_argument("--max-az-err", type=float, default=30.0)
    args = ap.parse_args()
    records = [json.loads(line) for line in args.path.read_text(encoding="utf-8").splitlines() if line.strip()]
    result = compare(records, args.window, args.max_az_err)
    width = max(map(len, result))
    for key, value in result.items():
        print(f"{key:<{width}}  {value:.3f}" if isinstance(value, float) else f"{key:<{width}}  {value}")


if __name__ == "__main__":
    main()
