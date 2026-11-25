"""Replay a record.py session to the overlay, keeping the original timing.

Usage: py -3.12 replay.py session.jsonl --ports 47800 [--speed 1.0]
"""
import argparse
import json
import socket
import time
from pathlib import Path


def replay(path: Path, ports: list[int], speed: float = 1.0) -> None:
    rows = [json.loads(line) for line in Path(path).read_text(encoding="utf-8").splitlines() if line.strip()]
    rows = [r for r in rows if "msg" in r]
    if not rows:
        return
    start = time.perf_counter()
    first = rows[0]["recv"]
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        for row in rows:
            delay = (row["recv"] - first) / speed - (time.perf_counter() - start)
            if delay > 0:
                time.sleep(delay)
            payload = json.dumps(row["msg"]).encode()
            for port in ports:
                s.sendto(payload, ("127.0.0.1", port))


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("path", type=Path)
    ap.add_argument("--ports", default="47800", help="comma-separated")
    ap.add_argument("--speed", type=float, default=1.0)
    args = ap.parse_args()
    replay(args.path, [int(p) for p in args.ports.split(",")], args.speed)


if __name__ == "__main__":
    main()
