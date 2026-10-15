#!/usr/bin/env python3
"""
VOID RUNNER - turns screen dumps ("!d" command, or the PC harness's "record") into
pictures in the real LED colours of the circuit (from gen_circuit.py). Useful to review
animation frames without watching the simulation, and for the documentation.

Usage:
    python tools/render_dumps.py <log> <out.png>               all screens side by side
    python tools/render_dumps.py <log> <out.png> 3 7 12         only screens 3, 7 and 12
    python tools/render_dumps.py <log> <out.gif> [ms]           animation (needs Pillow),
                                                                ms per frame (default 40)
"""
import re
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import gen_circuit as gc                    # LED colours and screen size

BACKGROUND = (40, 40, 40)


def hexrgb(s):
    return tuple(int(s[i:i + 2], 16) for i in (1, 3, 5))


def read_dumps(text):
    dumps, cur = [], None
    for line in re.sub(r'\[DUMP\]', '\n[DUMP]', text).splitlines():
        line = line.strip()
        if line.startswith('[DUMP] begin'):
            cur = []
        elif line.startswith('[DUMP] end') and cur is not None:
            dumps.append(cur)
            cur = None
        elif cur is not None and len(line) == gc.SCREEN_W and set(line) <= set('.#'):
            cur.append(line)
    return [d for d in dumps if len(d) == gc.TOTAL_ROWS]


def draw_screen(img, dump, ox, oy, pitch):
    """Paints one screen into img (a list of pixel rows) with its top-left at (ox, oy)."""
    r_on, r_off = pitch * 5 // 14, pitch * 4 // 14
    for i, line in enumerate(dump):                     # dump order: status bars, then game rows
        row = gc.SCREEN_H + i if i < gc.STATUS_ROWS else i - gc.STATUS_ROWS
        light, body = gc.led_colours(row)
        gy = i if i < gc.STATUS_ROWS else i + 1          # one empty row under the status bars
        for x, ch in enumerate(line):
            c = hexrgb(light) if ch == '#' else hexrgb(body)
            cx, cy = ox + x * pitch + pitch // 2, oy + gy * pitch + pitch // 2
            r = r_on if ch == '#' else r_off
            for y in range(cy - r, cy + r + 1):
                for xx in range(cx - r, cx + r + 1):
                    if (xx - cx) ** 2 + (y - cy) ** 2 <= r * r:
                        img[y][xx] = c


def screen_size(pitch):
    return gc.SCREEN_W * pitch, (gc.TOTAL_ROWS + 1) * pitch


def render(dumps, path, pitch=14, gap=24):
    fw, fh = screen_size(pitch)
    W, H = len(dumps) * (fw + gap) + gap, fh + 2 * gap
    img = [[BACKGROUND] * W for _ in range(H)]
    for n, dump in enumerate(dumps):
        draw_screen(img, dump, gap + n * (fw + gap), gap, pitch)
    raw = b''.join(b'\x00' + bytes(v for px in row for v in px) for row in img)
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
    Path(path).write_bytes(png + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def render_gif(dumps, path, ms=40, pitch=10, gap=10):
    from PIL import Image                           # Pillow: pip install pillow
    fw, fh = screen_size(pitch)
    W, H = fw + 2 * gap, fh + 2 * gap
    frames = []
    for dump in dumps:
        img = [[BACKGROUND] * W for _ in range(H)]
        draw_screen(img, dump, gap, gap, pitch)
        frame = Image.new('RGB', (W, H))
        frame.putdata([px for row in img for px in row])
        frames.append(frame.convert('P', palette=Image.ADAPTIVE, colors=32))
    frames[0].save(path, save_all=True, append_images=frames[1:], duration=ms, loop=0, optimize=True)


if __name__ == '__main__':
    dumps = read_dumps(Path(sys.argv[1]).read_text(encoding='utf-8', errors='replace'))
    out = sys.argv[2]
    if out.endswith('.gif'):
        render_gif(dumps, out, int(sys.argv[3]) if len(sys.argv) > 3 else 40)
    else:
        if len(sys.argv) > 3:                       # pick screens by number (1 = first)
            dumps = [dumps[int(n) - 1] for n in sys.argv[3:]]
        render(dumps, out)
    print('rendered %d screens to %s' % (len(dumps), out))
