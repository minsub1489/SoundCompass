"""Send a looping demo scene to the overlay for a visual check (no game, no analyzer needed).

Gunshot, Footstep, Door and Voice take turns at front, right, behind and left, one per second.
Also a Reload whose front/back is unknown (drawn mirrored) and an Alarm without a distance.
Usage: py -3.12 send_demo.py --ports 47800 [--seconds 30] [--src model]
"""
import argparse
import json
import socket
import time

ROTATION = [("Gunshot", 0.0, 25.0), ("Footstep", 90.0, 4.0), ("Door", 180.0, 10.0), ("Voice", -90.0, 7.0)]


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ports", default="47800")
    ap.add_argument("--seconds", type=float, default=30.0)
    ap.add_argument("--src", default="model")
    args = ap.parse_args()
    ports = [int(p) for p in args.ports.split(",")]
    start = time.perf_counter()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        while (elapsed := time.perf_counter() - start) < args.seconds:
            slot = int(elapsed) % len(ROTATION)
            cls, az, dist = ROTATION[slot]
            pulse = 0.5 + 0.5 * abs(((elapsed * 4) % 2) - 1)
            events = [
                {"id": 100 + slot, "cls": cls, "conf": 1.0, "az": az, "fb": 1.0, "dist_m": dist, "dist_bin": None,
                 "dist_src": "ue", "elev_m": None, "level": pulse},
                {"id": 1, "cls": "Reload", "conf": 0.8, "az": 35.0, "fb": 0.0, "dist_m": None, "dist_bin": "mid",
                 "dist_src": "level", "elev_m": None, "level": 0.6},
            ]
            if int(elapsed) % 6 < 3:
                events.append({"id": 2, "cls": "Alarm", "conf": 0.9, "az": -135.0, "fb": 1.0, "dist_m": None,
                               "dist_bin": None, "dist_src": None, "elev_m": None, "level": 0.8})
            payload = json.dumps({"v": 1, "src": args.src, "mode": "stereo", "t": elapsed, "events": events}).encode()
            for port in ports:
                s.sendto(payload, ("127.0.0.1", port))
            time.sleep(0.1)


if __name__ == "__main__":
    main()
