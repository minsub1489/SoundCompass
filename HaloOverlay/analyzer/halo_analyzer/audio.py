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


def default_render_id() -> str | None:
    """Id of the current default output device straight from Windows Core Audio (IMMDeviceEnumerator).

    PortAudio enumerates devices once per Pa_Initialize and its init is reference counted, so a second
    PyAudio() opened while a stream is alive still sees the startup device list.
    """
    import ctypes

    ole32 = ctypes.windll.ole32
    ole32.CoInitializeEx(None, 0)  # S_FALSE or RPC_E_CHANGED_MODE are fine: COM is usable either way
    clsid, iid = (ctypes.c_byte * 16)(), (ctypes.c_byte * 16)()
    ole32.CLSIDFromString("{BCDE0395-E52F-467C-8E3D-C4579291692E}", ctypes.byref(clsid))  # MMDeviceEnumerator
    ole32.CLSIDFromString("{A95664D2-9614-4F35-A746-DE8DB63617E6}", ctypes.byref(iid))    # IMMDeviceEnumerator
    enumerator, device, text = ctypes.c_void_p(), ctypes.c_void_p(), ctypes.c_wchar_p()

    def method(obj, index, *args):
        vtable = ctypes.cast(obj, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        return ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, *args)(vtable[index])

    try:
        if ole32.CoCreateInstance(ctypes.byref(clsid), None, 23, ctypes.byref(iid), ctypes.byref(enumerator)) != 0:
            return None
        # IMMDeviceEnumerator::GetDefaultAudioEndpoint(eRender, eConsole, &device), then IMMDevice::GetId(&text)
        if method(enumerator, 4, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)(enumerator, 0, 0, ctypes.byref(device)) != 0:
            return None
        if method(device, 5, ctypes.c_void_p)(device, ctypes.byref(text)) != 0:
            return None
        return text.value
    finally:
        if text:
            ole32.CoTaskMemFree(text)
        for obj in (device, enumerator):
            if obj:
                method(obj, 2)(obj)  # Release


class LoopbackSource:
    """Default render device through WASAPI loopback (PyAudioWPatch).

    Reopened when the default device changes (checked every check_s through Core Audio) or a read fails
    (WASAPI invalidates the stream when the device or its format changes). Each reopen tears PortAudio down
    completely, so the new instance enumerates devices again.
    """

    def __init__(self, block_s: float = 0.01, check_s: float = 1.0, retry_s: float = 0.5, backend=None,
                 default_id=default_render_id):
        self.block_s, self.check_s, self.retry_s = block_s, check_s, retry_s
        self.backend, self.default_id = backend, default_id

    def __iter__(self) -> Iterator[tuple[np.ndarray, int]]:
        pyaudio = self.backend
        if pyaudio is None:
            import pyaudiowpatch as pyaudio

        while True:
            device_id = self.default_id()
            pa = pyaudio.PyAudio()
            stream = None
            try:
                device = pa.get_default_wasapi_loopback()
                sr, channels = int(device["defaultSampleRate"]), int(device["maxInputChannels"])
                block = max(1, int(round(self.block_s * sr)))
                stream = pa.open(format=pyaudio.paFloat32, channels=channels, rate=sr, input=True,
                                 input_device_index=device["index"], frames_per_buffer=block)
                checked = time.perf_counter()
                while True:
                    raw = stream.read(block, exception_on_overflow=False)
                    yield np.frombuffer(raw, dtype=np.float32).reshape(-1, channels), sr
                    if time.perf_counter() - checked >= self.check_s:
                        checked = time.perf_counter()
                        if self.default_id() != device_id:
                            break
            except OSError:
                time.sleep(self.retry_s)  # device gone or format changed: reopen whatever is default now
            finally:
                if stream is not None:
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
