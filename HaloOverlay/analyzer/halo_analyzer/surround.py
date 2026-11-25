"""360-degree azimuth from 5.1/7.1 loopback without a model: energy vectors over the speaker layout."""
import numpy as np
from scipy import signal

# Windows channel order. Azimuth: 0 front, +90 right. LFE has no direction.
LAYOUT_71 = {0: -30.0, 1: 30.0, 2: 0.0, 3: None, 4: -150.0, 5: 150.0, 6: -90.0, 7: 90.0}
LAYOUT_51 = {0: -30.0, 1: 30.0, 2: 0.0, 3: None, 4: -110.0, 5: 110.0}  # channels 4/5 are back or side pairs


def layout_for(channels: int) -> dict | None:
    return {8: LAYOUT_71, 6: LAYOUT_51}.get(channels)


def surround_peaks(window: np.ndarray, sr: int, n_bands: int = 24, max_peaks: int = 3) -> list[tuple[float, float]]:
    """(azimuth, share of energy) per direction peak, strongest first. [] for silence or an unknown layout."""
    layout = layout_for(window.shape[1])
    if layout is None:
        return []
    channels = [c for c, a in layout.items() if a is not None]
    angles = np.radians([layout[c] for c in channels])
    units = np.stack([np.cos(angles), np.sin(angles)])                      # (2, speakers)

    nperseg = 1 << int(np.round(np.log2(0.021 * sr)))                         # about 21 ms
    f, _, spec = signal.stft(window[:, channels].T, fs=sr, nperseg=nperseg)  # (speakers, freqs, frames)
    power = np.abs(spec) ** 2
    edges = np.geomspace(100.0, min(16000.0, 0.45 * sr), n_bands + 1)
    band_energy = np.stack([power[:, (f >= lo) & (f < hi), :].sum(axis=1) for lo, hi in zip(edges[:-1], edges[1:])],
                           axis=1)                                            # (speakers, bands, frames)
    vectors = np.tensordot(units, band_energy, axes=(1, 0))                  # (2, bands, frames)
    weights = band_energy.sum(axis=0).ravel()
    total = weights.sum()
    if total <= 1e-10:
        return []
    azimuths = np.degrees(np.arctan2(vectors[1], vectors[0])).ravel()

    bins = 72
    index = np.floor(((azimuths + 180.0) % 360.0) / (360.0 / bins)).astype(int) % bins
    histogram = np.bincount(index, weights=weights, minlength=bins)
    smooth = sum(np.roll(histogram, k) for k in range(-2, 3))
    peaks = [i for i in range(bins)
             if smooth[i] >= smooth[i - 1] and smooth[i] > smooth[(i + 1) % bins]
             and histogram[[(i + k) % bins for k in range(-2, 3)]].sum() >= 0.1 * total]
    peaks.sort(key=lambda i: -smooth[i])

    out = []
    for i in peaks[:max_peaks]:
        # A single-bin source makes a flat smoothed plateau whose edge wins the peak test; re-centre on the raw bin.
        i = max(((i + k) % bins for k in range(-2, 3)), key=lambda j: histogram[j])
        centre = -180.0 + (i + 0.5) * 360.0 / bins
        near = np.abs((azimuths - centre + 180.0) % 360.0 - 180.0) <= 10.0
        w = weights[near]
        if w.sum() <= 0.0:
            continue
        mean = np.degrees(np.arctan2((w * np.sin(np.radians(azimuths[near]))).sum(),
                                     (w * np.cos(np.radians(azimuths[near]))).sum()))
        out.append((float(mean), float(w.sum() / total)))
    return out
