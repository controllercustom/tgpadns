"""End-to-end hardware tests for tgpadns.

Drives the real board over its WebSocket control port (81) and observes the
USB HID gamepad output via python3-evdev. These are skipped unless a board is
reachable (set TGAPDNS_HOST, or rely on the default tgpadns.local mDNS name).

Requirements:
  - python3-evdev (system package)
  - websocket-client (in test/requirements.txt)
  - root (the harness grabs the evdev device so events don't leak to console)

Run:
  sudo TGAPDNS_HOST=192.168.1.x python3 -m pytest test/e2e -v
"""

import os
import time
import threading
import os

import pytest
import evdev
from evdev import InputDevice, ecodes

import websocket


HOST = os.environ.get('TGAPDNS_HOST') or 'tgpadns.local'
WS_URL = 'ws://%s:81/' % HOST


def btn_code(idx):
    return 304 + idx if idx < 16 else 704 + (idx - 16)


BTN_INDEX = {
    '*Sq': 0, '*X': 1, '*O': 2, '*Tr': 3, '*L1': 4, '*R1': 5, '*L2': 6,
    '*R2': 7, '*Sh': 8, '*Op': 9, '*L3': 10, '*R3': 11, '*Ps': 12, '*Tp': 13,
}


def candidate_codes(tok):
    return [btn_code(BTN_INDEX[tok])]


def _find_gamepad():
    jsdev = os.environ.get('JSDEV')
    if jsdev:
        if jsdev.startswith('/dev/input/js'):
            base = os.path.basename(jsdev)
            js_link = os.path.realpath('/sys/class/input/%s/device' % base)
            for ent in sorted(os.listdir(js_link)):
                if not ent.startswith('event'):
                    continue
                dev_path = '/dev/input/%s' % ent
                if not os.path.exists(dev_path):
                    continue
                d = InputDevice(dev_path)
                n = d.name.lower()
                if 'motion sensors' in n or 'touchpad' in n:
                    continue
                return d
        else:
            return InputDevice(jsdev)
    candidates = []
    for p in evdev.list_devices():
        d = InputDevice(p)
        n = d.name.lower()
        if 'nslite' in n or 'horipad' in n or 'nintendo' in n:
            candidates.append(d)
    if candidates:
        return candidates[0]
    for p in evdev.list_devices():
        d = InputDevice(p)
        caps = d.capabilities()
        if ecodes.EV_KEY in caps and ecodes.EV_ABS in caps:
            return d
    return None


class EventWatcher:
    def __init__(self, dev):
        self.dev = dev
        self.events = []
        self.lock = threading.Lock()
        self.stop = False
        self.t = threading.Thread(target=self._run, daemon=True)

    def _run(self):
        while not self.stop:
            try:
                for e in self.dev.read_loop():
                    with self.lock:
                        self.events.append((e.type, e.code, e.value))
                    if self.stop:
                        break
            except OSError:
                break

    def start(self):
        self.t.start()

    def stop_now(self):
        self.stop = True

    def clear(self):
        with self.lock:
            self.events = []

    def has(self, etype, code, value):
        with self.lock:
            return (etype, code, value) in self.events

    def has_any_code(self, etype, codes, value):
        with self.lock:
            return any((etype, c, value) in self.events for c in codes)

    def abs_values(self, code):
        with self.lock:
            return [v for (t, c, v) in self.events if c == code]

    def wait_for(self, etype, codes, value, timeout=4.0, poll=0.02):
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self.lock:
                if any((etype, c, value) in self.events for c in codes):
                    return True
            time.sleep(poll)
        return False


def open_ws():
    return websocket.create_connection(WS_URL, timeout=5)


from contextlib import contextmanager

@contextmanager
def using_ws():
    ws = open_ws()
    try:
        yield ws
    finally:
        try:
            ws.close()
        except Exception:
            pass


@pytest.fixture(scope='module')
def harness():
    d = _find_gamepad()
    if d is None:
        pytest.skip("No NSLite evdev device found")
    try:
        d.grab()
    except OSError:
        pytest.skip("Could not grab evdev device (need root?)")
    w = EventWatcher(d)
    w.start()
    time.sleep(0.3)
    yield (d, w)
    w.stop_now()
    try:
        d.ungrab()
    except Exception:
        pass


def _press_release(ws, w, tok, settle=0.25):
    w.clear()
    time.sleep(0.05)
    ws.send(tok)
    time.sleep(settle)
    ws.send('~' + tok)
    time.sleep(settle + 0.1)


_ALL_RELEASE = ['*Sq', '*X', '*O', '*Tr', '*L1', '*R1', '*L2', '*R2',
                '*Sh', '*Op', '*L3', '*R3', '*Ps', '*Tp',
                '*LX', '*LY', '*RX', '*RY', '*DPAD']


def neutralize(ws, w, wait=0.4):
    for tok in _ALL_RELEASE:
        try:
            ws.send('~' + tok)
        except Exception:
            pass
    time.sleep(wait)
    w.clear()


@pytest.mark.e2e
def test_b_button(harness):
    dev, w = harness
    with using_ws() as ws:
        _press_release(ws, w, '*X')
    codes = candidate_codes('*X')
    assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected B press"
    assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected B release"


@pytest.mark.e2e
def test_face_buttons(harness):
    dev, w = harness
    with using_ws() as ws:
        for tok in ['*O', '*Sq', '*Tr']:
            _press_release(ws, w, tok)
            codes = candidate_codes(tok)
            assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected press for %s" % tok
            assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected release for %s" % tok


@pytest.mark.e2e
def test_shoulder_and_menu_buttons(harness):
    dev, w = harness
    with using_ws() as ws:
        for tok in ['*L1', '*R1', '*L2', '*R2', '*Sh', '*Op', '*Ps']:
            _press_release(ws, w, tok)
            codes = candidate_codes(tok)
            assert w.has_any_code(ecodes.EV_KEY, codes, 1), "expected press for %s" % tok
            assert w.has_any_code(ecodes.EV_KEY, codes, 0), "expected release for %s" % tok


@pytest.mark.e2e
def test_left_stick_axis(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*LX:100'); time.sleep(0.2)
        moved = w.abs_values(ecodes.ABS_X)
        ws.send('*LX:0'); time.sleep(0.3)
        rested = w.abs_values(ecodes.ABS_X)
    assert any(v > 128 for v in moved), "expected ABS_X to move above center"
    assert any(v == 128 for v in rested), "expected ABS_X back to center (128)"


@pytest.mark.e2e
def test_dpad_hat(harness):
    dev, w = harness
    with using_ws() as ws:
        w.clear(); time.sleep(0.05)
        ws.send('*DPAD:0'); time.sleep(0.25)
        ws.send('*DPAD:8'); time.sleep(0.3)
    assert w.has(ecodes.EV_ABS, ecodes.ABS_HAT0Y, -1), "expected HAT0Y=-1 (up)"
    assert w.has(ecodes.EV_ABS, ecodes.ABS_HAT0Y, 0), "expected HAT0Y centered"


@pytest.fixture(scope='function')
def two_clients():
    try:
        a = websocket.create_connection(WS_URL, timeout=5)
    except Exception as e:
        pytest.skip("Cannot reach WebSocket (client A): %s" % e)
    try:
        b = websocket.create_connection(WS_URL, timeout=5)
    except Exception:
        a.close()
        pytest.skip("Cannot open second WebSocket client")
    yield (a, b)
    try: a.close()
    except Exception: pass
    try: b.close()
    except Exception: pass


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_independent(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    w.clear(); time.sleep(0.1)
    a.send('*X'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 1), "A's B press lost"
    b.send('*O'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*O'), 1), "B's A press lost"
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 1), "A's B cleared by B connect"
    b.send('~*O'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 1), "A's B cleared by B release"
    a.send('~*X'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 0), "A's B release lost"


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_same_button_no_stuck(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    w.clear(); time.sleep(0.1)
    a.send('*X'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 1), "A's B press lost"
    b.send('*X'); time.sleep(0.1)
    b.send('~*X'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 1), "B dropped while A holds"
    w.clear(); time.sleep(0.1)
    a.send('~*X'); time.sleep(0.1)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*X'), 0, timeout=6.0), "B stuck ON after A release"


def _heartbeat(ws, stop, period=2.0):
    import time as _t
    while not stop.is_set():
        try:
            ws.send('#PING')
        except Exception:
            pass
        stop.wait(period)


@pytest.mark.e2e
def test_single_client_sticky_held_survives_watchdog(harness):
    dev, w = harness
    ws = open_ws()
    try:
        w.clear(); time.sleep(0.1)
        ws.send('*R3'); time.sleep(0.2)
        assert w.wait_for(ecodes.EV_KEY, candidate_codes('*R3'), 1), "R3 press lost"
        stop = threading.Event()
        t = threading.Thread(target=_heartbeat, args=(ws, stop), daemon=True)
        t.start()
        time.sleep(8)
        stop.set(); t.join(timeout=2)
        w.clear(); time.sleep(0.1)
        assert not w.wait_for(ecodes.EV_KEY, candidate_codes('*R3'), 0, timeout=1.0), \
            "R3 was released by watchdog during the hold"
        ws.send('~*R3'); time.sleep(0.2)
        assert w.wait_for(ecodes.EV_KEY, candidate_codes('*R3'), 0, timeout=4.0), \
            "R3 release not seen after explicit off"
    finally:
        try:
            ws.send('~*R3')
        except Exception:
            pass
        try:
            ws.close()
        except Exception:
            pass


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_sticky_l3_disconnect(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    neutralize(a, w); neutralize(b, w)
    stop = threading.Event()
    hb = threading.Thread(target=_heartbeat, args=(a, stop), daemon=True)
    hb.start()
    a.send('*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 1), "A's L3 press lost"
    b.send('*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 1), "B's L3 press lost"
    b.close()
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 1, timeout=8.0), \
        "A's L3 dropped when B disconnected"
    w.clear(); time.sleep(0.1)
    a.send('~*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 0, timeout=4.0), \
        "A's L3 not released after explicit off"
    stop.set()


@pytest.mark.e2e
@pytest.mark.multiclient
def test_two_clients_sticky_l3_r3_four_step(harness, two_clients):
    dev, w = harness
    a, b = two_clients
    neutralize(a, w); neutralize(b, w)
    a.send('*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 1), "step1 L3 on"
    b.send('*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 1), "step2 L3 on"
    b.send('~*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 1), "step3 L3 still on (A holds)"
    a.send('~*L3'); time.sleep(0.2)
    assert w.wait_for(ecodes.EV_KEY, candidate_codes('*L3'), 0, timeout=4.0), "step4 L3 off"
