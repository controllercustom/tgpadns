"""Gamepad UI data integrity tests (offline)."""
import os
import pytest

WEBPAGE = os.path.join(os.path.dirname(__file__), '..', 'webpage.h')


def _webpage_src():
    with open(WEBPAGE, 'r', encoding='utf-8') as f:
        return f.read()


def test_all_14_buttons_present(gamepad_ui_buttons):
    expected = {'*X', '*O', '*Sq', '*Tr', '*L1', '*R1', '*L2', '*R2',
                '*Sh', '*Op', '*L3', '*R3', '*Ps', '*Tp'}
    assert expected.issubset(set(gamepad_ui_buttons.keys()))


def test_no_duplicate_tokens(gamepad_ui_buttons):
    toks = list(gamepad_ui_buttons.keys())
    assert len(toks) == len(set(toks))


def test_dpad_tokens_present(dpad_tokens):
    assert set(dpad_tokens.keys()) == {'*DPAD'}


def test_axis_tokens_present(axis_tokens):
    assert set(axis_tokens.keys()) == {'*LX', '*LY', '*RX', '*RY'}


def test_button_tokens_distinct_from_dpad_and_axes(ws_button_tokens, dpad_tokens, axis_tokens):
    b = set(ws_button_tokens.keys())
    d = set(dpad_tokens.keys())
    a = set(axis_tokens.keys())
    assert b.isdisjoint(d)
    assert b.isdisjoint(a)
    assert d.isdisjoint(a)


def test_webpage_has_nintendo_labels():
    src = _webpage_src()
    assert "'B'" in src
    assert "'A'" in src
    assert "'Y'" in src
    assert "'X'" in src
    assert "'ZL'" in src
    assert "'ZR'" in src
    assert "'Cap'" in src
    assert "'Home'" in src
