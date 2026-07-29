"""Gamepad HID report structure tests (NSLite, 8-byte report)."""
import pytest


def test_14_buttons_used(button_constants):
    idxs = list(button_constants.values())
    assert len(idxs) == len(set(idxs))
    assert max(idxs) <= 13
    assert min(idxs) >= 0


def test_hat_values(hat_constants):
    # NSLite: 0 = centered, 1..8 = directions
    assert hat_constants['NS_DPAD_CENTERED'] == 0
    for v in hat_constants.values():
        assert 0 <= v <= 8


def test_stick_scaling():
    # WS -127..127 -> NSLite -32768..32767 (multiply by ~258)
    def scale(v):
        return v * 258
    assert scale(0) == 0
    assert scale(127) == 32766
    assert scale(-127) == -32766
