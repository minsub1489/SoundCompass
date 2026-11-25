"""Test double: always Door, with class scores (the new ModelEval key)."""


class Adapter:
    name = "fake classifier"

    def __init__(self, device="cpu"):
        self.calls = 0

    def predict(self, audio, sr):
        self.calls += 1
        return {"class": "Door", "azimuth": None, "distance": None,
                "scores": {"Door": 0.9, "Gunshot": 0.35, "Voice": 0.1}}
