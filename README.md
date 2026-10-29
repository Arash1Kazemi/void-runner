# VOID RUNNER

A vertical space shooter for the Arduino Mega: a 16 × 24 screen of single LEDs plus three
status bars, a 3 × 3 keypad of push buttons and four buzzers (lead, bass, rhythm guitar,
sound effects). Three phases, three bosses,
three ships. Design and rules: [docs/SPEC.md](docs/SPEC.md). Pictures and animations of the
real screens: [docs/media/](docs/media/).

## Run it in VS Code (Wokwi)

1. Open this folder in VS Code.
2. `F1` → **Wokwi: Start Simulator** (the compiled program is in `build/`).
3. **Click an empty spot of the circuit**, then use the keyboard (English layout):

| Key | Does |
|---|---|
| W A S D | move (tap = 1 pixel, hold = glide) |
| R | fire (tap = 1 bullet, hold = auto-fire) · OK in menus |
| Q | back · pause in the game |
| P | pause / continue |
| M | sound on/off (any screen) |
| B | brightness 100 / 50 / 20 % (any screen) |

After changing the code, run `build.cmd` (or tell Claude) and restart the simulator.

## Run it on wokwi.com

New **Arduino Mega** project → paste `void_runner/void_runner.ino` into `sketch.ino`, paste
`diagram.json`, and add the other files of `void_runner/` with the editor's
"Upload file(s)..." menu.

## Please test (full game)

| # | Check |
|---|---|
| 1 | Logo + melody, any key skips it; title screen; after 20 s untouched a **demo** plays (any key stops it) |
| 2 | Menu → **SHIP**: A/D shows BOLT, DART, TWIN (status bars = speed / ammo / fire); R takes one |
| 3 | Menu → **OPTS**: W/S pages LITE, SND, DIFF, TEST; A/D changes; TEST runs the LED test |
| 4 | **PLAY**: the phase number and name appear as still pages (ZONE 1, ROCK BELT, GO!); moving and firing feel right (tap / hold / R rules) |
| 5 | Status bars: red = wave progress → boss health, yellow = ammo (blinks when low), green = lives |
| 6 | Ammo crates and extra lives can be caught; a big rock breaks into two |
| 7 | "WARN" + blinking warning sign + siren, then the boss: WARDEN (phase 1), HUNTER (fast, dives at you), VOID KING (changes form at half health) |
| 8 | P pauses (score shown), P continues, Q quits |
| 9 | Game over: score, NEW record, BEST page shows it |
| 10 | Is NORMAL too hard or too easy? Is anything confusing on the small screen? |
| 11 | Simulation speed (the % next to the timer), and the Serial line after typing `!s` in the terminal: `frame work us=` (CPU time of one picture, must stay well below 20000) |

Note: Wokwi starts every simulation with an empty EEPROM, so the high score and settings are
kept while it runs but not after a restart (on a real Arduino they would stay).

## Tests

```bash
python tests/host/run_tests.py
```

Builds the real game code for the PC (MSYS2 `g++`) and runs `tests/host/*.txt`: the key
rules, every screen, and a whole game. `--balance` also lets the autopilot play 48 games per
difficulty without the shield. No Wokwi minutes needed.

Wokwi scenarios (use the free plan's CLI minutes): `tests/scenarios/stage1_check.yaml`
(logo, title, menu, global keys) and `tests/scenarios/playthrough.yaml` (whole game):

```bash
wokwi-cli --timeout 120000 --scenario tests/scenarios/playthrough.yaml .
```

Serial test commands (type them in the Wokwi terminal, always with `!`): `!d` screen as text ·
`!s` state · `!1` `!2` `!3` start at a phase · `!b` skip to the boss · `!w` beat the boss ·
`!i` shield · `!x` autopilot · `!f` fast mode · `!k` lose the last life.

## Tools

- `tools/gen_circuit.py` – generates `diagram.json` and `void_runner/pins.h`
- `tools/gen_font.py` – generates `void_runner/font.h` from ASCII-art letters
- `tools/draw_wiring.py` – draws `docs/media/wiring.png`, a simplified wiring diagram
- `tools/compose_music.py` – the in-game songs in a short note notation; writes them into
  `void_runner/tunes.h` (listen to them: `docs/media/music/`)
- `tools/render_dumps.py` – turns screen dumps into a PNG or an animated GIF
- `tools/media_record.script` – PC-harness script that records the clips in `docs/media/`
- `tools/make_doc.py` – builds the project documentation `docs/VOID_RUNNER_documentation.docx`
  (open it with Google Docs; `tools/make_doc_fa.py` the Persian one, `..._fa.docx`);
  `tools/make_latex.py` builds the same as LaTeX + PDF in
  `docs/latex/`, `tools/make_latex_fa.py` in Persian in `docs/latex_fa/` (translations in
  `tools/fa_text.json`). Compile with XeLaTeX (on Overleaf: upload the folder, compiler XeLaTeX)
