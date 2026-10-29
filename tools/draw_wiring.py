#!/usr/bin/env python3
"""
VOID RUNNER - draws docs/media/wiring.png: a simplified wiring diagram of the circuit (what
connects to which Mega pin), in the wire colours of diagram.json. The pin lists come from
gen_circuit.py, so the picture always matches the real circuit.

Usage: python tools/draw_wiring.py     (needs Pillow)
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).parent))
import contextlib, io
with contextlib.redirect_stdout(io.StringIO()):
    import gen_circuit as gc                  # pin lists and colours

W, H = 1500, 960
BG, FG, DIM = (40, 40, 40), (230, 230, 230), (150, 150, 150)
ORANGE, BLUE, VIOLET, GREEN, GREY, BLACK = (240, 150, 30), (70, 120, 255), (200, 90, 230), (60, 200, 80), (170, 170, 170), (15, 15, 15)
MEGA = (0, 110, 150)
img = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(img)


def font(size, bold=False):
    for name in (('arialbd.ttf' if bold else 'arial.ttf'), 'DejaVuSans.ttf'):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


F_TITLE, F_HEAD, F, F_SMALL = font(30, True), font(19, True), font(16), font(14)


def text(xy, s, f=F, fill=FG, anchor='la'):
    d.text(xy, s, font=f, fill=fill, anchor=anchor)


def line(points, fill, width=3):
    d.line(points, fill=fill, width=width, joint='curve')


def pin_range(pins):
    """'22-45, 47-49' from a list of pin numbers."""
    nums = sorted(int(p) for p in pins)
    parts, start = [], nums[0]
    for a, b in zip(nums, nums[1:] + [None]):
        if b != a + 1:
            parts.append('%d' % start if start == a else '%d-%d' % (start, a))
            start = b
    return ', '.join(parts)


text((30, 20), 'VOID RUNNER - wiring', F_TITLE)
text((30, 60), 'Simplified: every LED, key and buzzer of the real circuit (diagram.json) is wired this way.', F, DIM)

# ---------------------------------------------------------------- Mega
MX, MY, MW, MH = 560, 150, 360, 560
d.rounded_rectangle((MX, MY, MX + MW, MY + MH), 14, fill=MEGA)
text((MX + MW / 2, MY + 22), 'Arduino Mega 2560', F_HEAD, anchor='ma')
headers = [  # (y, label, colour)
    (MY + 90, 'A0 - A15  -  16 LED columns (-)', BLUE),
    (MY + 200, 'pins %s  -  27 LED rows (+)' % pin_range(gc.ROW_PINS), ORANGE),
    (MY + 330, 'pins %s-%s  -  keypad rows' % (gc.KEY_ROW_PINS[0], gc.KEY_ROW_PINS[-1]), VIOLET),
    (MY + 390, 'pins %s-%s  -  keypad columns' % (gc.KEY_COL_PINS[-1], gc.KEY_COL_PINS[0]), GREEN),
    (MY + 480, 'pins %s  -  buzzers' % ', '.join(gc.BUZZER_PINS), VIOLET),
    (MY + 545, 'GND  -  buzzers (-)', GREY),
]
for y, label, colour in headers:
    d.rectangle((MX + 12, y - 9, MX + 24, y + 9), fill=colour)
    text((MX + 34, y), label, F_SMALL, anchor='lm')

# ---------------------------------------------------------------- LED matrix (corner shown)
LX, LY, P = 60, 170, 70                       # 5 x 5 LEDs drawn, pitch P
rows_shown = [('row 1 (red bar)', (255, 60, 60)), ('row 2 (yellow bar)', (255, 210, 60)),
              ('row 3 (green bar)', (60, 255, 80)), ('row 4 (game, red)', (255, 60, 60)),
              ('...', DIM)]
text((LX, LY - 85), 'LED screen: 27 rows x 16 columns = 432 LEDs', F_HEAD)
for r, (name, colour) in enumerate(rows_shown):
    y = LY + r * P
    line([(LX + 10, y), (LX + 380, y)], ORANGE, 2)                     # row line (anodes)
for c in range(5):
    x = LX + 40 + c * P
    line([(x + 12, LY - 20), (x + 12, LY + 5 * P - 30)], BLUE, 2)     # column line (cathodes)
for r in range(4):
    for c in range(5):
        x, y = LX + 40 + c * P, LY + r * P
        colour = rows_shown[r][1]
        d.ellipse((x - 9, y + 4, x + 9, y + 22), fill=colour if (r + c) % 3 == 0 else (70, 70, 70), outline=FG)
        line([(x, y), (x, y + 4)], ORANGE, 2)                          # anode leg to the row
        line([(x + 6, y + 18), (x + 12, y + 18)], BLUE, 2)             # cathode leg to the column
text((LX + 40, LY + 5 * P - 25), 'col 1   col 2   col 3   col 4   ...  col 16', F_SMALL, DIM)
# the bundles to the Mega
line([(LX + 380, LY), (LX + 380, MY + 200), (MX, MY + 200)], ORANGE, 6)
text((LX + 392, MY + 170), '27 row wires', F_SMALL, ORANGE)
line([(LX + 52, LY - 20), (LX + 52, LY - 40), (LX + 460, LY - 40), (LX + 460, MY + 90), (MX, MY + 90)], BLUE, 6)
text((LX + 300, LY - 62), '16 column wires', F_SMALL, BLUE)
text((LX, LY + 5 * P + 10),
     'Each LED: + leg on its row wire, - leg on its column wire.\n'
     'The Mega switches ONE row on at a time (71 times a second for\n'
     'the whole screen) and pulls the columns of the LEDs that\n'
     'should light LOW. Your eye sees a steady picture.', F, FG)

# ---------------------------------------------------------------- keypad
KX, KY, KP = 1000, 150, 70
text((KX, KY - 45), 'Keypad: %d push buttons, %d x %d' % (gc.KR * gc.KC, gc.KR, gc.KC), F_HEAD)
labels = [b[0] for row in gc.BUTTONS for b in row]
for r in range(gc.KR):
    y = KY + 20 + r * KP
    line([(KX - 30, y), (KX + gc.KC * KP - 20, y)], VIOLET, 2)
    text((KX + gc.KC * KP - 10, y), 'row %d -> pin %s' % (r + 1, gc.KEY_ROW_PINS[r]), F_SMALL, VIOLET, 'lm')
for c in range(gc.KC):
    x = KX + 20 + c * KP
    line([(x + 18, KY), (x + 18, KY + gc.KR * KP + 10)], GREEN, 2)
    text((x + 18, KY + gc.KR * KP + 14), 'pin %s' % gc.KEY_COL_PINS[c], F_SMALL, GREEN, 'ma')
for r in range(gc.KR):
    for c in range(gc.KC):
        x, y = KX + 20 + c * KP, KY + 20 + r * KP
        d.rounded_rectangle((x - 16, y - 16, x + 16, y + 16), 5, fill=(70, 70, 70), outline=FG)
        text((x, y), labels[r * gc.KC + c], F_HEAD, anchor='mm')
line([(KX - 30, KY + 20), (KX - 30, MY + 330), (MX + MW, MY + 330)], VIOLET, 6)
line([(KX + 38, KY + gc.KR * KP + 40), (KX + 38, MY + 390), (MX + MW, MY + 390)], GREEN, 6)
text((KX + 60, KY + gc.KR * KP + 45),
     'A button connects its row to its column.\n'
     'The Mega drives one row LOW at a time and\n'
     'reads the columns: a LOW column = that key\n'
     'is pressed.', F, FG)

# ---------------------------------------------------------------- buzzers
BXp, BYp = 1000, 700
jobs = ['lead', 'bass', 'effects', 'rhythm']
text((BXp, BYp - 110), 'Buzzers: 4, one hardware timer each', F_HEAD)
for i, pin in enumerate(gc.BUZZER_PINS):
    x = BXp + 20 + i * 115
    y = BYp - 18 - i * 12                       # each buzzer has its OWN wire to its own pin
    line([(x, BYp), (x, y), (MX + MW, y)], VIOLET, 3)
    d.ellipse((x - 26, BYp, x + 26, BYp + 52), fill=BLACK, outline=FG)
    text((x, BYp + 26), 'B%d' % (i + 1), F_HEAD, anchor='mm')
    line([(x, BYp + 52), (x, BYp + 80)], GREY, 3)                      # other leg: ground
    text((x + 6, BYp + 92), 'pin %s' % pin, F_SMALL, VIOLET)
    text((x + 6, BYp + 110), jobs[i], F_SMALL, DIM)
line([(BXp + 20, BYp + 80), (BXp + 20 + 3 * 115, BYp + 80)], GREY, 3)
line([(BXp + 20, BYp + 80), (MX + MW + 20, BYp + 80), (MX + MW + 20, MY + 545), (MX + MW, MY + 545)], GREY, 6)
text((BXp, BYp + 140),
     'Signal leg -> its own pin; each pin is the output of a timer\n'
     '(Timer3, Timer4, Timer1, Timer5). Other legs -> GND together.', F, FG)

# ---------------------------------------------------------------- legend
text((60, 660), 'Thick line = a bundle of separate wires side by side\n'
                '(one wire per LED row, LED column, key row or key column).', F, DIM)

text((30, H - 40), 'Parts: 1 Arduino Mega, 432 LEDs, 9 push buttons, 4 buzzers. No resistors, no modules, no libraries.',
     F, DIM)
out = Path(__file__).resolve().parents[1] / 'docs' / 'media' / 'wiring.png'
img.save(out)
print('wrote', out)
