"""python -m halo_analyzer --input loopback|<wav> --classifier efficientat_mn10 --spatial gcc_phat|none --ports 47800[,47801]"""
import argparse
import json
import time
from pathlib import Path

from .adapters import load_adapter
from .audio import FileSource, LoopbackSource, Windower
from .pipeline import Analyzer, AnalyzerConfig
from .protocol import Sender, encode

HERE = Path(__file__).resolve().parent


def resolve_device(device: str) -> str:
    if device != "auto":
        return device
    try:
        import torch
        return "cuda" if torch.cuda.is_available() else "cpu"
    except ImportError:
        return "cpu"


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", default="loopback", help="loopback or a WAV file played in real time")
    ap.add_argument("--classifier", default="efficientat_mn10")
    ap.add_argument("--spatial", default="gcc_phat", help="spatial adapter name, or none")
    ap.add_argument("--modeleval", type=Path, default=HERE.parents[2] / "Saved" / "ModelEval")
    ap.add_argument("--device", default="auto")
    ap.add_argument("--ports", default="47800")
    ap.add_argument("--config", type=Path, default=HERE.parent / "analyzer.json")
    args = ap.parse_args(argv)

    config = AnalyzerConfig(**json.loads(args.config.read_text(encoding="utf-8"))) if args.config.exists() else AnalyzerConfig()
    device = resolve_device(args.device)
    classifier = load_adapter(args.classifier, args.modeleval, device)
    spatial = None if args.spatial == "none" else load_adapter(args.spatial, args.modeleval, device)
    analyzer = Analyzer(classifier, spatial, None if spatial is None else args.spatial, config)
    source = LoopbackSource() if args.input == "loopback" else FileSource(Path(args.input))
    sender = Sender([int(p) for p in args.ports.split(",")])
    windower = Windower()
    start = time.perf_counter()
    try:
        for block, sr in source:
            for window in windower.push(block, sr):
                t = time.perf_counter() - start
                mode, events = analyzer.process(window, sr, t)
                sender.send(encode(events, mode, t))
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
