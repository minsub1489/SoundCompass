"""Test signals: pairwise constant-power panning onto a surround layout."""
import numpy as np


def _wrap(a):
    return (a + 180.0) % 360.0 - 180.0


def pan_71(mono: np.ndarray, az: float, layout: dict) -> np.ndarray:
    """mono (n,) -> (n, channels): sin/cos panning between the two speakers either side of az."""
    speakers = sorted(((angle, ch) for ch, angle in layout.items() if angle is not None))
    out = np.zeros((len(mono), len(layout)), np.float32)
    for i, (a0, c0) in enumerate(speakers):
        a1, c1 = speakers[(i + 1) % len(speakers)]
        span = (a1 - a0) % 360.0 or 360.0
        offset = (az - a0) % 360.0
        if offset <= span:
            t = offset / span * (np.pi / 2)
            out[:, c0] = mono * np.cos(t)
            out[:, c1] += mono * np.sin(t)
            return out
    raise ValueError(az)


def noise(seconds=1.0, sr=48000, seed=0, band=None):
    x = np.random.default_rng(seed).standard_normal(int(seconds * sr)).astype(np.float32)
    if band:
        spectrum = np.fft.rfft(x)
        f = np.fft.rfftfreq(len(x), 1 / sr)
        spectrum[(f < band[0]) | (f > band[1])] = 0
        x = np.fft.irfft(spectrum, len(x)).astype(np.float32)
    return x / (np.abs(x).max() + 1e-9) * 0.5
