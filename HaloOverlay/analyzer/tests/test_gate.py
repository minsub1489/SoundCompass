import warnings

import numpy as np

from halo_analyzer.audio import Windower
from halo_analyzer.gate import ClassGate, OnsetGate

SR = 48000


def run(signal):
    """[(window end seconds, onset?)] for a mono signal fed through 1 s / 100 ms windows."""
    gate, windower, out = OnsetGate(), Windower(), []
    x = signal.astype(np.float32)[:, None]
    for i in range(0, len(x), 480):
        for window in windower.push(x[i:i + 480], SR):
            end = (i + 480) / SR
            out.append((round(end, 2), gate.update(window, SR)))
    return out


def white(seconds, dbfs, seed=0):
    rms = 10 ** (dbfs / 20)
    return np.random.default_rng(seed).standard_normal(int(seconds * SR)) * rms


def test_steady_noise_no_onset():
    results = run(white(5, -30))
    assert not any(onset for end, onset in results if end > 1.0)


def test_burst_is_onset():
    x = white(5, -30)
    a, b = int(3.0 * SR), int(3.2 * SR)
    x[a:b] += white(0.2, -10, seed=1)
    results = run(x)
    assert any(onset for end, onset in results if 3.05 <= end <= 3.25)


def test_music_then_click():
    t = np.arange(5 * SR) / SR
    x = 0.2 * np.sin(2 * np.pi * 440 * t) + 0.2 * np.sin(2 * np.pi * 660 * t)
    a = int(3.02 * SR)
    x[a:a + 240] += np.random.default_rng(2).uniform(-0.5, 0.5, 240)
    hits = [end for end, onset in run(x) if onset and end > 1.0]
    assert hits == [3.1]


def test_silence():
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        assert not any(onset for _, onset in run(np.zeros(5 * SR)))


def test_class_gate():
    gate = ClassGate({"Gunshot": 0.5})
    assert not gate.passes("Gunshot", 0.4)
    assert gate.passes("Door", 0.31)
    assert not gate.passes("Door", 0.29)
