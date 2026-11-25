from halo_analyzer.tracker import Detection, Tracker


def test_continuation():
    t = Tracker()
    (first, new1), = t.assign([Detection("Gunshot", 30.0)])
    (second, new2), = t.assign([Detection("Gunshot", 40.0)])
    assert first == second and new1 and not new2


def test_wrap():
    t = Tracker()
    (a, _), = t.assign([Detection("Door", 175.0)])
    (b, new), = t.assign([Detection("Door", -175.0)])
    assert a == b and not new


def test_gap_breaks():
    t = Tracker()
    (a, _), = t.assign([Detection("Door", 0.0)])
    assert t.assign([]) == []
    (b, new), = t.assign([Detection("Door", 0.0)])
    assert a != b and new


def test_class_differs():
    t = Tracker()
    (a, _), = t.assign([Detection("Door", 0.0)])
    (b, new), = t.assign([Detection("Gunshot", 0.0)])
    assert a != b and new


def test_two_same_class():
    t = Tracker()
    left, right = t.assign([Detection("Footstep", -60.0), Detection("Footstep", 60.0)])
    moved = t.assign([Detection("Footstep", 55.0), Detection("Footstep", -65.0)])
    assert moved[0][0] == right[0] and moved[1][0] == left[0]
    assert not moved[0][1] and not moved[1][1]
