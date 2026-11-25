"""Record halo overlay messages from a UDP port to JSON lines: {"recv": perf_counter seconds, "msg": {...}}.

Usage: py -3.12 record.py --port 47801 --out session.jsonl   (Ctrl+C to stop)
Senders (analyzer, Unreal) send to several ports, e.g. 47800 for the overlay and 47801 for this recorder.
"""
import argparse
import json
import socket
import threading
import time
from pathlib import Path


def record(port: int, out: Path, stop: threading.Event) -> None:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s, open(out, "w", encoding="utf-8") as f:
        s.bind(("127.0.0.1", port))
        s.settimeout(0.1)
        while not stop.is_set():
            try:
                data = s.recv(65536)
            except socket.timeout:
                continue
            now = time.perf_counter()
            try:
                row = {"recv": now, "msg": json.loads(data)}
            except ValueError:
                row = {"recv": now, "raw": data.decode("utf-8", "replace")}
            f.write(json.dumps(row) + "\n")
            f.flush()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=47801)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    stop = threading.Event()
    print(f"recording 127.0.0.1:{args.port} -> {args.out} (Ctrl+C to stop)")
    try:
        record(args.port, args.out, stop)
    except KeyboardInterrupt:
        stop.set()


if __name__ == "__main__":
    main()
