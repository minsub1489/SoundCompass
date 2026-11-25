"""Message format v1 to the overlay (design doc section 6)."""
import json
import socket
from dataclasses import asdict, dataclass

CLASSES = ("Alarm", "Dog", "Door", "Explosion", "Fire", "Footstep", "GlassBreak", "Gunshot", "Impact", "Item",
           "Reload", "Throwable", "Vehicle", "Voice")


@dataclass
class Event:
    id: int
    cls: str
    conf: float
    az: float
    fb: float
    level: float
    dist_m: float | None = None
    dist_bin: str | None = None
    dist_src: str | None = None
    elev_m: float | None = None


def encode(events: list[Event], mode: str, t: float) -> bytes:
    out = []
    for event in events:
        if event.cls not in CLASSES:
            raise ValueError(f"unknown class {event.cls!r}")
        row = asdict(event)
        out.append({k: row[k] for k in ("id", "cls", "conf", "az", "fb", "dist_m", "dist_bin", "dist_src", "elev_m", "level")})
    return json.dumps({"v": 1, "src": "model", "mode": mode, "t": t, "events": out}).encode("utf-8")


class Sender:
    def __init__(self, ports: list[int]):
        self.ports = list(ports)
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def send(self, payload: bytes) -> None:
        for port in self.ports:
            try:
                self.socket.sendto(payload, ("127.0.0.1", port))
            except OSError:
                pass  # nobody listening is fine; the overlay may start later
