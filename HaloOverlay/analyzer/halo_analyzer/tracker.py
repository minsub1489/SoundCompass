"""Stable ids: a detection continues the previous hop's track of the same class within max_gap_deg."""
from dataclasses import dataclass


@dataclass
class Detection:
    cls: str
    az: float


def _gap(a: float, b: float) -> float:
    return abs((a - b + 180.0) % 360.0 - 180.0)


class Tracker:
    def __init__(self, max_gap_deg: float = 20.0):
        self.max_gap_deg = max_gap_deg
        self.previous: list[tuple[int, Detection]] = []
        self.next_id = 1

    def assign(self, detections: list[Detection]) -> list[tuple[int, bool]]:
        """(id, is new track) per detection, in input order. Closest pairs are matched first."""
        pairs = sorted((_gap(d.az, p.az), i, j) for i, d in enumerate(detections)
                       for j, (_, p) in enumerate(self.previous) if d.cls == p.cls and _gap(d.az, p.az) <= self.max_gap_deg)
        result: list[tuple[int, bool] | None] = [None] * len(detections)
        taken = set()
        for _, i, j in pairs:
            if result[i] is None and j not in taken:
                result[i] = (self.previous[j][0], False)
                taken.add(j)
        for i, r in enumerate(result):
            if r is None:
                result[i] = (self.next_id, True)
                self.next_id += 1
        self.previous = [(r[0], d) for r, d in zip(result, detections)]
        return result
