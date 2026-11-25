"""Is there a new sound at all? Without this, background music and ambience would produce an event on every hop."""
from collections import deque

import numpy as np


def band_db(x: np.ndarray, sr: int, n_bands: int) -> np.ndarray:
    """Mono-summed energy per log-spaced band (100 Hz..16 kHz), in dB."""
    mono = x.mean(axis=1) if x.ndim == 2 else x
    spectrum = np.abs(np.fft.rfft(mono)) ** 2
    f = np.fft.rfftfreq(len(mono), 1 / sr)
    edges = np.geomspace(100.0, min(16000.0, 0.45 * sr), n_bands + 1)
    energy = np.array([spectrum[(f >= lo) & (f < hi)].sum() for lo, hi in zip(edges[:-1], edges[1:])])
    return 10.0 * np.log10(energy + 1e-12)


class OnsetGate:
    """Onset when any band of the newest hop rises rise_db above that band's median over the last background_s."""

    def __init__(self, rise_db: float = 9.0, background_s: float = 3.0, hop_s: float = 0.1, n_bands: int = 24):
        self.rise_db, self.hop_s, self.n_bands = rise_db, hop_s, n_bands
        self.history = deque(maxlen=max(1, int(round(background_s / hop_s))))

    def update(self, window: np.ndarray, sr: int) -> bool:
        hop = window[-int(round(self.hop_s * sr)):]
        current = band_db(hop, sr, self.n_bands)
        onset = bool(self.history) and bool(np.any(current - np.median(self.history, axis=0) >= self.rise_db))
        self.history.append(current)
        return onset


class ClassGate:
    def __init__(self, thresholds: dict[str, float] | None = None, default: float = 0.3):
        self.thresholds, self.default = dict(thresholds or {}), default

    def passes(self, cls: str, score: float) -> bool:
        return score >= self.thresholds.get(cls, self.default)
