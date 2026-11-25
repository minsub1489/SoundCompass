"""Load Saved/ModelEval adapters as they are (Adapter(device).predict(audio, sr) -> dict | list[dict])."""
import importlib
import sys
from pathlib import Path


def load_adapter(name: str, modeleval_dir: Path, device: str):
    root = str(Path(modeleval_dir).resolve())
    if root not in sys.path:
        sys.path.insert(0, root)  # adapters import each other as adapters._common
    return importlib.import_module(f"adapters.{name}").Adapter(device)


def class_scores(result: dict) -> dict[str, float]:
    """Per-class probabilities; adapters without the scores key count their winning class as certain."""
    if result.get("scores"):
        return dict(result["scores"])
    return {result["class"]: 1.0} if result.get("class") else {}


def as_list(result: dict | list[dict]) -> list[dict]:
    return list(result) if isinstance(result, list) else [result]
