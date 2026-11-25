import numpy as np

from halo_analyzer.distance import LevelDistance, peak_db


def test_needs_three_observations():
    d = LevelDistance()
    d.observe("Door", -20.0, 0.0)
    d.observe("Door", -20.0, 0.1)
    assert d.estimate("Door", -20.0, 0.2) is None
    d.observe("Door", -20.0, 0.2)
    assert d.estimate("Door", -20.0, 0.3) == "near"


def test_default_reference_used():
    d = LevelDistance(defaults={"Gunshot": -10.0})
    assert d.estimate("Gunshot", -12.0, 0.0) == "near"
    assert d.estimate("Gunshot", -20.0, 0.0) == "mid"
    assert d.estimate("Gunshot", -40.0, 0.0) == "far"


def test_bins_from_reference():
    d = LevelDistance()
    for i in range(20):
        d.observe("Door", -20.0, i * 0.1)
    assert [d.estimate("Door", v, 3.0) for v in (-22.0, -30.0, -45.0, -15.0)] == ["near", "mid", "far", "near"]


def test_reference_forgets_old():
    d = LevelDistance()
    for _ in range(20):
        d.observe("Door", -10.0, 0.0)
    for _ in range(20):
        d.observe("Door", -30.0, 200.0)
    assert d.estimate("Door", -32.0, 200.0) == "near"


def test_volume_shift_invariant():
    values = [-12.0, -18.0, -25.0, -31.0, -40.0]
    def bins(shift):
        d = LevelDistance()
        for i, v in enumerate(values * 4):
            d.observe("Footstep", v + shift, i * 0.5)
        return [d.estimate("Footstep", v + shift, 10.0) for v in values]
    assert bins(0.0) == bins(-12.0)


def test_peak_db_silence():
    assert peak_db(np.zeros((48000, 2), np.float32), 48000) == -120.0


def test_peak_db_finds_loud_frame():
    x = np.zeros((48000, 2), np.float32)
    x[24000:26400] = 0.5  # 50 ms at 0.5 full scale
    assert abs(peak_db(x, 48000) - 20 * np.log10(0.5)) < 0.5
