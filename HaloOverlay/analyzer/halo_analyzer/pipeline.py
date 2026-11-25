"""One analysis window -> overlay events (design doc section 7)."""
from dataclasses import dataclass, field

import numpy as np

from .adapters import as_list, class_scores
from .distance import LevelDistance, peak_db
from .gate import ClassGate, OnsetGate
from .protocol import CLASSES, Event
from .surround import layout_for, surround_peaks
from .tracker import Detection, Tracker


@dataclass
class AnalyzerConfig:
    fb: dict[str, float] = field(default_factory=dict)            # spatial adapter name -> front/back certainty
    class_thresholds: dict[str, float] = field(default_factory=dict)
    default_threshold: float = 0.3
    max_events: int = 3
    reference_db: dict[str, float] = field(default_factory=dict)  # LevelDistance start values per class
    rise_db: float = 9.0                                          # OnsetGate: rise over the band background
    near_db: float = 6.0                                          # LevelDistance bin edges below the reference
    mid_db: float = 18.0


def _fold_front(az: float) -> float:
    return float(np.sign(az) * 180.0 - az) if abs(az) > 90.0 else float(az)


class Analyzer:
    def __init__(self, classifier, spatial, spatial_name: str | None, config: AnalyzerConfig):
        self.classifier, self.spatial, self.spatial_name, self.config = classifier, spatial, spatial_name, config
        self.onset = OnsetGate(rise_db=config.rise_db)
        self.classes = ClassGate(config.class_thresholds, config.default_threshold)
        self.distance = LevelDistance(config.reference_db, near_db=config.near_db, mid_db=config.mid_db)
        self.tracker = Tracker()
        self.bins: dict[int, str | None] = {}
        self.active = False

    def _directions(self, window, sr, mode, chosen):
        """[(az, fb, dist_m) or None] per chosen class."""
        if window.shape[1] == 1:
            return [(0.0, 0.0, None)] * len(chosen)  # mono device: no direction at all
        if mode == "surround":
            peaks = surround_peaks(window, sr)
            return [((peaks[k] if k < len(peaks) else peaks[0])[0], 1.0, None) if peaks else None for k in range(len(chosen))]
        if self.spatial is None:
            return [None] * len(chosen)
        fb = self.config.fb.get(self.spatial_name, 0.0)
        results = [r for r in as_list(self.spatial.predict(window, sr)) if r.get("azimuth") is not None]
        unclassed = [r for r in results if not r.get("class")]
        out = []
        for _, cls in chosen:
            match = next((r for r in results if r.get("class") == cls), None) or (unclassed.pop(0) if unclassed else None)
            out.append((float(match["azimuth"]), fb, match.get("distance")) if match else None)
        return out

    def process(self, window: np.ndarray, sr: int, t: float) -> tuple[str, list[Event]]:
        window = np.asarray(window, dtype=np.float32)
        mode = "surround" if layout_for(window.shape[1]) else "stereo"
        onset = self.onset.update(window, sr)
        if not onset and not self.active:
            self.tracker.assign([])
            return mode, []

        scores = class_scores(self.classifier.predict(window.mean(axis=1, keepdims=True), sr))
        # Without an onset only sounds already being tracked may continue; new classes need an onset.
        tracked = None if onset else {d.cls for _, d in self.tracker.previous}
        chosen = sorted(((s, c) for c, s in scores.items()
                         if c in CLASSES and self.classes.passes(c, s) and (tracked is None or c in tracked)), reverse=True)
        chosen = chosen[:self.config.max_events]
        kept = [(score, cls, d) for (score, cls), d in zip(chosen, self._directions(window, sr, mode, chosen)) if d]
        detections = []
        for score, cls, (az, fb, dist_m) in kept:
            detections.append((score, cls, az if fb >= 1.0 else _fold_front(az), fb, dist_m))
        ids = self.tracker.assign([Detection(cls, az) for _, cls, az, _, _ in detections])

        hop = window[-int(round(0.1 * sr)):]
        # Total power over channels: averaging would make a sound on one of eight speakers ~9 dB too quiet.
        level = float(np.clip((10.0 * np.log10(np.mean(np.sum(hop ** 2, axis=1)) + 1e-12) + 60.0) / 60.0, 0.0, 1.0))
        onset_level = peak_db(window[-int(round(0.2 * sr)):], sr)
        events = []
        for (score, cls, az, fb, dist_m), (track, new) in zip(detections, ids):
            if new and dist_m is None:
                self.distance.observe(cls, onset_level, t)
                self.bins[track] = self.distance.estimate(cls, onset_level, t)
            dist_bin = None if dist_m is not None else self.bins.get(track)
            dist_src = "model" if dist_m is not None else ("level" if dist_bin else None)
            events.append(Event(track, cls, float(score), float(az), float(fb), level,
                                dist_m=None if dist_m is None else float(dist_m), dist_bin=dist_bin, dist_src=dist_src))
        live = {track for track, _ in ids}
        self.bins = {k: v for k, v in self.bins.items() if k in live}
        self.active = bool(events)
        return mode, events
