"""
render_circuit.py -- a "photo" of the whole circuit as Wokwi draws it: the parts of the
generated circuit (tools/gen_circuit.py) with Wokwi's own part drawings (@wokwi/elements),
every visible wire, and the LEDs lit with one real game picture from a screen dump.

Usage (from the project folder):
    python tools/render_circuit.py <dump log> <dump number> <out.png> [zoom]
e.g. python tools/render_circuit.py build/rec_phase1.txt 120 docs/media/circuit_photo.png 2

Writes a web page next to the picture and lets Microsoft Edge (headless) take the screenshot;
the page loads @wokwi/elements from the internet.
"""
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EDGE = r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
SCREEN_W, SCREEN_H = 16, 24

spec = importlib.util.spec_from_file_location('gen_circuit', ROOT / 'tools' / 'gen_circuit.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)               # builds parts, wires and PIN_TABLE (no files written)


def read_dump(log, number):
    """Lit pixels of dump `number` (1 = first): {led id: True}."""
    blocks = Path(log).read_text(encoding='utf-8').split('[DUMP] begin')[1:]
    rows = [r for r in blocks[number - 1].splitlines()[1:] if r and not r.startswith('[')]
    bars, game = rows[:3], [r for r in rows[3:] if not r.startswith('-')]
    lit = {}
    for k, r in enumerate(bars):
        for x, c in enumerate(r):
            if c == '#':
                lit['st_%d_%d' % (x, k)] = True
    for y, r in enumerate(game[:SCREEN_H]):
        for x, c in enumerate(r):
            if c == '#':
                lit['led_%d_%d' % (x, y)] = True
    return lit


def page(lit, zoom):
    pts = list(gen.PIN_TABLE.values()) + [pt for w in gen.wires for pt in gen.polyline(w)]
    xs, ys = zip(*pts)
    x0, y0 = min(xs) - 80, min(ys) - 80
    w, h = max(xs) - x0 + 120, max(ys) - y0 + 120
    parts = []
    for p in gen.parts:
        a = dict(p.get('attrs', {}))
        tag = p['type']
        attrs = ''.join(' %s="%s"' % (k, v) for k, v in a.items() if k not in ('gamma', 'fps', 'key'))
        if tag == 'wokwi-led' and lit.get(p['id']):
            attrs += ' data-lit="1"'
        rot = p.get('rotate', 0)
        style = 'left:%.1fpx;top:%.1fpx;%s' % (p['left'] - x0, p['top'] - y0,
                                                'transform:rotate(%ddeg);' % rot if rot else '')
        parts.append('<%s class="p" style="%s"%s></%s>' % (tag, style, attrs, tag))
    lines = []
    for wire in gen.wires:
        if not wire['colour']:
            continue
        d = ' '.join('%.1f,%.1f' % (x - x0, y - y0) for x, y in gen.polyline(wire))
        lines.append('<polyline points="%s" stroke="%s"/>' % (d, wire['colour']))
    return f'''<!doctype html><html><head><meta charset="utf-8">
<style>html,body{{margin:0;background:#2b2b2b}}#c{{position:relative;width:{w:.0f}px;height:{h:.0f}px;
transform:scale({zoom});transform-origin:0 0}}.p{{position:absolute}}
svg{{position:absolute;left:0;top:0;pointer-events:none}}
polyline{{fill:none;stroke-width:2.2;stroke-linejoin:round;opacity:.9}}</style>
<script type="module">import 'https://cdn.jsdelivr.net/npm/@wokwi/elements@1.9.2/+esm';
customElements.whenDefined('wokwi-led').then(() =>
  document.querySelectorAll('[data-lit]').forEach(e => e.value = true));</script>
</head><body><div id="c">{''.join(parts)}
<svg width="{w:.0f}" height="{h:.0f}">{''.join(lines)}</svg></div></body></html>''', w * zoom, h * zoom


def main():
    log, number, out = sys.argv[1], int(sys.argv[2]), Path(sys.argv[3]).resolve()
    zoom = float(sys.argv[4]) if len(sys.argv) > 4 else 2
    html, w, h = page(read_dump(log, number), zoom)
    page_file = out.with_suffix('.html')
    page_file.write_text(html, encoding='utf-8', newline='\n')
    subprocess.run([EDGE, '--headless=new', '--disable-gpu', '--hide-scrollbars',
                    '--virtual-time-budget=15000', '--window-size=%d,%d' % (w, h),
                    '--screenshot=%s' % out, page_file.as_uri()], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    page_file.unlink()
    print('wrote', out, '%dx%d' % (w, h))


if __name__ == '__main__':
    main()
