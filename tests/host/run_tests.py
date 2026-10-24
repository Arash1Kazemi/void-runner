#!/usr/bin/env python3
"""
VOID RUNNER - automated tests on the PC (no Wokwi minutes needed).

Builds tests/host/sim.cpp (the real game code + a PC stand-in for Arduino) with g++ and
runs every test script in tests/host/*.txt. Each script presses keys, sends Serial test
commands and waits for the game's Serial messages; see sim.cpp for the commands.

    python tests/host/run_tests.py              build + run all test scripts
    python tests/host/run_tests.py --balance    also: autopilot games without the shield
                                                on EASY / NORMAL / HARD (wins per level)

Needs g++ (MSYS2 UCRT64 on this PC: C:\\msys64\\ucrt64\\bin). Logs go to build/.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HOST = ROOT / 'tests' / 'host'
BUILD = ROOT / 'build'
EXE = '.exe' if os.name == 'nt' else ''


def find_gpp():
    gpp = shutil.which('g++')
    if not gpp and Path(r'C:\msys64\ucrt64\bin\g++.exe').exists():
        gpp = r'C:\msys64\ucrt64\bin\g++.exe'
    if not gpp:
        sys.exit('g++ not found (install MSYS2 UCRT64 or add it to PATH)')
    return gpp


def build(name, defines=()):
    exe = BUILD / (name + EXE)
    cmd = [find_gpp(), '-std=c++17', '-O2', '-static', '-I', str(HOST),
           *('-D' + d for d in defines), '-o', str(exe), str(HOST / 'sim.cpp')]
    subprocess.run(cmd, check=True, cwd=ROOT)
    return exe


def run(exe, script_text, log):
    script = BUILD / (log.stem + '.script')
    script.write_text(script_text, encoding='utf-8')
    result = subprocess.run([str(exe), str(script)], capture_output=True, text=True, cwd=ROOT)
    log.write_text(result.stdout, encoding='utf-8')
    return result.returncode == 0, result.stdout


def run_tests():
    exe = build('sim')
    failed = 0
    for script in sorted(HOST.glob('*.txt')):
        ok, out = run(exe, script.read_text(encoding='utf-8'), BUILD / ('test_' + script.stem + '.log'))
        last = [l for l in out.splitlines() if l.startswith('[SIM]')][-1:]
        print('%-6s %-12s %s' % ('PASS' if ok else 'FAIL', script.stem, last[0] if last else ''))
        failed += not ok
    return failed


# Autopilot games without the shield. The autopilot here "thinks" only every 250 ms,
# a rough stand-in for a human's reaction time, so the result says how hard each level is.
def balance(seeds=48, react_ms=250):
    exe = build('sim_balance', ['AUTOPILOT_REACT_MS=%d' % react_ms])
    for level, key in (('EASY', 'tap A'), ('NORMAL', ''), ('HARD', 'tap D')):
        wins, bosses = 0, []
        for seed in range(1, seeds + 1):
            script = '\n'.join([
                'seed %d' % seed, 'until "[VR] screen logo"', 'tap R', 'until "[VR] screen title"',
                'tap R', 'until "[VR] screen menu"', 'tap S', 'tap S', 'tap R',
                'until "[VR] screen options"', 'tap S', 'tap S', key, 'tap Q',
                'until "[VR] screen menu"', 'send !x', 'send !1', 'until "[VR] phase 1"',
                'until "[VR] screen victory" 900000', ''])
            ok, out = run(exe, script, BUILD / 'balance.log')
            wins += ok
            bosses.append(out.count('[VR] boss down'))
        print('%-6s wins %2d/%d   bosses beaten per game: %s' % (level, wins, seeds, ' '.join(map(str, bosses))))


if __name__ == '__main__':
    BUILD.mkdir(exist_ok=True)
    failed = run_tests()
    if '--balance' in sys.argv:
        balance()
    sys.exit(1 if failed else 0)
