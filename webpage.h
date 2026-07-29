/*
 * MIT License
 *
 * Copyright (c) 2026 controllercustom@myyahoo.com
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
    <meta name="mobile-web-app-capable" content="yes">
    <title>TGPad-NS v1.0.0 Gamepad</title>
    <style>
        :root {
            --bg-chassis: #e0e0d1;
            --pale-yellow: #fdf6be;
            --deep-navy: #1a2b4c;
            --border-color: #dcd3a0;
            --key-shadow-inset: inset -2px -2px 4px rgba(0,0,0,0.1), 2px 2px 5px rgba(0,0,0,0.2);
        }

        * {
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            -webkit-tap-highlight-color: transparent;
            user-select: none;
        }
        html, body {
            margin: 0; padding: 0;
            width: 100%; height: 100%;
            overflow: hidden;
            font-family: Helvetica, Arial, sans-serif;
            background-color: var(--bg-chassis);
            display: flex;
            flex-direction: column;
            -webkit-touch-callout: none;
            -webkit-user-select: none;
            user-select: none;
            touch-action: none;
        }

        #pad {
            flex: 1;
            position: relative;
            margin: 4px;
            box-sizing: border-box;
            min-height: 0;
            touch-action: none;
        }

        .key {
            background-color: var(--pale-yellow);
            border: 2px solid var(--border-color);
            box-shadow: var(--key-shadow-inset);
            display: flex; align-items: center; justify-content: center;
            font-weight: bold; cursor: pointer;
            transition: all 0.05s ease-in-out;
            user-select: none; -webkit-user-select: none; -webkit-touch-callout: none;
            border-radius: 8px;
            color: #111; font-size: 3.2vmin;
            line-height: 1; text-align: center;
            box-sizing: border-box; padding: 0;
            touch-action: none;
            overflow: hidden;
            position: absolute;
        }
        .key:active {
            transform: translateY(1px);
            box-shadow: inset 2px 2px 4px rgba(0,0,0,0.1), 0 1px 2px rgba(0,0,0,0.2);
        }

        .nav-key {
            background-color: var(--deep-navy) !important;
            border: none !important;
            box-shadow: none !important;
            color: #fff; font-size: 3.5vmin;
            border-radius: 12px;
        }

        .dpad8 {
            background:
                conic-gradient(from -22.5deg,
                    rgba(26,43,76,0.95) 0deg 45deg,
                    rgba(46,70,120,0.95) 45deg 90deg,
                    rgba(26,43,76,0.95) 90deg 135deg,
                    rgba(46,70,120,0.95) 135deg 180deg,
                    rgba(26,43,76,0.95) 180deg 225deg,
                    rgba(46,70,120,0.95) 225deg 270deg,
                    rgba(26,43,76,0.95) 270deg 315deg,
                    rgba(46,70,120,0.95) 315deg 360deg);
            border: 3px solid #1a2b4c;
            box-shadow: inset 0 0 8px rgba(0,0,0,0.35), 2px 2px 6px rgba(0,0,0,0.25);
            border-radius: 50%;
            position: absolute;
            box-sizing: border-box;
        }
        .dpad8-sector {
            fill: rgba(207, 224, 255, 0.75);
            transition: fill 0.1s;
        }
        .dpad8-sector.btn-active {
            fill: #fff;
            filter: drop-shadow(0 0 6px #ffd86b) drop-shadow(0 0 3px #ffd86b);
        }
        .dpad8-hub {
            position: absolute;
            left: 50%; top: 50%;
            width: 26%; height: 26%;
            transform: translate(-50%, -50%);
            background: rgba(255,255,255,0.18);
            border-radius: 50%;
            border: 2px solid rgba(255,255,255,0.35);
            pointer-events: none;
        }

        .round {
            border-radius: 50%;
            aspect-ratio: 1 / 1;
            height: auto !important;
        }

        .face {
            font-size: 7vmin;
        }

        .mod-active, .btn-active {
            box-shadow: inset 2px 2px 10px rgba(0,0,0,0.5) !important;
            transform: translateY(1px);
        }
        .btn-active { background-color: #b89c4c !important; }

        .led { margin-right: 6px; font-size: 11px; font-weight: bold; }
        .led.on  { color: #0f0; }
        .led.off { color: #888; }

        .stick-base {
            background-color: #cfcfcf;
            border: 3px solid #999;
            box-shadow: inset -2px -2px 6px rgba(0,0,0,0.2), 2px 2px 6px rgba(0,0,0,0.2);
            border-radius: 50%;
            position: absolute;
            display: flex; align-items: center; justify-content: center;
            touch-action: none;
        }
        .stick-thumb {
            background-color: #2a2a2a;
            border-radius: 50%;
            box-shadow: inset -1px -1px 3px rgba(255,255,255,0.15), 2px 2px 4px rgba(0,0,0,0.4);
            position: absolute;
            left: 50%; top: 50%;
            transform: translate(-50%, -50%);
            pointer-events: none;
        }
        .stick-label {
            position: absolute;
            bottom: 2px;
            font-size: 2vmin; color: #555;
            pointer-events: none;
        }

        .trigger {
            background-color: #6a4a9a !important;
            color: #fff;
            border-radius: 8px 8px 4px 4px;
        }
        .trigger.btn-active { background-color: #4a2a7a !important; }
        .bumper {
            background-color: #4a7acc !important;
            color: #fff;
        }
        .sticky {
            background-color: #c77d4a !important;
            color: #fff;
            font-size: 2.2vmin;
            border: 2px solid #8a5a2a !important;
        }
        .menu-key {
            background-color: #8a8a8a !important;
            color: #fff;
            font-size: 2.4vmin;
        }
        .ps-key {
            background-color: #222 !important;
            color: #ddd;
            border-radius: 50%;
        }

        .rotate-overlay {
            display: none;
            position: fixed;
            inset: 0;
            z-index: 9999;
            background: var(--deep-navy);
            color: #fff;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            font-size: 5vmin;
            text-align: center;
        }
        .rotate-overlay .icon { font-size: 12vmin; margin-bottom: 4vmin; }
    </style>
</head>
<body>
    <div id="rotate-overlay" class="rotate-overlay">
        <div class="icon">↻</div>
        <div>Rotate device to landscape</div>
    </div>
    <div id="pad"></div>

<script>
let ws;

function checkOrientation() {
    const el = document.getElementById('rotate-overlay');
    if (!el) return;
    const portrait = window.matchMedia('(orientation: portrait)').matches;
    el.style.display = portrait ? 'flex' : 'none';
}

function connectWS() {
    ws = new WebSocket('ws://' + location.hostname + ':81/');
    ws.onopen  = () => { resyncSticky(); };
    ws.onmessage = (e) => {
        const d = e.data;
        if (d.startsWith('#HOST:')) {
        } else if (d.startsWith('#DZ:')) {
        }
    };
    ws.onclose   = () => { setTimeout(connectWS, 2000); };
    ws.onerror   = () => { };
}
connectWS();

document.addEventListener('touchstart', function(e) {
    if (e.target.closest('.key, .dpad8, .stick-base')) e.preventDefault();
}, {passive: false});

setInterval(() => { if (ws && ws.readyState === 1) ws.send('#PING'); }, 2000);

window.matchMedia('(orientation: portrait)').addListener(checkOrientation);
checkOrientation();

const pad = document.getElementById('pad');

const keyEls = {};
function mkKey(o) {
    const b = document.createElement('button');
    b.className = 'key ' + (o.cls || '');
    b.style.left   = o.x + '%';
    b.style.top    = o.y + '%';
    b.style.width  = o.w + '%';
    b.style.height = o.h + '%';
    b.style.transform = 'translate(-50%, -50%)';
    if (o.round) b.classList.add('round');
    b.innerHTML = o.label || '';
    if (o.k !== undefined) { b.dataset.k = o.k; keyEls[o.k] = b; }
    pad.appendChild(b);
    return b;
}

const buttons = [
    // Bumpers
    {k:'*L1', label:'L', x:9.5, y:26, w:18, h:10, cls:'bumper'},
    {k:'*R1', label:'R', x:90.5, y:26, w:18, h:10, cls:'bumper'},

    // Digital triggers (ZL / ZR)
    {k:'*L2', label:'ZL', x:9.5, y:14, w:18, h:10, cls:'trigger'},

    {k:'*R2', label:'ZR', x:90.5, y:14, w:18, h:10, cls:'trigger'},

    // Face buttons (Nintendo layout: X top, A right, B bottom, Y left)
    {k:'*Tr', round:true, label:'X', x:90, y:42, w:8, h:9, cls:'round face'},
    {k:'*O',  round:true, label:'A', x:95, y:56, w:8, h:9, cls:'round face'},
    {k:'*X',  round:true, label:'B', x:90, y:70, w:8, h:9, cls:'round face'},
    {k:'*Sq', round:true, label:'Y', x:85, y:56, w:8, h:9, cls:'round face'},

    // Menu buttons (Minus, Capture, Plus)
    {k:'*Sh', label:'−',    x:34, y:14, w:11, h:6, cls:'menu-key'},
    {k:'*Tp', label:'Cap',  x:50, y:14, w:11, h:6, cls:'menu-key'},
    {k:'*Op', label:'+',    x:66, y:14, w:11, h:6, cls:'menu-key'},

    // Home
    {k:'*Ps', label:'Home', x:50, y:48, w:8, h:8, cls:'ps-key round'},
];

buttons.forEach(mkKey);

// ---- 8-way directional pad ----
function mkDpad8(o) {
    const el = document.createElement('div');
    el.className = 'dpad8';
    el.style.left = o.x + '%';
    el.style.top  = o.y + '%';
    el.style.width = o.size + '%';
    el.style.aspectRatio = '1 / 1';
    el.style.transform = 'translate(-50%, -50%)';
    el.style.touchAction = 'none';
    pad.appendChild(el);

    const svgNS = 'http://www.w3.org/2000/svg';
    const svg = document.createElementNS(svgNS, 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.style.position = 'absolute';
    svg.style.top = '0';
    svg.style.left = '0';
    svg.style.width = '100%';
    svg.style.height = '100%';
    svg.style.pointerEvents = 'none';
    el.appendChild(svg);

    const paths = [];
    const cx = 12, cy = 12, rOut = 12, rIn = 3.2;
    for (let i = 0; i < 8; i++) {
        const a = (i * 45 - 90) * Math.PI / 180;
        const hp = (22.5 + 0.5) * Math.PI / 180;
        const p = document.createElementNS(svgNS, 'path');
        p.setAttribute('d',
            `M${cx + rOut * Math.cos(a - hp)} ${cy + rOut * Math.sin(a - hp)}` +
            `L${cx + rOut * Math.cos(a + hp)} ${cy + rOut * Math.sin(a + hp)}` +
            `L${cx + rIn  * Math.cos(a + hp)} ${cy + rIn  * Math.sin(a + hp)}` +
            `L${cx + rIn  * Math.cos(a - hp)} ${cy + rIn  * Math.sin(a - hp)}Z`);
        p.classList.add('dpad8-sector');
        svg.appendChild(p);
        paths.push(p);
    }

    const hub = document.createElement('div');
    hub.className = 'dpad8-hub';
    el.appendChild(hub);

    function sliceFor(dx, dy) {
        let a = Math.atan2(dy, dx) * 180 / Math.PI;
        a = (a + 90 + 360) % 360;
        return Math.round(a / 45) % 8;
    }

    let cur = -1;
    function apply(idx) {
        if (idx === cur) return;
        if (cur >= 0 && paths[cur]) paths[cur].classList.remove('btn-active');
        cur = idx;
        if (cur >= 0) {
            if (paths[cur]) paths[cur].classList.add('btn-active');
            if (ws && ws.readyState === 1) ws.send('*DPAD:' + cur);
        } else {
            if (ws && ws.readyState === 1) ws.send('*DPAD:8');
        }
    }

    function update(e) {
        const r = el.getBoundingClientRect();
        const cx = r.left + r.width / 2, cy = r.top + r.height / 2;
        const dx = e.clientX - cx, dy = e.clientY - cy;
        const rad = r.width / 2;
        if (Math.hypot(dx, dy) < rad * 0.18) { apply(-1); return; }
        apply(sliceFor(dx, dy));
    }

    el.addEventListener('pointerdown', (e) => {
        e.preventDefault(); e.stopPropagation();
        el.setPointerCapture(e.pointerId);
        update(e);
    });
    el.addEventListener('pointermove', (e) => {
        if (el.hasPointerCapture && el.hasPointerCapture(e.pointerId)) {
            e.preventDefault(); update(e);
        }
    });
    function end() { apply(-1); }
    el.addEventListener('pointerup', end);
    el.addEventListener('pointercancel', end);
    el.addEventListener('lostpointercapture', end);
}
mkDpad8({x:11, y:55.5, size:20});

// ---- Analog sticks ----
function mkStick(o) {
    const base = document.createElement('div');
    base.className = 'stick-base';
    base.style.left = o.x + '%';
    base.style.top  = o.y + '%';
    base.style.width = o.size + '%';
    base.style.aspectRatio = '1 / 1';
    base.style.transform = 'translate(-50%, -50%)';
    const thumb = document.createElement('div');
    thumb.className = 'stick-thumb';
    const ts = o.size * 0.45;
    thumb.style.width = ts + '%';
    thumb.style.height = ts + '%';
    const lbl = document.createElement('div');
    lbl.className = 'stick-label';
    lbl.textContent = o.label;
    base.appendChild(thumb);
    base.appendChild(lbl);
    pad.appendChild(base);
    return {base, thumb};
}

const RANGE = 127;
function buildStick(cfg) {
    const s = mkStick(cfg);
    let active = false;
    function report(dx, dy) {
        let nx = dx, ny = dy;
        const mag = Math.hypot(nx, ny);
        if (mag > 1) { nx /= mag; ny /= mag; }
        const vx = Math.round(nx * RANGE);
        const vy = Math.round(ny * RANGE);
        ws.send(cfg.axes.x + ':' + vx);
        ws.send(cfg.axes.y + ':' + vy);
        s.thumb.style.left = (50 + nx * 50) + '%';
        s.thumb.style.top  = (50 + ny * 50) + '%';
    }
    function center() {
        s.thumb.style.left = '50%';
        s.thumb.style.top  = '50%';
        ws.send(cfg.axes.x + ':0');
        ws.send(cfg.axes.y + ':0');
    }
    s.base.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        s.base.setPointerCapture(e.pointerId);
        active = true;
        move(e);
    });
    function move(e) {
        const r = s.base.getBoundingClientRect();
        const cx = r.left + r.width/2, cy = r.top + r.height/2;
        const dx = (e.clientX - cx) / (r.width/2);
        const dy = (e.clientY - cy) / (r.height/2);
        report(dx, dy);
    }
    s.base.addEventListener('pointermove', (e) => { if (active) { e.preventDefault(); e.stopPropagation(); move(e); } });
    function end(e) {
        active = false;
        center();
    }
    s.base.addEventListener('pointerup', end);
    s.base.addEventListener('pointercancel', end);
    s.base.addEventListener('lostpointercapture', end);
}

buildStick({x:31, y:72, size:20, label:'', axes:{x:'*LX', y:'*LY'}});
buildStick({x:69, y:72, size:20, label:'', axes:{x:'*RX', y:'*RY'}});

// ---- Sticky toggle buttons ----
const stickyControls = [];
function mkToggle(o) {
    const b = mkKey(o);
    let on = false;
    const ctrl = { k: o.k, getOn: () => on };
    stickyControls.push(ctrl);
    b.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        b.setPointerCapture(e.pointerId);
        on = !on;
        if (on) { ws.send(o.k); b.classList.add('btn-active'); }
        else      { ws.send('~' + o.k); b.classList.remove('btn-active'); }
    });
    return b;
}

function resyncSticky() {
    if (!ws || ws.readyState !== 1) return;
    for (const c of stickyControls) {
        ws.send(c.getOn() ? c.k : '~' + c.k);
    }
}

mkToggle({k:'*L3', label:'L3', x:45, y:72, w:8, h:6, cls:'sticky'});
mkToggle({k:'*R3', label:'R3', x:55, y:72, w:8, h:6, cls:'sticky'});

// ---- Generic button press/release with slide support ----
document.addEventListener('contextmenu', function(e) { e.preventDefault(); });

const pointers = new Map();

function findBtnByKey(k) { return keyEls[k] || null; }

function sendDown(k, btn) {
    if (!(ws && ws.readyState === 1)) return;
    ws.send(k);
    if (btn) btn.classList.add('btn-active');
}
function sendUp(k, btn) {
    if (!(ws && ws.readyState === 1)) return;
    ws.send('~' + k);
    if (btn) btn.classList.remove('btn-active');
}

function keysUnder(x, y) {
    let els;
    try { els = document.elementsFromPoint(x, y); }
    catch (_) { els = []; }
    const out = [];
    for (const el of els) {
        if (!el || !el.closest) continue;
        const btn = el.closest('.key');
        if (!btn) continue;
        if (btn.classList.contains('stick-base') || btn.classList.contains('sticky')) continue;
        if (btn.dataset.k === undefined) continue;
        out.push(btn);
    }
    return out;
}

document.addEventListener('pointerdown', function(e) {
    const btns = keysUnder(e.clientX, e.clientY);
    if (btns.length === 0) return;
    e.preventDefault();
    const set = new Set();
    for (const b of btns) {
        const k = b.dataset.k;
        set.add(k);
        sendDown(k, b);
    }
    pointers.set(e.pointerId, set);
}, {capture: true});

document.addEventListener('pointermove', function(e) {
    const set = pointers.get(e.pointerId);
    if (!set) return;
    e.preventDefault();
    const btns = keysUnder(e.clientX, e.clientY);
    const newKeys = new Set(btns.map(b => b.dataset.k));
    for (const k of Array.from(set)) {
        if (!newKeys.has(k)) {
            sendUp(k, findBtnByKey(k));
            set.delete(k);
        }
    }
    for (const b of btns) {
        const k = b.dataset.k;
        if (!set.has(k)) { sendDown(k, b); set.add(k); }
    }
}, {capture: true});

function pointerEnd(e) {
    const set = pointers.get(e.pointerId);
    if (!set) return;
    for (const k of set) sendUp(k, findBtnByKey(k));
    pointers.delete(e.pointerId);
}
document.addEventListener('pointerup', pointerEnd, {capture: true});
document.addEventListener('pointercancel', pointerEnd, {capture: true});
</script>
</body>
</html>
)rawliteral";

#endif
