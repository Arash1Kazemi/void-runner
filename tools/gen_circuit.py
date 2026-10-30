#!/usr/bin/env python3
"""
VOID RUNNER - circuit generator.

Builds the whole Wokwi circuit (diagram.json) from the tables below and writes the matching
pin table for the sketch (pins.h), so the circuit and the code can never disagree.

  * 16 x 24 game screen + 3 status bars of single LEDs, wired as one row/column matrix
  * 3 x 3 push-button keypad: exactly the 9 keys the game uses (every button also answers
    to a PC keyboard key)
  * 4 buzzers

Layout: LED panel on the left, the Mega (turned 180 degrees) on the right with its
double header facing the panel rows and its analog header facing up for the panel
columns, keypad and buzzers under the Mega. The wires inside the LED panel are hidden,
like a real LED panel; every wire from the panel, keypad and buzzers to the Mega is shown.

To change colours, size or key labels, edit the tables below and run:

    python tools/gen_circuit.py <project_dir> <sketch_dir>

Writes <project_dir>/diagram.json, <sketch_dir>/pins.h and <project_dir>/circuit_preview.png,
then checks that every LED, button and buzzer reaches the right Mega pin and that no two
visible wires cross.
"""
import json
import struct
import sys
import zlib
from pathlib import Path

# ============================================================== design tables (edit these)

SCREEN_W, SCREEN_H, STATUS_ROWS = 16, 24, 3

# Colour bands of the game screen, top to bottom: (first row, last row, light, body).
# The LED body is kept dark so an LED that is off almost disappears; the light colour is
# what you see when it is on (like the colour strips on the 1978 Space Invaders cabinet).
GAME_ZONES = [
    (0, 5, '#ff3a3a', '#3a1818'),       # enemies and bosses enter here
    (6, 17, '#ffffff', '#2c2c2c'),      # open space
    (18, 23, '#3aff4a', '#183a1c'),     # your ship's zone
]
# One (light, body) pair per status bar, top to bottom: boss health, ammo, lives.
STATUS_COLOURS = [('#ff3a3a', '#3a1818'), ('#ffd23a', '#3a3214'), ('#3aff4a', '#183a1c')]

# Brightness of a lit LED in Wokwi. The screen is multiplexed: each LED is on only 1/27 of
# the time, and Wokwi shows it with brightness = (on-time share) ^ (1 / gamma), measured on
# wokwi.com: default gamma 2.8 -> 32 % (too dim on the grey canvas), 30 -> 90 %. A real LED
# display gets the same effect by driving each LED harder during its short time slot.
LED_GAMMA = '30'
# How often Wokwi updates an LED's brightness. It averages over 8 updates, so the default
# 80 gives a 100 ms window and anything moving smears over several pixels; 300 gives
# 27 ms (about two full screen scans), which keeps moving things sharp.
LED_FPS = '300'

# Keypad, row by row: (key letter, PC keyboard key, button colour, name shown on the button).
# The letter is what the code and the part ids use (never change it without the code); the
# shown name is only the text under the button, in the style of an Atari joystick + console:
# W A S D move (W above S, like a keyboard), R fire / OK, Q back, P pause, M sound, B light.
BUTTONS = [
    [('Q', 'q', 'white', 'BACK'), ('W', 'w', 'blue', 'UP'), ('R', 'r', 'red', 'FIRE')],
    [('A', 'a', 'blue', 'LEFT'), ('S', 's', 'blue', 'DOWN'), ('D', 'd', 'blue', 'RIGHT')],
    [('P', 'p', 'yellow', 'PAUSE'), ('M', 'm', 'yellow', 'SOUND'), ('B', 'b', 'yellow', 'LIGHT')],
]
KR, KC = len(BUTTONS), len(BUTTONS[0])               # keypad rows and columns

# Visible wire colours by job (wires inside the LED panel are hidden).
WIRE = {'row_bus': 'orange', 'col_bus': 'blue', 'key_row': 'purple', 'key_col': 'green',
        'buzzer': 'violet', 'gnd': 'black'}

# ============================================================== part geometry (wokwi-elements)

MM = 96 / 25.4                                     # px per mm
LED_PINS = {'A': (25.0, 42.0), 'C': (15.0, 42.0)}  # wokwi-led is 40 x 50 px
BTN_PINS = {'1.l': (0.0, 13.0), '2.l': (0.0, 32.0), '1.r': (67.0, 13.0), '2.r': (67.0, 32.0)}
BUZ_W, BUZ_H = 17.0 * MM, 20.0 * MM                # wokwi-buzzer 17 x 20 mm, legs below
BUZ_PINS = {'1': (27.0, 84.0), '2': (37.0, 84.0)}
MEGA_W, MEGA_H = 102.66 * MM, 50.80 * MM
MEGA_PINS = {
    'GND.1': (119, 9), '13': (129, 9), '12': (138, 9), '11': (148, 9), '10': (157.5, 9),
    '9': (167.5, 9), '8': (177, 9), '7': (190, 9), '6': (200, 9), '5': (209.5, 9), '4': (219, 9),
    '3': (228.5, 9), '2': (238, 9), '1': (247.5, 9), '0': (257.5, 9), '14': (270.5, 9),
    '15': (280, 9), '16': (289.5, 9), '17': (299, 9), '18': (308.5, 9), '19': (318.5, 9),
    '20': (328, 9), '21': (337.5, 9),
}
for _i, _x in enumerate([208.5, 218, 227.5, 237.25, 246.75, 256.25, 266, 275.5,
                         290.25, 300, 309.5, 319.25, 328.75, 338.5, 348, 357.75]):
    MEGA_PINS['A%d' % _i] = (_x, 184.5)
for _k, _y in enumerate([17.5, 27.25, 36.75, 46.25, 56, 65.5, 75, 84.5,
                         94.25, 103.75, 113.5, 123, 132.75, 142.25, 152, 161.5]):
    MEGA_PINS[str(22 + 2 * _k)] = (361, _y)       # double header: even pins inner column
    MEGA_PINS[str(23 + 2 * _k)] = (371, _y)       #                odd pins outer column

# ============================================================== layout

TOTAL_ROWS = SCREEN_H + STATUS_ROWS
PX, PY = 44, 46                  # LED pitch: about square, so shapes are not stretched
SX, SY = 160, 240                # top-left LED of the panel (first status bar)
STATUS_GAP = 24                  # space between the status bars and the game screen
LANE = 6                         # spacing of parallel wires

# physical order of the matrix rows, top to bottom: status bars first, then the game screen
PHYS_ROWS = list(range(SCREEN_H, TOTAL_ROWS)) + list(range(SCREEN_H))

def led_xy(x, row):
    """Top-left of the LED in column x of matrix row `row` (rows >= SCREEN_H are status bars)."""
    if row >= SCREEN_H:
        return SX + PX * x, SY + PY * (row - SCREEN_H)
    return SX + PX * x, SY + PY * STATUS_ROWS + STATUS_GAP + PY * row

def led_id(x, row):
    return 'led_%d_%d' % (x, row) if row < SCREEN_H else 'st_%d_%d' % (x, row - SCREEN_H)

def led_colours(row):
    if row >= SCREEN_H:
        return STATUS_COLOURS[row - SCREEN_H]
    return next((light, body) for lo, hi, light, body in GAME_ZONES if lo <= row <= hi)

PANEL_RIGHT = SX + PX * (SCREEN_W - 1) + 40
MEGA_ROT = 180
MX = PANEL_RIGHT + 40 + LANE * TOTAL_ROWS + 30   # room for one lane per row wire
MY = SY - 10                                     # analog header level with the panel top

def rot(px, py, w, h, deg):
    return (w - px, h - py) if deg == 180 else (px, py)

def mega_pin(name):
    x, y = rot(*MEGA_PINS[name], MEGA_W, MEGA_H, MEGA_ROT)
    return MX + x, MY + y

# ============================================================== pin assignment

# Rows: the double header, read top to bottom on the turned board. At each height the even
# pin sits further in, so its wire arrives 5 px above the odd pin's wire.
# Pin 46 is left out: it is OC5A, the output of Timer5, used by the fourth buzzer.
_row_pins = []
for k in range(15, -1, -1):
    _row_pins += [str(22 + 2 * k), str(23 + 2 * k)]
_used = sorted([p for p in _row_pins if p != '46'], key=int)[:TOTAL_ROWS]
_row_pins = [p for p in _row_pins if p in _used]
ROW_PIN_OF_PHYS = _row_pins                          # physical row i (top to bottom) -> pin
ROW_PINS = [None] * TOTAL_ROWS
for i, row in enumerate(PHYS_ROWS):
    ROW_PINS[row] = ROW_PIN_OF_PHYS[i]
COL_PINS = ['A%d' % x for x in range(SCREEN_W)]      # column x -> Ax (PORTF / PORTK bit order)
KEY_ROW_PINS = ['18', '19', '20', '21'][:KR]
KEY_COL_PINS = ['17', '16', '15', '14'][:KC]
BUZZER_PINS = ['5', '6', '11', '46']                  # OC3A, OC4A, OC1A, OC5A
BUZZER_TIMERS = ['Timer3 OC3A', 'Timer4 OC4A', 'Timer1 OC1A', 'Timer5 OC5A']

# ============================================================== diagram building

parts, wires = [], []

def part(ptype, pid, left, top, attrs=None, rotate=0):
    p = {'type': ptype, 'id': pid, 'top': round(top, 2), 'left': round(left, 2), 'attrs': attrs or {}}
    if rotate:
        p['rotate'] = rotate
    parts.append(p)

def mv(axis, d):
    return '%s%g' % (axis, round(d, 2))

def wire(a, b, colour, route, kind):
    """kind: 'hidden' (inside the panel), 'link' (short helper wire) or 'bus' (to the Mega)."""
    wires.append({'a': a, 'b': b, 'colour': colour, 'route': route, 'kind': kind})

part('wokwi-arduino-mega', 'mega', MX, MY, {}, MEGA_ROT)

# --- LEDs and the hidden wires inside the panel
for row in range(TOTAL_ROWS):
    light, body = led_colours(row)
    for x in range(SCREEN_W):
        lx, ly = led_xy(x, row)
        part('wokwi-led', led_id(x, row), lx, ly, {'color': body, 'lightColor': light, 'gamma': LED_GAMMA, 'fps': LED_FPS})
for row in range(TOTAL_ROWS):                         # anodes of one row
    for x in range(SCREEN_W - 1):
        wire('%s:A' % led_id(x, row), '%s:A' % led_id(x + 1, row), '', [], 'hidden')
for x in range(SCREEN_W):                             # cathodes of one column
    for a, b in zip(PHYS_ROWS, PHYS_ROWS[1:]):
        wire('%s:C' % led_id(x, a), '%s:C' % led_id(x, b), '', [], 'hidden')

# --- column bus: from the top of each column, nested arcs over the panel into A0-A15.
# The leftmost column takes the highest arc and the rightmost pin, so no two cross.
top_row = PHYS_ROWS[0]
for x in range(SCREEN_W):
    lx, ly = led_xy(x, top_row)
    cx, cy = lx + LED_PINS['C'][0], ly + LED_PINS['C'][1]
    lane_x = lx + 6                                   # just left of the LED body
    h = SY - 16 - LANE * (SCREEN_W - 1 - x)
    px, py = mega_pin(COL_PINS[x])
    wire('%s:C' % led_id(x, top_row), 'mega:%s' % COL_PINS[x], WIRE['col_bus'],
         [mv('h', lane_x - cx), mv('v', h - cy), '*', mv('v', h - py)], 'bus')

# --- row bus: from the right end of each row to its own lane, then to the double header.
approach = []
for i, row in enumerate(PHYS_ROWS):
    px, py = mega_pin(ROW_PIN_OF_PHYS[i])
    approach.append(py - 5 if int(ROW_PIN_OF_PHYS[i]) % 2 == 0 else py)
src_y = [led_xy(SCREEN_W - 1, row)[1] + LED_PINS['A'][1] for row in PHYS_ROWS]
down = [i for i in range(TOTAL_ROWS) if approach[i] > src_y[i]]      # rows above their pin
up = [i for i in range(TOTAL_ROWS) if approach[i] <= src_y[i]]       # rows at or below it
lane_rank = {}
for k, i in enumerate(sorted(down, reverse=True)):   # descending: upper rows take outer lanes
    lane_rank[i] = k
for k, i in enumerate(sorted(up)):                   # ascending: lower rows take outer lanes
    lane_rank[i] = k
for i, row in enumerate(PHYS_ROWS):
    lx, ly = led_xy(SCREEN_W - 1, row)
    ax, ay = lx + LED_PINS['A'][0], ly + LED_PINS['A'][1]
    lane_x = PANEL_RIGHT + 20 + LANE * lane_rank[i]
    pin = ROW_PIN_OF_PHYS[i]
    route = [mv('h', lane_x - ax)]
    if abs(approach[i] - ay) > 0.01:
        route.append(mv('v', approach[i] - ay))
    if int(pin) % 2 == 0:
        route += ['*', 'v-5']
    wire('%s:A' % led_id(SCREEN_W - 1, row), 'mega:%s' % pin, WIRE['row_bus'], route, 'bus')

# --- keypad and buzzers under the Mega (wires come up into its lower header)
BELOW = MY + MEGA_H                    # lower edge of the turned board
KX, KPX, KPY = MX + 40, 90, 70
BX, BPX = KX + KC * KPX + 30, 85

def fan_heights(items, top, step):
    """items: (lane_x, pin_x, key) of wires coming up from below. Returns a turn height per
    wire so that no two wires of the fan cross (outer wires turn higher)."""
    right = sorted([i for i in items if i[1] >= i[0]], key=lambda i: i[0])
    left = sorted([i for i in items if i[1] < i[0]], key=lambda i: -i[0])
    heights = {}
    for group in (right, left):
        for rank, item in enumerate(group):
            heights[item[2]] = top + step * rank
    return heights

buz_top = BELOW + 16
key_top = buz_top + LANE * 4 + 12
KY = key_top + LANE * (KR + KC) + 30
BY = KY + 10

for r in range(KR):
    for c in range(KC):
        letter, key, colour, name = BUTTONS[r][c]
        part('wokwi-pushbutton', 'btn_%s' % letter, KX + KPX * c, KY + KPY * r,
             {'color': colour, 'label': name, 'key': key})
for r in range(KR):                   # contact 1 of each button = its row line
    for c in range(KC - 1):
        wire('btn_%s:1.r' % BUTTONS[r][c][0], 'btn_%s:1.l' % BUTTONS[r][c + 1][0],
             WIRE['key_row'], [], 'link')
for c in range(KC):                   # contact 2 of each button = its column line
    for r in range(KR - 1):
        wire('btn_%s:2.r' % BUTTONS[r][c][0], 'btn_%s:2.r' % BUTTONS[r + 1][c][0],
             WIRE['key_col'], ['h11', '*', 'h11'], 'link')

key_items = []                         # (lane, pin_x, key, src, pin, (x, y), first move, colour)
for r in range(KR):
    sx_, sy_ = KX + BTN_PINS['1.l'][0], KY + KPY * r + BTN_PINS['1.l'][1]
    lane = sx_ - 12 - 8 * r            # lower rows take outer lanes
    key_items.append((lane, mega_pin(KEY_ROW_PINS[r])[0], 'r%d' % r, 'btn_%s:1.l' % BUTTONS[r][0][0],
                      KEY_ROW_PINS[r], (sx_, sy_), mv('h', lane - sx_), WIRE['key_row']))
for c in range(KC):
    sx_, sy_ = KX + KPX * c + BTN_PINS['2.r'][0], KY + BTN_PINS['2.r'][1]
    key_items.append((sx_ + 11, mega_pin(KEY_COL_PINS[c])[0], 'c%d' % c, 'btn_%s:2.r' % BUTTONS[0][c][0],
                      KEY_COL_PINS[c], (sx_, sy_), 'h11', WIRE['key_col']))
turn = fan_heights([(i[0], i[1], i[2]) for i in key_items], key_top, LANE)
for lane, _, key, src, pin, (sx_, sy_), first, colour in key_items:
    _, py = mega_pin(pin)
    wire(src, 'mega:%s' % pin, colour, [first, mv('v', turn[key] - sy_), '*', mv('v', turn[key] - py)], 'bus')

buz_items = []
for i in range(len(BUZZER_PINS)):
    bx = BX + BPX * i
    on_side = int(BUZZER_PINS[i]) >= 22               # its wire goes up beside the board:
    turned = 0 if on_side else 180                    # not turned, so its signal leg is on the right
    part('wokwi-buzzer', 'bz%d' % (i + 1), bx, BY, {'volume': '0.6'}, turned)
    p2 = rot(*BUZ_PINS['2'], BUZ_W, BUZ_H, turned)
    buz_items.append((bx + p2[0], mega_pin(BUZZER_PINS[i])[0], 'b%d' % i, 'bz%d:2' % (i + 1),
                      BUZZER_PINS[i], (bx + p2[0], BY + p2[1]), WIRE['buzzer']))
    if i < len(BUZZER_PINS) - 1:       # ground from buzzer to buzzer (hidden)
        wire('bz%d:1' % (i + 1), 'bz%d:1' % (i + 2), '', [], 'hidden')
p1 = rot(*BUZ_PINS['1'], BUZ_W, BUZ_H, turned)
last = len(BUZZER_PINS) - 1
buz_items.append((BX + last * BPX + p1[0], mega_pin('GND.1')[0], 'gnd', 'bz%d:1' % (last + 1), 'GND.1',
                  (BX + last * BPX + p1[0], BY + p1[1]), WIRE['gnd']))
# A buzzer on the double header (pin 46, the rightmost buzzer) goes straight up beside the
# board and across it at its pin's height; the others fan into the lower header.
side = [i for i in buz_items if i[4].isdigit() and int(i[4]) >= 22]
fan = [i for i in buz_items if i not in side]
turn = fan_heights([(i[0], i[1], i[2]) for i in fan], buz_top, LANE)
for lane, _, key, src, pin, (sx_, sy_), colour in fan:
    _, py = mega_pin(pin)
    wire(src, 'mega:%s' % pin, colour, [mv('v', turn[key] - sy_), '*', mv('v', turn[key] - py)], 'bus')
for lane, _, key, src, pin, (sx_, sy_), colour in side:
    _, py = mega_pin(pin)
    wire(src, 'mega:%s' % pin, colour, [mv('v', py - sy_)], 'bus')

# ============================================================== geometry helpers for checks

PIN_TABLE = {}
for p in parts:
    if p['type'] == 'wokwi-led':
        for n, (x, y) in LED_PINS.items():
            PIN_TABLE['%s:%s' % (p['id'], n)] = (p['left'] + x, p['top'] + y)
    elif p['type'] == 'wokwi-pushbutton':
        for n, (x, y) in BTN_PINS.items():
            PIN_TABLE['%s:%s' % (p['id'], n)] = (p['left'] + x, p['top'] + y)
    elif p['type'] == 'wokwi-buzzer':
        for n, (x, y) in BUZ_PINS.items():
            rx, ry = rot(x, y, BUZ_W, BUZ_H, p.get('rotate', 0))
            PIN_TABLE['%s:%s' % (p['id'], n)] = (p['left'] + rx, p['top'] + ry)
for n in MEGA_PINS:
    PIN_TABLE['mega:%s' % n] = mega_pin(n)

def apply(pt, moves):
    pts = [pt]
    x, y = pt
    for m in moves:
        d = float(m[1:])
        x, y = (x + d, y) if m[0] == 'h' else (x, y + d)
        pts.append((x, y))
    return pts

def polyline(w):
    route = w['route']
    star = route.index('*') if '*' in route else len(route)
    src = apply(PIN_TABLE[w['a']], route[:star])
    dst = apply(PIN_TABLE[w['b']], list(reversed(route[star + 1:])))
    (x1, y1), (x2, y2) = src[-1], dst[-1]
    mid = [] if (abs(x1 - x2) < 0.01 or abs(y1 - y2) < 0.01) else [(x2, y1)]
    return src + mid + list(reversed(dst))

# ============================================================== checks

def check_nets():
    parent = {}
    def find(a):
        parent.setdefault(a, a)
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a
    for w in wires:
        parent[find(w['a'])] = find(w['b'])
    for p in parts:                     # a push button's left and right pins are one contact
        if p['type'] == 'wokwi-pushbutton':
            parent[find(p['id'] + ':1.l')] = find(p['id'] + ':1.r')
            parent[find(p['id'] + ':2.l')] = find(p['id'] + ':2.r')
    nets = {}
    for pin in list(parent):
        nets.setdefault(find(pin), set()).add(pin)
    mega_of = {}
    for members in nets.values():
        megas = sorted(m for m in members if m.startswith('mega:'))
        assert len(megas) <= 1, 'short between Mega pins: %s' % megas
        for m in members:
            mega_of[m] = megas[0][5:] if megas else None
    errors = 0
    for row in range(TOTAL_ROWS):
        for x in range(SCREEN_W):
            pid = led_id(x, row)
            if mega_of.get(pid + ':A') != ROW_PINS[row] or mega_of.get(pid + ':C') != COL_PINS[x]:
                errors += 1
    for r in range(KR):
        for c in range(KC):
            pid = 'btn_%s' % BUTTONS[r][c][0]
            if mega_of.get(pid + ':1.l') != KEY_ROW_PINS[r] or mega_of.get(pid + ':2.l') != KEY_COL_PINS[c]:
                errors += 1
    for i in range(len(BUZZER_PINS)):
        if mega_of.get('bz%d:2' % (i + 1)) != BUZZER_PINS[i] or mega_of.get('bz%d:1' % (i + 1)) != 'GND.1':
            errors += 1
    return errors

def count_crossings():
    segs = []
    for wi, w in enumerate(wires):
        if w['kind'] == 'hidden':
            continue
        pts = polyline(w)
        for a, b in zip(pts, pts[1:]):
            segs.append((wi, a, b))
    def cross(s, t):
        (ax1, ay1), (ax2, ay2) = s
        (bx1, by1), (bx2, by2) = t
        a_h, b_h = abs(ay1 - ay2) < 0.01, abs(by1 - by2) < 0.01
        if a_h == b_h:
            return False
        h, v = (s, t) if a_h else (t, s)
        (hx1, hy), (hx2, _) = h
        (vx, vy1), (_, vy2) = v
        return (min(hx1, hx2) < vx < max(hx1, hx2)) and (min(vy1, vy2) < hy < max(vy1, vy2))
    found = []
    for i in range(len(segs)):
        for j in range(i + 1, len(segs)):
            a, b = wires[segs[i][0]], wires[segs[j][0]]
            if a is not b and a['kind'] == 'bus' and cross(segs[i][1:], segs[j][1:]):
                found.append((a['b'], b['b']))
    return found

# ============================================================== outputs

def write_diagram(path):
    doc = {'version': 1, 'author': 'VOID RUNNER team', 'editor': 'wokwi', 'parts': parts,
           'connections': [[w['a'], w['b'], w['colour'], w['route']] for w in wires],
           'dependencies': {}}
    path.write_text(json.dumps(doc, indent=1), encoding='utf-8', newline='\n')

def write_pins_h(path):
    def arr(items):
        return ', '.join(str(i) for i in items)
    keys = ',\n  '.join('{%s}' % ', '.join("'%s'" % b[0] for b in row) for row in BUTTONS)
    text = f"""/*
 * pins.h -- GENERATED by tools/gen_circuit.py together with diagram.json.
 * Do not edit by hand: change the generator and run it again, so the circuit
 * and the code always match.
 */
#pragma once
#include <Arduino.h>

// ---- LED screen: {SCREEN_W} columns x {SCREEN_H} game rows + {STATUS_ROWS} status bars, one big matrix
const uint8_t SCREEN_W    = {SCREEN_W};
const uint8_t SCREEN_H    = {SCREEN_H};
const uint8_t STATUS_ROWS = {STATUS_ROWS};
const uint8_t TOTAL_ROWS  = SCREEN_H + STATUS_ROWS;

// Anode pin of every LED row: rows 0-{SCREEN_H - 1} = game screen top to bottom,
// rows {SCREEN_H}-{TOTAL_ROWS - 1} = status bars top to bottom (all on the double header).
const uint8_t ROW_PINS[TOTAL_ROWS] = {{
  {arr(ROW_PINS)}
}};

// Cathode pin of every LED column, left to right: column x is wired to Ax,
// so columns 0-7 are PORTF bits 0-7 and columns 8-15 are PORTK bits 0-7.
const uint8_t COL_PINS[SCREEN_W] = {{
  {arr(COL_PINS)}
}};

// ---- {KR} x {KC} push-button keypad: contact 1 = row line, contact 2 = column line
const uint8_t KEY_ROWS = {KR}, KEY_COLS = {KC};
const uint8_t KEY_ROW_PINS[KEY_ROWS] = {{ {arr(KEY_ROW_PINS)} }};
const uint8_t KEY_COL_PINS[KEY_COLS] = {{ {arr(KEY_COL_PINS)} }};
const char KEY_NAMES[KEY_ROWS][KEY_COLS] = {{
  {keys}
}};

// ---- buzzers, each on the "A" output of its own 16-bit timer
const uint8_t BUZZER_COUNT = {len(BUZZER_PINS)};
const uint8_t BUZZER_PINS[BUZZER_COUNT] = {{ {arr(BUZZER_PINS)} }};   // {', '.join(BUZZER_TIMERS)}
"""
    path.write_text(text, encoding='utf-8', newline='\n')

def write_preview(path, scale=0.5):
    xs, ys = zip(*(list(PIN_TABLE.values()) + [pt for w in wires for pt in polyline(w)]))
    x0, y0 = min(xs) - 60, min(ys) - 60
    W, H = int((max(xs) - x0 + 100) * scale), int((max(ys) - y0 + 100) * scale)
    img = bytearray(b'\x30\x30\x30' * W * H)            # dark canvas, like Wokwi's dark theme
    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            i = (y * W + x) * 3
            img[i:i + 3] = bytes(c)
    def line(p, q, c):
        (x1, y1), (x2, y2) = [(int((a - x0) * scale), int((b - y0) * scale)) for a, b in (p, q)]
        n = max(abs(x2 - x1), abs(y2 - y1), 1)
        for k in range(n + 1):
            put(x1 + (x2 - x1) * k // n, y1 + (y2 - y1) * k // n, c)
    def rect(x, y, w, h, c):
        for yy in range(int((y - y0) * scale), int((y + h - y0) * scale)):
            for xx in range(int((x - x0) * scale), int((x + w - x0) * scale)):
                put(xx, yy, c)
    def hexrgb(s, default=(120, 120, 120)):
        named = {'blue': (40, 90, 230), 'black': (10, 10, 10), 'orange': (240, 140, 0), 'purple': (150, 60, 200),
                 'violet': (200, 90, 220), 'green': (40, 190, 60), 'red': (230, 40, 40), 'white': (230, 230, 230),
                 'yellow': (230, 200, 0)}
        if s in named:
            return named[s]
        return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5)) if s.startswith('#') else default
    for p in parts:
        if p['type'] == 'wokwi-led':                     # draw every LED "on" in its light colour
            rect(p['left'] + 12, p['top'] + 4, 16, 22, hexrgb(p['attrs']['lightColor']))
        elif p['type'] == 'wokwi-pushbutton':
            rect(p['left'] + 12, p['top'], 45, 45, (70, 70, 70))
            rect(p['left'] + 22, p['top'] + 10, 25, 25, hexrgb(p['attrs']['color']))
        elif p['type'] == 'wokwi-buzzer':
            rect(p['left'], p['top'], BUZ_W, BUZ_H, (10, 10, 10))
        elif p['type'] == 'wokwi-arduino-mega':
            rect(p['left'], p['top'], MEGA_W, MEGA_H, (0, 110, 150))
    for w in wires:
        if w['colour']:
            pts = polyline(w)
            for a, b in zip(pts, pts[1:]):
                line(a, b, hexrgb(w['colour']))
    raw = b''.join(b'\x00' + bytes(img[y * W * 3:(y + 1) * W * 3]) for y in range(H))
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
    path.write_bytes(png + chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + chunk(b'IEND', b''))

if __name__ == '__main__':
    project, sketch = Path(sys.argv[1]), Path(sys.argv[2])
    write_diagram(project / 'diagram.json')
    write_pins_h(sketch / 'pins.h')
    write_preview(project / 'circuit_preview.png')
    leds = sum(p['type'] == 'wokwi-led' for p in parts)
    xs, ys = zip(*PIN_TABLE.values())
    print('parts: %d (%d LEDs), wires: %d (%d visible)' % (
        len(parts), leds, len(wires), sum(1 for w in wires if w['colour'])))
    print('circuit size: about %d x %d px' % (max(xs) - min(xs), max(ys) - min(ys)))
    print('wiring errors: %d' % check_nets())
    crossings = count_crossings()
    print('visible wire crossings: %d' % len(crossings))
    for a, b in crossings[:20]:
        print('   ', a, 'x', b)
