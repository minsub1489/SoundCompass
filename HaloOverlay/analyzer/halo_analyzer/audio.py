"""Audio input: a WAV file (tests, replays) or WASAPI loopback of the default output device, cut into windows."""
import time
from pathlib import Path
from typing import Iterator

import numpy as np
import soundfile as sf


class FileSource:
    def __init__(self, path: Path, block_s: float = 0.01, realtime: bool = True):
        self.path, self.block_s, self.realtime = Path(path), block_s, realtime

    def __iter__(self) -> Iterator[tuple[np.ndarray, int]]:
        data, sr = sf.read(str(self.path), dtype="float32", always_2d=True)
        block = max(1, int(round(self.block_s * sr)))
        start = time.perf_counter()
        for i in range(0, len(data), block):
            if self.realtime:
                delay = i / sr - (time.perf_counter() - start)
                if delay > 0:
                    time.sleep(delay)
            yield data[i:i + block], sr


class LoopbackSource:
    """Default render device through WASAPI loopback (PyAudioWPatch), reopened when the default device changes."""

    def __init__(self, block_s: float = 0.01, check_s: float = 1.0):
        self.block_s, self.check_s = block_s, check_s

    @staticmethod
    def _default_loopback(pyaudio_module):
        pa = pyaudio_module.PyAudio()  # a fresh instance sees the current default device
        try:
            return pa.get_default_wasapi_loopback()
        finally:
            pa.terminate()

    def __iter__(self) -> Iterator[tuple[np.ndarray, int]]:
        import pyaudiowpatch as pyaudio

        while True:
            device = self._default_loopback(pyaudio)
            pa = pyaudio.PyAudio()
            sr, channels = int(device["defaultSampleRate"]), int(device["maxInputChannels"])
            block = max(1, int(round(self.block_s * sr)))
            stream = pa.open(format=pyaudio.paFloat32, channels=channels, rate=sr, input=True,
                             input_device_index=device["index"], frames_per_buffer=block)
            try:
                checked = time.perf_counter()
                while True:
                    raw = stream.read(block, exception_on_overflow=False)
                    yield np.frombuffer(raw, dtype=np.float32).reshape(-1, channels), sr
                    if time.perf_counter() - checked >= self.check_s:
                        checked = time.perf_counter()
                        if self._default_loopback(pyaudio)["name"] != device["name"]:
                            break
            finally:
                stream.close()
                pa.terminate()


class Windower:
    """Overlapping windows of window_s, one every hop_s. A change of rate or channel count starts over."""

    def __init__(self, window_s: float = 1.0, hop_s: float = 0.1):
        self.window_s, self.hop_s = window_s, hop_s
        self.format = None
        self.buffer = np.zeros((0, 0), np.float32)
        self.total = 0
        self.next_emit = 0

    def push(self, block: np.ndarray, sr: int) -> list[np.ndarray]:
        block = np.asarray(block, dtype=np.float32)
        if block.ndim == 1:
            block = block[:, None]
        window, hop = int(round(self.window_s * sr)), int(round(self.hop_s * sr))
        if self.format != (sr, block.shape[1]):
            self.format = (sr, block.shape[1])
            self.buffer = np.zeros((0, block.shape[1]), np.float32)
            self.total, self.next_emit = 0, window
        self.buffer = np.concatenate([self.buffer, block])
        self.total += len(block)
        out = []
        while self.total >= self.next_emit:
            end = len(self.buffer) - (self.total - self.next_emit)  # buffer index where this window ends
            if end >= window:
                out.append(self.buffer[end - window:end].copy())
            self.next_emit += hop
        keep = window + hop
        if len(self.buffer) > keep:
            self.buffer = self.buffer[-keep:]
        return out
