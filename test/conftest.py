"""Shared fixtures for tgpadns test suite."""
import os

import pytest

_MCLIENT_MSG = (
    "multi-client test is skipped unless enabled with TGAPDNS_MULTICLIENT=1 "
    "(verified passing on both the generic ESP32-S3 dev module and the "
    "M5Stack AtomS3 — distinct per-client slot indices on both)."
)


def pytest_collection_modifyitems(config, items):
    enabled = os.environ.get("TGAPDNS_MULTICLIENT", "") == "1"
    if enabled:
        return
    skip_mc = pytest.mark.skip(reason=_MCLIENT_MSG)
    for item in items:
        if "multiclient" in item.keywords:
            item.add_marker(skip_mc)


@pytest.fixture
def button_constants():
    """Button bit indices matching NSLiteController.h (and tgpadns.ino dispatch)."""
    return {
        'NS_BTN_Y':       0,
        'NS_BTN_B':       1,
        'NS_BTN_A':       2,
        'NS_BTN_X':       3,
        'NS_BTN_L':       4,
        'NS_BTN_R':       5,
        'NS_BTN_ZL':      6,
        'NS_BTN_ZR':      7,
        'NS_BTN_MINUS':   8,
        'NS_BTN_PLUS':    9,
        'NS_BTN_LCLICK':  10,
        'NS_BTN_RCLICK':  11,
        'NS_BTN_HOME':    12,
        'NS_BTN_CAPTURE': 13,
    }


@pytest.fixture
def hat_constants():
    """Hat switch values matching NSLiteController.h (NS_DPAD_*)."""
    return {
        'NS_DPAD_CENTERED':   0,
        'NS_DPAD_UP':         1,
        'NS_DPAD_UP_RIGHT':   2,
        'NS_DPAD_RIGHT':      3,
        'NS_DPAD_DOWN_RIGHT': 4,
        'NS_DPAD_DOWN':       5,
        'NS_DPAD_DOWN_LEFT':  6,
        'NS_DPAD_LEFT':       7,
        'NS_DPAD_UP_LEFT':    8,
    }


@pytest.fixture
def ws_button_tokens():
    """Mapping of WS button tokens -> NS_BTN_* index (from tgpadns.ino btnIndex())."""
    return {
        '*Sq': 0,   # NS_BTN_Y
        '*X':  1,   # NS_BTN_B
        '*O':  2,   # NS_BTN_A
        '*Tr': 3,   # NS_BTN_X
        '*L1': 4,   # NS_BTN_L
        '*R1': 5,   # NS_BTN_R
        '*L2': 6,   # NS_BTN_ZL
        '*R2': 7,   # NS_BTN_ZR
        '*Sh': 8,   # NS_BTN_MINUS
        '*Op': 9,   # NS_BTN_PLUS
        '*L3': 10,  # NS_BTN_LCLICK
        '*R3': 11,  # NS_BTN_RCLICK
        '*Ps': 12,  # NS_BTN_HOME
        '*Tp': 13,  # NS_BTN_CAPTURE
    }


@pytest.fixture
def dpad_tokens():
    """Directional pad WS token -> NSLite hat mapping.

    The pad is sent as a single *DPAD:<n> token where n is 0..7 (N, NE, E, SE,
    S, SW, W, NW clockwise from straight up) and 8 means centered/no-direction.
    The firmware converts: 0..7 -> 1..8 (NS_DPAD_UP..NS_DPAD_UP_LEFT), 8 -> 0 (centered).
    """
    mapping = {
        # WS value (DS4 convention) -> firmware converts to NSLite hat
        0: 1,  # N   -> NS_DPAD_UP
        1: 2,  # NE  -> NS_DPAD_UP_RIGHT
        2: 3,  # E   -> NS_DPAD_RIGHT
        3: 4,  # SE  -> NS_DPAD_DOWN_RIGHT
        4: 5,  # S   -> NS_DPAD_DOWN
        5: 6,  # SW  -> NS_DPAD_DOWN_LEFT
        6: 7,  # W   -> NS_DPAD_LEFT
        7: 8,  # NW  -> NS_DPAD_UP_LEFT
        8: 0,  # centered -> NS_DPAD_CENTERED
    }
    return {'*DPAD': mapping}


@pytest.fixture
def axis_tokens():
    """Analog axis WS tokens -> (signed?, invert-Y?). NSLite has no analog triggers."""
    return {
        '*LX': (True,  False),
        '*LY': (True,  False),
        '*RX': (True,  False),
        '*RY': (True,  False),
    }


@pytest.fixture
def gamepad_ui_buttons():
    """Button definitions rendered by webpage.h (label -> token)."""
    return {
        '*DPAD': 'DPad',
        '*X': 'B', '*O': 'A', '*Sq': 'Y', '*Tr': 'X',
        '*L1': 'L', '*R1': 'R',
        '*L2': 'ZL', '*R2': 'ZR',
        '*Sh': '−', '*Op': '+',
        '*Ps': 'Home', '*Tp': 'Cap',
        '*L3': 'L3', '*R3': 'R3',
    }
