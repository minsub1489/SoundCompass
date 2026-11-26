from pathlib import Path

import numpy as np

import pytest

from halo_analyzer.adapters import as_list, class_scores, load_adapter, overlay_scores, to_overlay

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


def test_overlay_scores_maps_labels():
    mapping = {"Whoosh": "Throwable", "Hiss": "Throwable"}
    scores = overlay_scores({"Whoosh": 0.2, "Hiss": 0.6, "Door": 0.5, "Laser": 0.9}, mapping)
    assert scores == {"Throwable": 0.6, "Door": 0.5}  # highest label wins, unknown label dropped
    assert to_overlay(None, mapping) is None


def test_missing_adapter_names_the_path():
    with pytest.raises(SystemExit, match="no_such_model"):
        load_adapter("no_such_model", FAKE, "cpu")
