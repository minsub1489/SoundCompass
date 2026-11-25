"""Test double: a 360-degree spatial adapter that always answers 150 degrees, no class, no distance."""


class Adapter:
    def __init__(self, device="cpu"):
        self.calls = 0

    def predict(self, audio, sr):
        self.calls += 1
        return [{"class": None, "azimuth": 150.0, "distance": None}]
