"""The model interface: any Saved/ModelEval adapter plugs in unchanged.

An adapter is `adapters/<name>.py` with `Adapter(device)` and `predict(audio, sr)`:
  audio   float32 [samples, channels] at the device rate (usually 48 kHz); the adapter resamples itself.
          The classifier gets a mono mix [samples, 1], the spatial model the device channels.
  returns one dict or a list of dicts (one per sound) with any of
          "scores"   {label: probability}   preferred, lets the class thresholds work
          "class"    label                  used as certain when "scores" is missing
          "azimuth"  degrees, 0 front, +90 right (spatial models)
          "distance" metres (optional; otherwise distance comes from the level)
Labels are the model's own; `class_map` in analyzer.json renames them to overlay classes (protocol.CLASSES).
How far a spatial model tells front from back is `fb` in analyzer.json, per adapter name.
One model can do both jobs: pass the same name to --classifier and --spatial and it is loaded once.
"""
import importlib
import sys
from pathlib import Path

from .protocol import CLASSES


def load_adapter(name: str, modeleval_dir: Path, device: str):
    root = Path(modeleval_dir).resolve()
    if not (root / "adapters" / f"{name}.py").exists():
        raise SystemExit(f"no adapter {name!r}: expected {root / 'adapters' / (name + '.py')}")
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))  # adapters import each other as adapters._common
    return importlib.import_module(f"adapters.{name}").Adapter(device)


def class_scores(result: dict) -> dict[str, float]:
    """Per-label probabilities; adapters without the scores key count their winning label as certain."""
    if result.get("scores"):
        return dict(result["scores"])
    return {result["class"]: 1.0} if result.get("class") else {}


_warned: set[str] = set()


def to_overlay(label: str | None, class_map: dict[str, str]) -> str | None:
    """Overlay class for a model label, or None (reported once) when the overlay has no such class."""
    if label is None:
        return None
    cls = class_map.get(label, label)
    if cls in CLASSES:
        return cls
    if label not in _warned:
        _warned.add(label)
        print(f"halo_analyzer: model label {label!r} has no overlay class, add it to class_map", file=sys.stderr)
    return None


def overlay_scores(scores: dict[str, float], class_map: dict[str, str]) -> dict[str, float]:
    """Model scores renamed to overlay classes; several labels on one class keep the highest score."""
    out: dict[str, float] = {}
    for label, score in scores.items():
        cls = to_overlay(label, class_map)
        if cls is not None:
            out[cls] = max(out.get(cls, 0.0), float(score))
    return out


def as_list(result: dict | list[dict]) -> list[dict]:
    return list(result) if isinstance(result, list) else [result]
