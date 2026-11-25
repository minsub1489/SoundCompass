from pathlib import Path

import numpy as np

from halo_analyzer.adapters import as_list, class_scores, load_adapter

FAKE = Path(__file__).parent / "fake_modeleval"


def test_load_fake():
    result = load_adapter("fake_classifier", FAKE, "cpu").predict(np.zeros((48000, 1)), 48000)
    assert result["class"] == "Door" and result["azimuth"] is None and result["scores"]["Door"] == 0.9


def test_missing_scores_fallback():
    result = load_adapter("fake_old_classifier", FAKE, "cpu").predict(np.zeros((48000, 1)), 48000)
    assert class_scores(result) == {"Gunshot": 1.0}
    assert class_scores({"class": None, "azimuth": 10.0, "distance": None}) == {}


def test_as_list():
    one = {"class": "Door"}
    assert as_list(one) == [one]
    assert as_list([one, one]) == [one, one]
