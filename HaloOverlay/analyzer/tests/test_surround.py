import numpy as np
import pytest

from halo_analyzer.surround import LAYOUT_51, LAYOUT_71, layout_for, surround_peaks
from synth import noise, pan_71


def err(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


@pytest.mark.parametrize("az", [0.0, 45.0, 90.0, 120.0, -135.0, 180.0])
def test_single_source_angles(az):
    peaks = surround_peaks(pan_71(noise(), az, LAYOUT_71), 48000)
    assert peaks and err(peaks[0][0], az) <= 10.0


def test_51_rear():
    peaks = surround_peaks(pan_71(noise(), 110.0, LAYOUT_51), 48000)
    assert peaks and err(peaks[0][0], 110.0) <= 10.0


def test_two_sources_two_peaks():
    low = pan_71(noise(seed=1, band=(100, 1000)), -90.0, LAYOUT_71)
    high = pan_71(noise(seed=2, band=(4000, 12000)), 60.0, LAYOUT_71)
    peaks = surround_peaks(low + high, 48000)
    assert len(peaks) >= 2
    found = sorted(p[0] for p in peaks[:2])
    assert err(found[0], -90.0) <= 15.0 and err(found[1], 60.0) <= 15.0


def test_silence():
    assert surround_peaks(np.zeros((48000, 8), np.float32), 48000) == []


@pytest.mark.parametrize("sr", [44100, 96000])
def test_sample_rate_independent(sr):
    peaks = surround_peaks(pan_71(noise(sr=sr), 90.0, LAYOUT_71), sr)
    assert peaks and err(peaks[0][0], 90.0) <= 10.0


def test_lfe_ignored():
    x = np.zeros((48000, 8), np.float32)
    x[:, 3] = noise()
    assert surround_peaks(x, 48000) == []


def test_layout_for():
    assert layout_for(8) is LAYOUT_71 and layout_for(6) is LAYOUT_51
    assert layout_for(2) is None
