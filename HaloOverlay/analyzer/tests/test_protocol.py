import json
import socket

import pytest

from halo_analyzer.protocol import Event, Sender, encode


def test_encode_matches_spec():
    event = Event(17, "Gunshot", 0.82, 35.0, 0.2, 0.7, dist_bin="mid", dist_src="level")
    message = json.loads(encode([event], "stereo", 1234.5))
    assert message == {
        "v": 1, "src": "model", "mode": "stereo", "t": 1234.5,
        "events": [{"id": 17, "cls": "Gunshot", "conf": 0.82, "az": 35.0, "fb": 0.2, "dist_m": None,
                    "dist_bin": "mid", "dist_src": "level", "elev_m": None, "level": 0.7}],
    }


def test_encode_rejects_unknown_class():
    with pytest.raises(ValueError):
        encode([Event(1, "Other", 1.0, 0.0, 0.0, 0.5)], "stereo", 0.0)


def test_sender_reaches_all_ports():
    receivers = []
    for port in (47861, 47862):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("127.0.0.1", port))
        s.settimeout(2)
        receivers.append(s)
    try:
        Sender([47861, 47862]).send(b"x")
        assert [s.recv(16) for s in receivers] == [b"x", b"x"]
    finally:
        for s in receivers:
            s.close()
