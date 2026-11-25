"""Distance bin from loudness: games reuse the same assets, so once the class is known, quieter means farther.

The reference is relative to this session's own loud examples of the class, so system and game volume cancel out.
shortcut: bins assume ~6 dB per doubling of distance; recalibrate near_db/mid_db per game when calibration exists.
"""
from collections import defaultdict, deque

import numpy as np


def peak_db(window: np.ndarray, sr: int, frame_s: float = 0.05) -> float:
    """Loudest 50 ms RMS in the window, dBFS; -120 for silence."""
    x = window if window.ndim == 2 else window[:, None]
    frame = max(1, int(round(frame_s * sr)))
    n = len(x) // frame * frame
    if n == 0:
        return -120.0
    ms = (x[:n] ** 2).mean(axis=1).reshape(-1, frame).mean(axis=1)
    return float(max(-120.0, 10.0 * np.log10(ms.max() + 1e-12)))


class LevelDistance:
    def __init__(self, defaults: dict[str, float] | None = None, near_db: float = 6.0, mid_db: float = 18.0,
                 window_s: float = 180.0, min_obs: int = 3, percentile: float = 95.0):
        self.defaults = dict(defaults or {})
        self.near_db, self.mid_db, self.window_s, self.min_obs, self.percentile = near_db, mid_db, window_s, min_obs, percentile
        self.seen: dict[str, deque] = defaultdict(deque)

    def observe(self, cls: str, level_db: float, t: float) -> None:
        self.seen[cls].append((t, level_db))

    def estimate(self, cls: str, level_db: float, t: float) -> str | None:
        seen = self.seen[cls]
        while seen and t - seen[0][0] > self.window_s:
            seen.popleft()
        if len(seen) >= self.min_obs:
            reference = float(np.percentile([db for _, db in seen], self.percentile))
        elif cls in self.defaults:
            reference = self.defaults[cls]
        else:
            return None
        below = reference - level_db
        return "near" if below <= self.near_db else ("mid" if below <= self.mid_db else "far")
