import json
import socket
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import record  # noqa: E402
import replay  # noqa: E402


def send(port, payload):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.sendto(payload, ("127.0.0.1", port))


def run_recorder(port, out):
    stop = threading.Event()
    thread = threading.Thread(target=record.record, args=(port, out, stop), daemon=True)
    thread.start()
    time.sleep(0.2)  # let it bind
    return stop, thread


class ToolsTest(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())

    def test_round_trip(self):
        out = self.dir / "rec.jsonl"
        stop, thread = run_recorder(47851, out)
        messages = [{"v": 1, "src": "ue", "mode": "ground_truth", "t": i, "events": []} for i in range(3)]
        for m in messages:
            send(47851, json.dumps(m).encode())
            time.sleep(0.1)
        time.sleep(0.1)
        stop.set()
        thread.join(2)
        lines = [json.loads(l) for l in out.read_text(encoding="utf-8").splitlines()]
        self.assertEqual([l["msg"] for l in lines], messages)
        self.assertTrue(all(a["recv"] < b["recv"] for a, b in zip(lines, lines[1:])))

    def test_replay_timing(self):
        path = self.dir / "timed.jsonl"
        rows = [{"recv": 10.0 + 0.1 * i, "msg": {"v": 1, "src": "ue", "mode": "ground_truth", "t": i, "events": []}} for i in range(3)]
        path.write_text("".join(json.dumps(r) + "\n" for r in rows), encoding="utf-8")
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.bind(("127.0.0.1", 47852))
            s.settimeout(2)
            threading.Thread(target=replay.replay, args=(path, [47852]), daemon=True).start()
            times = []
            for _ in rows:
                s.recv(65536)
                times.append(time.perf_counter())
        self.assertAlmostEqual(times[-1] - times[0], 0.2, delta=0.05)

    def test_raw_kept(self):
        out = self.dir / "raw.jsonl"
        stop, thread = run_recorder(47853, out)
        send(47853, b"{")
        time.sleep(0.2)
        stop.set()
        thread.join(2)
        line = json.loads(out.read_text(encoding="utf-8").splitlines()[0])
        self.assertEqual(line["raw"], "{")


if __name__ == "__main__":
    unittest.main()
