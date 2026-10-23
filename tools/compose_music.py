#!/usr/bin/env python3
"""
VOID RUNNER - the in-game music (original metal tracks), written in a short notation and
turned into the note lists of void_runner/tunes.h (the block between the BEGIN/END
markers is replaced; the rest of tunes.h is not touched).

Usage: python tools/compose_music.py

Notation: 'E2:2' = note E2 for 2 ticks (1 tick = a 16th note), 'r:4' = rest. Bars are 16
ticks. The rhythm guitar (buzzer 4) plays a fifth above the bass in the same rhythm, so
bass + rhythm sound like a power chord.
"""
import re
from pathlib import Path

NAMES = {'C': 0, 'C#': 1, 'Db': 1, 'D': 2, 'D#': 3, 'Eb': 3, 'E': 4, 'F': 5, 'F#': 6, 'Gb': 6,
         'G': 7, 'G#': 8, 'Ab': 8, 'A': 9, 'A#': 10, 'Bb': 10, 'B': 11}


def midi(name):
    m = re.fullmatch(r'([A-G][#b]?)(\d)', name)
    return 12 * (int(m.group(2)) + 1) + NAMES[m.group(1)]


def parse(text):
    notes = []
    for tok in text.split():
        n, t = tok.split(':')
        notes.append((0 if n == 'r' else midi(n), int(t)))
    return notes


def gallop(n, beats=4):  # 8th + two 16ths per beat
    return ' '.join(['%s:2 %s:1 %s:1' % (n, n, n)] * beats)


def fifths(notes):
    return [(p + 7 if p else 0, t) for p, t in notes]


def bars(notes):
    total = sum(t for _, t in notes)
    return total / 16


SONGS = {}

# ---------------------------------------------------------------- phase 1: ROCK BELT
# Thrash, E minor, galloping bass (8th + two 16ths), tick 80 ms (about 190 BPM).
bass = ' '.join([
    gallop('E2'),
    gallop('E2', 2), 'G2:2 G2:1 G2:1 A2:2 A2:1 A2:1',
    gallop('E2'),
    gallop('E2', 2), 'Bb2:4 A2:4',
    gallop('E2'),
    gallop('E2', 2), 'G2:2 G2:1 G2:1 A2:2 A2:1 A2:1',
    gallop('C3', 2), gallop('B2', 2),
    gallop('B2', 2), 'B2:8',
])
lead = '''
E4:4 G4:2 A4:2 B4:4 A4:2 G4:2
A4:4 G4:2 E4:2 D4:4 E4:4
E4:4 G4:2 A4:2 B4:4 D5:2 E5:2
Bb4:4 A4:4 G4:4 F#4:4
E5:6 D5:2 B4:4 G4:4
A4:6 B4:2 G4:4 E4:4
E5:2 D5:2 B4:2 A4:2 G4:2 A4:2 B4:2 D5:2
F#4:8 B4:8
'''
SONGS['PHASE1'] = (80, parse(lead), parse(bass), 'Phase 1, ROCK BELT: thrash, E minor, galloping bass')

# ---------------------------------------------------------------- phase 2: DEEP MIST
# Slow and heavy, drop-D chugs (low D2), D minor lead with long notes, tick 100 ms.
chug_a = 'D2:1 D2:1 r:1 D2:1 D2:2 r:2 D2:1 D2:1 r:1 D2:1 F2:2 E2:2'
chug_b = 'D2:1 D2:1 r:1 D2:1 D2:2 r:2 A2:4 G#2:2 G2:2'
bass = ' '.join([chug_a, chug_b, chug_a, chug_b, chug_a, chug_b, chug_a, 'Bb2:8 A2:8'])
lead = '''
r:8 A4:4 F4:4
E4:8 D4:4 r:4
r:8 A4:4 C5:4
Bb4:8 A4:4 r:4
D5:6 C5:2 A4:4 F4:4
G4:6 A4:2 E4:8
F4:4 E4:4 D4:4 C#4:4
D4:12 r:4
'''
SONGS['PHASE2'] = (100, parse(lead), parse(bass), 'Phase 2, DEEP MIST: slow drop-D chugs, D minor')

# ---------------------------------------------------------------- phase 3: THE VOID
# Syncopated heavy groove with a catchy lead, E minor, tick 75 ms.
groove_x = 'E2:1 r:1 E2:1 r:1 E2:1 E2:1 r:2 G2:2 E2:2 Bb2:2 A2:2'
groove_y = 'E2:1 r:1 E2:1 r:1 E2:1 E2:1 r:2 C3:4 B2:4'
bass = ' '.join([groove_x, groove_y, groove_x, groove_y, groove_x, groove_y,
                 ' '.join(['E2:2'] * 8), ' '.join(['E2:1'] * 16)])
lead = '''
B4:4 B4:2 G4:2 A4:4 E4:4
B4:4 B4:2 G4:2 C5:4 B4:4
B4:4 B4:2 G4:2 A4:4 E4:2 G4:2
F#4:4 G4:4 A4:4 B4:4
E5:4 D5:2 B4:2 D5:4 E5:4
G5:4 F#5:2 E5:2 D5:4 B4:4
E5:2 B4:2 G4:2 E4:2 E5:2 B4:2 G4:2 E4:2
E5:1 D5:1 B4:1 A4:1 G4:1 A4:1 B4:1 D5:1 E5:8
'''
SONGS['PHASE3'] = (75, parse(lead), parse(bass), 'Phase 3, THE VOID: syncopated groove, E minor')

# ---------------------------------------------------------------- boss
# Tremolo-picked bass (16ths), chromatic wailing lead, tick 70 ms.
bass = ' '.join([' '.join(['E2:1'] * 16), ' '.join(['F2:1'] * 16),
                 ' '.join(['E2:1'] * 8 + ['G2:1'] * 4 + ['Bb2:1'] * 4),
                 ' '.join(['Bb2:1'] * 8 + ['A2:1'] * 8)])
lead = '''
E5:2 F5:2 E5:2 D#5:2 E5:8
F5:2 G5:2 F5:2 E5:2 F5:8
G5:2 A5:2 Bb5:2 A5:2 G5:4 E5:4
Bb4:4 A4:4 Ab4:4 G4:4
'''
SONGS['BOSS'] = (70, parse(lead), parse(bass), 'Boss fights: tremolo bass, chromatic lead')


def c_array(name, notes):
    rows, line = [], []
    for p, t in notes:
        line.append('{%d, %d}' % (p, t))
        if len(line) == 8:
            rows.append('  ' + ', '.join(line) + ',')
            line = []
    if line:
        rows.append('  ' + ', '.join(line) + ',')
    return 'const Note %s[] PROGMEM = {\n%s\n};' % (name, '\n'.join(rows))


out = []
for key, (tick, lead, bass, title) in SONGS.items():
    rhythm = fifths(bass)
    assert bars(lead) == bars(bass) == bars(rhythm), (key, bars(lead), bars(bass))
    secs = sum(t for _, t in bass) * tick / 1000
    out.append('// ================================================================= %s' % title)
    out.append('// %d bars, 1 tick = %d ms, one loop = %.1f s' % (bars(bass), tick, secs))
    out.append(c_array(key + '_LEAD', lead))
    out.append(c_array(key + '_BASS', bass))
    out.append(c_array(key + '_RHYTHM', rhythm))
    out.append('const Track SONG_%s[BUZZER_COUNT] = {TRACK(%s_LEAD), TRACK(%s_BASS), NO_TRACK, TRACK(%s_RHYTHM)};'
               % (key, key, key, key))
    out.append('const uint8_t SONG_%s_TICK_MS = %d;\n' % (key, tick))

BEGIN = '// ---- BEGIN songs generated by tools/compose_music.py (edit the songs there)\n'
END = '// ---- END songs generated by tools/compose_music.py\n'
path = Path(__file__).resolve().parents[1] / 'void_runner' / 'tunes.h'
text = path.read_text(encoding='utf-8')
start, stop = text.index(BEGIN) + len(BEGIN), text.index(END)
path.write_text(text[:start] + '\n'.join(out) + text[stop:], encoding='utf-8', newline='\n')
print('wrote %d songs into %s' % (len(SONGS), path))
