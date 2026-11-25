import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_streams import compare  # noqa: E402


def msg(src, events):
    return {"v": 1, "src": src, "mode": "ground_truth" if src == "ue" else "stereo", "t": 0, "events": events}


def ev(i, cls, az):
    return {"id": i, "cls": cls, "conf": 1, "az": az, "fb": 1, "level": 0.5}


def rec(recv, src, events):
    return {"recv": recv, "msg": msg(src, events)}


class CompareTest(unittest.TestCase):
    def test_perfect_pairing(self):
        records = []
        for k, (cls, az) in enumerate([("Gunshot", 0), ("Door", 90), ("Voice", -90)]):
            records.append(rec(10.0 * k, "ue", [ev(k, cls, az)]))
            records.append(rec(10.0 * k + 0.15, "model", [ev(100 + k, cls, az + 5)]))
        r = compare(records)
        self.assertEqual(r["recall"], 1.0)
        self.assertEqual(r["class_acc"], 1.0)
        self.assertAlmostEqual(r["az_mae"], 5.0)
        self.assertAlmostEqual(r["latency_ms_median"], 150.0)

    def test_late_is_missed(self):
        r = compare([rec(0.0, "ue", [ev(1, "Door", 0)]), rec(1.2, "model", [ev(9, "Door", 0)])])
        self.assertEqual(r["recall"], 0.0)

    def test_wrong_class_counted(self):
        r = compare([rec(0.0, "ue", [ev(1, "Door", 30)]), rec(0.2, "model", [ev(9, "Gunshot", 30)])])
        self.assertEqual(r["recall"], 1.0)
        self.assertEqual(r["class_acc"], 0.0)
        self.assertNotEqual(r["latency_ms_median"], r["latency_ms_median"])  # NaN: no same-class pair

    def test_front_back(self):
        r = compare([rec(0.0, "ue", [ev(1, "Door", 150)]), rec(0.2, "model", [ev(9, "Door", 30)])], max_az_err=180.0)
        self.assertEqual(r["front_back_rate"], 1.0)

    def test_repeated_frames_are_one_onset(self):
        records = [rec(0.03 * k, "ue", [ev(1, "Door", 0)]) for k in range(10)]
        records.append(rec(0.1, "model", [ev(5, "Door", 0)]))
        r = compare(records)
        self.assertEqual(r["n_truth"], 1)
        self.assertEqual(r["recall"], 1.0)


if __name__ == "__main__":
    unittest.main()
