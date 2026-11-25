import json
import socket
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

from halo_analyzer.adapters import load_adapter
from halo_analyzer.pipeline import Analyzer, AnalyzerConfig
from halo_analyzer.surround import LAYOUT_71
from synth import noise, pan_71

FAKE = Path(__file__).parent / "fake_modeleval"
SR = 48000


def make(spatial=True, **config):
    classifier = load_adapter("fake_classifier", FAKE, "cpu")
    spatial_adapter = load_adapter("fake_spatial", FAKE, "cpu") if spatial else None
    return Analyzer(classifier, spatial_adapter, "fake_spatial" if spatial else None, AnalyzerConfig(**config)), classifier, spatial_adapter


def burst(channels, az=120.0, seed=0):
    """1 s window: quiet, then a loud burst in the last 0.2 s."""
    mono = np.zeros(SR, np.float32)
    mono[-int(0.2 * SR):] = noise(0.2, seed=seed)
    if channels == 8:
        return pan_71(mono, az, LAYOUT_71)
    return np.repeat(mono[:, None], channels, axis=1)


def feed(analyzer, windows):
    return [analyzer.process(w, SR, i * 0.1) for i, w in enumerate(windows)]


def silence(channels, n=3):
    return [np.zeros((SR, channels), np.float32)] * n


def test_silence_emits_nothing():
    analyzer, classifier, _ = make()
    assert all(events == [] for _, events in feed(analyzer, silence(2, 30)))
    assert classifier.calls == 0


def test_surround_event():
    analyzer, _, _ = make(spatial=False)
    mode, events = feed(analyzer, silence(8) + [burst(8, 120.0)])[-1]
    assert mode == "surround" and len(events) == 1
    e = events[0]
    assert e.cls == "Door" and abs(e.az - 120.0) <= 10.0 and e.fb == 1.0


def test_stereo_folds_back():
    analyzer, _, _ = make()
    _, events = feed(analyzer, silence(2) + [burst(2)])[-1]
    assert len(events) == 1 and events[0].az == 30.0 and events[0].fb == 0.0


def test_stereo_trusted_fb():
    analyzer, _, _ = make(fb={"fake_spatial": 1.0})
    _, events = feed(analyzer, silence(2) + [burst(2)])[-1]
    assert events[0].az == 150.0 and events[0].fb == 1.0


def test_track_keeps_distance_bin():
    analyzer, _, _ = make(spatial=False, reference_db={"Door": -10.0})
    loud = burst(8, 60.0)
    results = feed(analyzer, silence(8) + [loud] + [loud * 0.05] * 4)
    tracked = [events[0] for _, events in results if events]
    assert len(tracked) == 5
    assert len({e.id for e in tracked}) == 1
    assert len({e.dist_bin for e in tracked}) == 1 and tracked[0].dist_bin is not None


def test_mode_follows_channels():
    analyzer, _, _ = make()
    modes = [mode for mode, _ in feed(analyzer, silence(2) + [burst(2)] + silence(8) + [burst(8)])]
    assert modes[3] == "stereo" and modes[-1] == "surround"


def test_mono_device():
    analyzer, _, spatial = make()
    _, events = feed(analyzer, silence(1) + [burst(1)])[-1]
    assert len(events) == 1 and events[0].az == 0.0 and events[0].fb == 0.0
    assert spatial.calls == 0


def test_level_clipped():
    analyzer, _, _ = make(spatial=False)
    square = np.sign(np.sin(np.linspace(0, 400 * np.pi, SR))).astype(np.float32)
    window = pan_71(square, 0.0, LAYOUT_71) * 1.5
    _, events = feed(analyzer, silence(8) + [window])[-1]
    assert events and events[0].level == 1.0


def test_cli_file_input(tmp_path):
    mono = np.zeros(int(2.0 * SR), np.float32)
    mono[SR:int(1.5 * SR)] = noise(0.5)
    path = tmp_path / "burst71.wav"
    sf.write(path, pan_71(mono, -90.0, LAYOUT_71), SR, subtype="FLOAT")
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.bind(("127.0.0.1", 47871))
        s.settimeout(0.5)
        proc = subprocess.Popen([sys.executable, "-m", "halo_analyzer", "--input", str(path), "--classifier", "fake_classifier",
                                 "--spatial", "none", "--modeleval", str(FAKE), "--ports", "47871"],
                                cwd=Path(__file__).parents[1])
        messages = []
        while proc.poll() is None:
            try:
                messages.append(json.loads(s.recv(65536)))
            except socket.timeout:
                pass
        assert proc.returncode == 0
    assert any(m["mode"] == "surround" and m["events"] for m in messages)


def test_rise_db_from_config():
    analyzer, _, _ = make(rise_db=30.0)
    assert analyzer.onset.rise_db == 30.0
    # Steady noise, then a +20 dB burst: onset at the default 9 dB, none at 30 dB.
    steady = np.random.default_rng(5).standard_normal((SR, 2)).astype(np.float32) * 0.01
    loud = steady.copy()
    loud[-int(0.2 * SR):] *= 10.0
    assert feed(analyzer, [steady] * 4 + [loud])[-1][1] == []
    default, _, _ = make()
    assert feed(default, [steady] * 4 + [loud])[-1][1] != []


def test_no_onset_only_continues_tracked_classes():
    classifier = load_adapter("fake_classifier", FAKE, "cpu")
    analyzer = Analyzer(classifier, None, None, AnalyzerConfig())
    window = burst(8, 40.0)
    assert [e.cls for e in feed(analyzer, silence(8) + [window])[-1][1]] == ["Door"]
    steady = np.repeat(window[-4800:], 10, axis=0)  # same level as the burst: once it is the background, no onset
    for i in range(40):  # the Door keeps playing until it is the gate's background
        assert [e.cls for e in analyzer.process(steady, SR, 1.0 + 0.1 * i)[1]] == ["Door"]
    assert not analyzer.onset.update(steady, SR)
    classifier.predict = lambda audio, sr: {"class": "Gunshot", "scores": {"Door": 0.8, "Gunshot": 0.9}}
    _, events = analyzer.process(steady, SR, 5.0)
    assert [e.cls for e in events] == ["Door"]
