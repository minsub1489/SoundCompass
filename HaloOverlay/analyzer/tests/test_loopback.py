import itertools
import sys
from types import SimpleNamespace

import numpy as np
import pytest

from halo_analyzer.audio import LoopbackSource, default_render_id


class FakeWorld:
    """The default output device and what PortAudio would report for it; reads can be made to fail."""

    def __init__(self):
        self.device = {"index": 1, "name": "Headphones [Loopback]", "defaultSampleRate": 48000.0, "maxInputChannels": 2}
        self.device_id = "headphones"
        self.fail_next_read = False
        self.opened = []

    def backend(self):
        world = self

        class Stream:
            def __init__(self, channels):
                self.channels = channels

            def read(self, frames, exception_on_overflow=False):
                if world.fail_next_read:
                    world.fail_next_read = False
                    raise OSError(-9999, "Unanticipated host error")
                return np.full(frames * self.channels, 0.25, np.float32).tobytes()

            def close(self):
                pass

        class PyAudio:
            def __init__(self):
                self.device = dict(world.device)  # snapshot at init, like PortAudio's device cache

            def get_default_wasapi_loopback(self):
                return self.device

            def open(self, format, channels, rate, input, input_device_index, frames_per_buffer):
                world.opened.append((self.device["name"], channels, rate))
                return Stream(channels)

            def terminate(self):
                pass

        return SimpleNamespace(PyAudio=PyAudio, paFloat32=1)


def test_reopens_when_default_device_changes():
    world = FakeWorld()
    source = LoopbackSource(check_s=0.0, retry_s=0.0, backend=world.backend(), default_id=lambda: world.device_id)
    blocks = iter(source)
    assert next(blocks)[0].shape[1] == 2
    world.device = {"index": 7, "name": "Speakers 7.1 [Loopback]", "defaultSampleRate": 44100.0, "maxInputChannels": 8}
    world.device_id = "speakers"
    shapes = [(b.shape[1], sr) for b, sr in itertools.islice(blocks, 5)]
    assert (8, 44100) in shapes
    assert world.opened[-1] == ("Speakers 7.1 [Loopback]", 8, 44100)


def test_read_error_reopens_instead_of_crashing():
    world = FakeWorld()
    source = LoopbackSource(check_s=60.0, retry_s=0.0, backend=world.backend(), default_id=lambda: world.device_id)
    blocks = iter(source)
    next(blocks)
    world.fail_next_read = True
    assert len(list(itertools.islice(blocks, 3))) == 3
    assert len(world.opened) == 2


@pytest.mark.skipif(sys.platform != "win32", reason="Windows Core Audio")
def test_default_render_id_is_a_device_id():
    device_id = default_render_id()
    assert device_id is None or device_id.startswith("{")
