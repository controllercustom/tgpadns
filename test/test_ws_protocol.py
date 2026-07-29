"""WebSocket protocol -> gamepad field mapping tests (offline)."""
import pytest


def test_button_token_to_index(ws_button_tokens):
    idxs = list(ws_button_tokens.values())
    assert len(idxs) == len(set(idxs))
    for k, idx in ws_button_tokens.items():
        assert k.startswith('*')
        assert 0 <= idx <= 15


def test_button_release_prefix():
    tok = '*X'
    assert '~' + tok == '~*X'


def test_dpad_conversion(dpad_tokens, hat_constants):
    # WS *DPAD:<n> converts: 0..7 -> 1..8 (NS_DPAD_*), 8 -> 0 (centered)
    mapping = dpad_tokens['*DPAD']

    # WS 0 (N) -> NS_DPAD_UP (1)
    assert mapping[0] == 1 and hat_constants['NS_DPAD_UP'] == 1
    # WS 1 (NE) -> NS_DPAD_UP_RIGHT (2)
    assert mapping[1] == hat_constants['NS_DPAD_UP_RIGHT']
    # WS 2 (E) -> NS_DPAD_RIGHT (3)
    assert mapping[2] == hat_constants['NS_DPAD_RIGHT']
    # WS 3 (SE) -> NS_DPAD_DOWN_RIGHT (4)
    assert mapping[3] == hat_constants['NS_DPAD_DOWN_RIGHT']
    # WS 4 (S) -> NS_DPAD_DOWN (5)
    assert mapping[4] == hat_constants['NS_DPAD_DOWN']
    # WS 5 (SW) -> NS_DPAD_DOWN_LEFT (6)
    assert mapping[5] == hat_constants['NS_DPAD_DOWN_LEFT']
    # WS 6 (W) -> NS_DPAD_LEFT (7)
    assert mapping[6] == hat_constants['NS_DPAD_LEFT']
    # WS 7 (NW) -> NS_DPAD_UP_LEFT (8)
    assert mapping[7] == hat_constants['NS_DPAD_UP_LEFT']
    # WS 8 -> centered (0)
    assert mapping[8] == hat_constants['NS_DPAD_CENTERED']


def test_stick_y_inversion(axis_tokens):
    _, invert = axis_tokens['*LY']
    assert invert is False
    screen_y = 100
    axis_val = -screen_y if invert else screen_y
    assert axis_val == 100
