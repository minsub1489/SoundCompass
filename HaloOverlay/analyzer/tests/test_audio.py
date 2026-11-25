import numpy as np
import soundfile as sf

from halo_analyzer.audio import FileSource, Windower


def test_file_source_blocks(tmp_path):
    data = np.random.default_rng(0).uniform(-0.5, 0.5, (24000, 2)).astype(np.float32)
    path = tmp_path / "a.wav"
    sf.write(path, data, 48000, subtype="FLOAT")
    blocks = list(FileSource(path, realtime=False))
    assert all(sr == 48000 for _, sr in blocks)
    joined = np.concatenate([b for b, _ in blocks])
    assert joined.shape == (24000, 2)
    assert np.allclose(joined, data)


def test_windower_hops():
    w = Windower(1.0, 0.1)
    rng = np.random.default_rng(1)
    stream = rng.standard_normal((48000 * 5 // 2, 2)).astype(np.float32)
    windows = []
    for i in range(250):
        windows += w.push(stream[i * 480:(i + 1) * 480], 48000)
    assert len(windows) == 16
    assert all(x.shape == (48000, 2) for x in windows)
    assert np.array_equal(windows[1][:-4800], windows[0][4800:])
    assert np.array_equal(windows[-1], stream[-48000:])


def test_channel_change_resets():
    w = Windower(1.0, 0.1)
    out = []
    for _ in range(90):
        out += w.push(np.zeros((480, 2), np.float32), 48000)
    assert out == []
    for _ in range(99):
        assert w.push(np.zeros((480, 8), np.float32), 48000) == []
    last = w.push(np.zeros((480, 8), np.float32), 48000)
    assert len(last) == 1 and last[0].shape == (48000, 8)
