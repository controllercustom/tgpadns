"""Offline tests for true multi-client (per-client) reference counting.

These mirror the per-client recompute model in tgpadns.ino:

  - Buttons are OR-combined across all active clients.
  - Axes / d-pad take the value of the most recently active contributing
    client (last-writer-wins); fall back to center/zero when no client
    contributes.
  - A client connecting does NOT clear other clients' held inputs.
  - A client disconnecting (or going silent >5s) releases ONLY its own
    contributions; remaining clients keep theirs.

The recompute function below is a faithful port of recomputeAndSend() so the
offline suite validates the same behavior the firmware runs.
"""
import pytest

MAX_WS_CLIENTS = 5
NS_DPAD_CENTERED = 0


class ClientState:
    def __init__(self):
        self.active = False
        self.last_seen = 0
        self.btn = [False] * 16
        self.lx = self.ly = self.rx = self.ry = 0
        self.dpad = NS_DPAD_CENTERED
        self.axis_ts = self.dpad_ts = 0


def recompute(clients, now):
    """Return the effective gamepad state from all active clients.

    Mirrors recomputeAndSend() in tgpadns.ino.
    """
    btn_ref = [0] * 16
    for c in clients:
        if not c.active:
            continue
        for b in range(14):
            if c.btn[b]:
                btn_ref[b] += 1

    lx = ly = rx = ry = 0
    dpad = NS_DPAD_CENTERED
    best_axis = best_dpad = 0

    for c in clients:
        if not c.active:
            continue
        if c.axis_ts >= best_axis:
            best_axis = c.axis_ts
            lx, ly, rx, ry = c.lx, c.ly, c.rx, c.ry
        if c.dpad_ts >= best_dpad:
            best_dpad = c.dpad_ts
            dpad = c.dpad

    return {
        'buttons': [btn_ref[b] > 0 for b in range(14)],
        'btn_ref': btn_ref[:14],
        'lx': lx, 'ly': ly, 'rx': rx, 'ry': ry,
        'dpad': dpad,
    }


def fresh_clients():
    return [ClientState() for _ in range(MAX_WS_CLIENTS)]


def test_two_clients_both_buttons_pressed():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[1] = True
    clients[1].active = True
    clients[1].btn[2] = True
    state = recompute(clients, now=1000)
    assert state['buttons'][1] is True
    assert state['buttons'][2] is True
    assert state['btn_ref'][1] == 1
    assert state['btn_ref'][2] == 1


def test_two_clients_same_button_or_combined():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[1] = True
    clients[1].active = True
    clients[1].btn[1] = True
    state = recompute(clients, now=1000)
    assert state['buttons'][1] is True
    assert state['btn_ref'][1] == 2
    clients[0].btn[1] = False
    state = recompute(clients, now=1001)
    assert state['buttons'][1] is True
    clients[1].btn[1] = False
    state = recompute(clients, now=1002)
    assert state['buttons'][1] is False


def test_connect_does_not_clear_others():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[1] = True
    clients[1] = ClientState()
    clients[1].active = True
    state = recompute(clients, now=1000)
    assert state['buttons'][1] is True


def test_disconnect_releases_only_own_button():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[1] = True
    clients[1].active = True
    clients[1].btn[2] = True
    clients[1] = ClientState()
    state = recompute(clients, now=1000)
    assert state['buttons'][1] is True
    assert state['buttons'][2] is False


def test_axis_last_writer_wins_across_clients():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lx = 100
    clients[0].axis_ts = 100
    clients[1].active = True
    clients[1].lx = -50
    clients[1].axis_ts = 200
    state = recompute(clients, now=300)
    assert state['lx'] == -50
    clients[1] = ClientState()
    state = recompute(clients, now=301)
    assert state['lx'] == 100


def test_axis_centers_when_no_contributor():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].lx = 100
    clients[0].axis_ts = 100
    clients[0] = ClientState()
    state = recompute(clients, now=200)
    assert state['lx'] == 0
    assert state['dpad'] == NS_DPAD_CENTERED


def test_watchdog_silent_client_released_others_kept():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[1] = True
    clients[0].last_seen = 0
    clients[1].active = True
    clients[1].btn[2] = True
    clients[1].last_seen = 10000

    now = 10000
    released = False
    for c in clients:
        if not c.active:
            continue
        if now - c.last_seen > 5000:
            held = any(c.btn) or c.axis_ts or c.dpad_ts != NS_DPAD_CENTERED
            if held:
                c.btn = [False] * 16
                released = True
    assert released is True
    state = recompute(clients, now=now)
    assert state['buttons'][1] is False
    assert state['buttons'][2] is True


def _apply_watchdog(clients, now, timeout=5000):
    released = False
    for c in clients:
        if not c.active:
            continue
        if now - c.last_seen > timeout:
            held = any(c.btn) or c.axis_ts or c.dpad_ts != NS_DPAD_CENTERED
            if held:
                c.btn = [False] * 14
                c.axis_ts = c.dpad_ts = 0
                released = True
            else:
                c.last_seen = now
    return released


def test_watchdog_keeps_heartbeating_client():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True
    now = 100000
    for t in range(now, now + 20000, 2000):
        clients[0].last_seen = t
        assert _apply_watchdog(clients, t + 1) is False
    state = recompute(clients, now=now + 20000)
    assert state['buttons'][11] is True


def test_watchdog_releases_silent_client():
    clients = fresh_clients()
    clients[0].active = True
    clients[0].btn[11] = True
    clients[0].last_seen = 0
    assert _apply_watchdog(clients, 10000) is True
    state = recompute(clients, now=10000)
    assert state['buttons'][11] is False


def test_two_clients_same_button_stuck_regression():
    clients = fresh_clients()
    clients[0].active = True
    clients[1].active = True

    clients[0].btn[1] = True
    state = recompute(clients, now=1000)
    assert state['buttons'][1] is True

    clients[1].btn[1] = True
    state = recompute(clients, now=1001)
    assert state['buttons'][1] is True
    assert state['btn_ref'][1] == 2

    clients[1].btn[1] = False
    state = recompute(clients, now=1002)
    assert state['buttons'][1] is True
    assert state['btn_ref'][1] == 1

    clients[0].btn[1] = False
    state = recompute(clients, now=1003)
    assert state['buttons'][1] is False
    assert state['btn_ref'][1] == 0
