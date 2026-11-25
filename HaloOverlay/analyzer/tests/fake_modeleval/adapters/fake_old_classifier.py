"""Test double: an adapter written before the scores key existed."""


class Adapter:
    def __init__(self, device="cpu"):
        pass

    def predict(self, audio, sr):
        return {"class": "Gunshot", "azimuth": None, "distance": None}
