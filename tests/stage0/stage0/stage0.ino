/*
 * VOID RUNNER - Stage 0 feasibility test (NOT the game).
 *
 * Hardware (pins.h is generated together with diagram.json by tools/gen_circuit.py):
 *   - 16 x 24 game screen + 3 status bars of single LEDs, wired as ONE matrix:
 *     27 row lines (LED anodes) and 16 column lines (LED cathodes)
 *   - 4 x 4 push-button keypad, every button also bound to a PC keyboard key
 *   - 3 buzzers, each driven by its own hardware timer
 *
 * What it checks:
 *   1. Wokwi can run 432 LEDs at full speed
 *   2. Lighting one row at a time (scanning) looks steady and bright enough
 *   3. Buttons: press, release, two at once, debouncing (Wokwi simulates bounce)
 *   4. Three buzzers playing together while the screen is being scanned
 *   5. CPU time of the scan: Arduino digitalWrite() vs. direct port writes
 *
 * Keys:   E = next test pattern   B = scan method   M = sound mode
 *         F = refresh rate (71 / 50 / 35 pictures per second)   X = scanning on/off
 * Serial: !p / !s / !m / !f / !x do the same, !d prints the screen as text
 * Once per second a [STAT] line reports the measurements.
 */
#include "pins.h"

// ================================================================= screen buffers
// One 16-bit word per LED row, one bit per LED: bit x is column x, which is wired to
// analog pin Ax (see pins.h), so a row word can be written to PORTF / PORTK directly.
uint16_t drawBuf[TOTAL_ROWS];            // the main loop draws here
volatile uint16_t showBuf[TOTAL_ROWS];   // the scan interrupt shows this one

void clearScreen() {
  memset(drawBuf, 0, sizeof(drawBuf));
}

void setPixel(uint8_t x, uint8_t row, bool on) {
  if (x >= SCREEN_W || row >= TOTAL_ROWS) return;
  uint16_t bit = 1u << x;
  if (on) drawBuf[row] |= bit;
  else drawBuf[row] &= ~bit;
}

bool getPixel(uint8_t x, uint8_t row) {
  return drawBuf[row] & (1u << x);
}

// Copies the finished picture to the scan buffer in one go, so the interrupt
// never shows a half-drawn picture.
void present() {
  noInterrupts();
  for (uint8_t r = 0; r < TOTAL_ROWS; r++) showBuf[r] = drawBuf[r];
  interrupts();
}

// ================================================================= scanning (Timer2 interrupt)
// At every Timer2 interrupt one LED row is switched off and the next one on.
// Timer2 counts at 16 MHz / 256 = one count every 16 us. Wokwi spends time on every
// timer count, so this slow clock keeps the simulation fast (measured in Stage 0).
const uint8_t TICK_US = 16;
const uint8_t REFRESH_HZ[] = {71, 50, 35};        // pictures per second (key F)
uint8_t refreshIndex = 0;

uint8_t refreshCounts(uint8_t hz) {               // Timer2 counts per row, minus one
  return 1000000UL / ((uint32_t)hz * TOTAL_ROWS * TICK_US) - 1;
}
bool scanEnabled = true;
volatile uint8_t scanRow = 0;
volatile bool scanWithPorts = true;      // true = PORTF / PORTK (3% CPU), false = digitalWrite() (35%)
volatile uint8_t scanMaxTicks = 0;       // longest interrupt (in TICK_US units)
volatile uint32_t scanSumTicks = 0;
volatile uint16_t scanCount = 0;

ISR(TIMER2_COMPA_vect) {
  uint8_t start = TCNT2;
  uint8_t row = scanRow;
  digitalWrite(ROW_PINS[row], LOW);              // 1. old row off: nothing lit while columns change
  row = (row + 1 == TOTAL_ROWS) ? 0 : row + 1;
  uint16_t bits = showBuf[row];
  if (scanWithPorts) {                           // 2. columns of the new row (LOW = LED on)
    PORTF = (uint8_t)~lowByte(bits);             //    A0..A7 in one step
    PORTK = (uint8_t)~highByte(bits);            //    A8..A15 in one step
  } else {
    for (uint8_t x = 0; x < SCREEN_W; x++)
      digitalWrite(COL_PINS[x], (bits & (1u << x)) ? LOW : HIGH);
  }
  digitalWrite(ROW_PINS[row], HIGH);             // 3. new row on
  scanRow = row;
  uint8_t end = TCNT2;                           // the counter may have restarted meanwhile
  uint8_t ticks = (end >= start) ? end - start : end + (OCR2A + 1) - start;
  if (ticks > scanMaxTicks) scanMaxTicks = ticks;
  scanSumTicks += ticks;
  scanCount++;
}

void scanBegin() {
  for (uint8_t r = 0; r < TOTAL_ROWS; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], LOW);
  }
  for (uint8_t x = 0; x < SCREEN_W; x++) {
    pinMode(COL_PINS[x], OUTPUT);
    digitalWrite(COL_PINS[x], HIGH);
  }
  TCCR2A = _BV(WGM21);                 // CTC mode: count 0..OCR2A, then start again
  TCCR2B = _BV(CS22) | _BV(CS21);      // prescaler 256: one count = 16 us
  OCR2A = refreshCounts(REFRESH_HZ[refreshIndex]);
  TIMSK2 = _BV(OCIE2A);                // interrupt at every match
}

void setScanEnabled(bool on) {         // off = no interrupt and every LED dark (speed baseline)
  scanEnabled = on;
  TIMSK2 = on ? _BV(OCIE2A) : 0;
  if (!on)
    for (uint8_t r = 0; r < TOTAL_ROWS; r++) digitalWrite(ROW_PINS[r], LOW);
}

// ================================================================= keypad
// Same method as the course: drive one row LOW and read the columns (INPUT_PULLUP,
// so a pressed key reads LOW). Rows that are not being read are set to INPUT
// instead of HIGH: with two keys of one column pressed, a HIGH and a LOW output
// would otherwise be shorted together through the buttons.
uint16_t keysHeld = 0;                 // debounced, bit (row * 4 + column)
int8_t lastKey = -1;                   // newest key that is still held

uint16_t keysRaw() {
  uint16_t mask = 0;
  for (uint8_t r = 0; r < KEY_ROWS; r++) {
    pinMode(KEY_ROW_PINS[r], OUTPUT);
    digitalWrite(KEY_ROW_PINS[r], LOW);
    delayMicroseconds(5);
    for (uint8_t c = 0; c < KEY_COLS; c++)
      if (digitalRead(KEY_COL_PINS[c]) == LOW) mask |= 1u << (r * KEY_COLS + c);
    pinMode(KEY_ROW_PINS[r], INPUT);
  }
  return mask;
}

char keyName(uint8_t i) {
  return KEY_NAMES[i / KEY_COLS][i % KEY_COLS];
}

// ================================================================= buzzers
// Each buzzer pin is the "A" output of a 16-bit timer. In CTC mode with "toggle on
// compare match" the timer flips the pin by itself: a note costs no CPU time and the
// screen interrupt cannot disturb it. f = 16 MHz / (2 * 256 * (OCRnA + 1)).
// The timer clock is /256: with /8 Wokwi ran 2.5x slower (measured), and /256 still
// hits every note within 1 %.
struct TimerRegs {
  volatile uint8_t* tccrA;
  volatile uint8_t* tccrB;
  volatile uint16_t* ocrA;
  volatile uint16_t* tcnt;
};
const TimerRegs BUZZER_TIMER[BUZZER_COUNT] = {
  { &TCCR3A, &TCCR3B, &OCR3A, &TCNT3 },    // pin 5  = OC3A
  { &TCCR4A, &TCCR4B, &OCR4A, &TCNT4 },    // pin 6  = OC4A
  { &TCCR1A, &TCCR1B, &OCR1A, &TCNT1 },    // pin 11 = OC1A
};
uint16_t voiceHz[BUZZER_COUNT];          // note playing on each buzzer (0 = silent)

void buzzerTone(uint8_t v, uint16_t hz) {
  if (hz == voiceHz[v]) return;
  voiceHz[v] = hz;
  const TimerRegs& t = BUZZER_TIMER[v];
  *t.tccrB = 0;                                  // stop the timer while changing it
  if (hz == 0) {
    *t.tccrA = 0;                                // disconnect the pin from the timer
    digitalWrite(BUZZER_PINS[v], LOW);
    return;
  }
  *t.tccrA = _BV(COM1A0);                        // mode bits first: leave Arduino's PWM mode,
  *t.tcnt = 0;                                   // where a new OCRnA would wait for a cycle end
  *t.ocrA = F_CPU / (2UL * 256 * hz) - 1;
  *t.tccrB = _BV(WGM12) | _BV(CS12);             // CTC mode, prescaler 256
}

const uint16_t MELODY[] = {523, 659, 784, 1047, 784, 659};
const uint16_t CHORDS[4][BUZZER_COUNT] = {        // C, Am, F, G
  {262, 330, 392}, {220, 262, 330}, {175, 220, 262}, {196, 247, 294}
};
uint8_t soundMode = 2;                           // 0 off, 1 = one buzzer, 2 = three buzzers

void soundUpdate(uint32_t now) {
  for (uint8_t v = 0; v < BUZZER_COUNT; v++) {
    uint16_t hz = 0;
    if (soundMode == 1 && v == 0) hz = MELODY[(now / 150) % 6];
    if (soundMode == 2) hz = CHORDS[(now / 500) % 4][v];
    buzzerTone(v, hz);
  }
}

// ================================================================= test patterns
const uint8_t PATTERNS = 4;      // 0 test card, 1 all LEDs on, 2 checkerboard, 3 one LED at a time
uint8_t pattern = 0;

void statusBar(uint8_t bar, uint8_t level) {     // light `level` LEDs from the left
  for (uint8_t x = 0; x < SCREEN_W; x++) setPixel(x, SCREEN_H + bar, x < level);
}

void drawTestCard(uint32_t now) {
  for (uint8_t x = 0; x < SCREEN_W; x++) {       // border around the game screen
    setPixel(x, 0, true);
    setPixel(x, SCREEN_H - 1, true);
  }
  for (uint8_t y = 0; y < SCREEN_H; y++) {
    setPixel(0, y, true);
    setPixel(SCREEN_W - 1, y, true);
  }
  for (uint8_t y = 1; y < 4; y++)                // 3x3 block: marks the top-left corner
    for (uint8_t x = 1; x < 4; x++) setPixel(x, y, true);
  const uint8_t BLOCK_X[KEY_COLS] = {1, 5, 9, 12};   // 3x3 blocks inside the border
  for (uint8_t i = 0; i < 16; i++)               // keypad mirror: a held key lights its block
    if (keysHeld & (1u << i))
      for (uint8_t dy = 0; dy < 3; dy++)
        for (uint8_t dx = 0; dx < 3; dx++)
          setPixel(BLOCK_X[i % KEY_COLS] + dx, 5 + (i / KEY_COLS) * 4 + dy, true);
  uint8_t bx = 1 + (now / 70) % 26;              // a dot bouncing along the bottom
  setPixel(bx < 14 ? bx : 27 - bx, SCREEN_H - 2 - (now / 110) % 3, true);
  statusBar(0, (now / 150) % 17);                // status bars: fill up, empty, walking dot
  statusBar(1, 16 - (now / 150) % 17);
  setPixel((now / 100) % SCREEN_W, SCREEN_H + 2, true);
}

void drawFrame(uint32_t now) {
  clearScreen();
  if (pattern == 0) drawTestCard(now);
  if (pattern == 1)
    for (uint8_t r = 0; r < TOTAL_ROWS; r++) drawBuf[r] = 0xFFFF;
  if (pattern == 2)
    for (uint8_t r = 0; r < TOTAL_ROWS; r++) drawBuf[r] = (r & 1) ? 0xAAAA : 0x5555;
  if (pattern == 3) {
    uint16_t i = (now / 15) % (TOTAL_ROWS * SCREEN_W);
    setPixel(i % SCREEN_W, i / SCREEN_W, true);
  }
}

void dumpScreen() {                              // the picture as text, for automated checks
  Serial.println(F("[DUMP] begin"));
  for (uint8_t r = 0; r < TOTAL_ROWS; r++) {
    if (r == SCREEN_H) Serial.println(F("------ status bars ------"));
    for (uint8_t x = 0; x < SCREEN_W; x++) Serial.print(getPixel(x, r) ? '#' : '.');
    Serial.println();
  }
  Serial.println(F("[DUMP] end"));
}

// ================================================================= actions (keys and serial)
void nextPattern() {
  pattern = (pattern + 1) % PATTERNS;
  Serial.print(F("[S0] pattern ")); Serial.println(pattern);
}

void toggleScanMethod() {
  scanWithPorts = !scanWithPorts;
  Serial.print(F("[S0] scan method ")); Serial.println(scanWithPorts ? F("ports") : F("digitalWrite"));
}

void nextSoundMode() {
  soundMode = (soundMode + 1) % 3;
  Serial.print(F("[S0] sound mode ")); Serial.println(soundMode);
}

void nextRefreshRate() {
  refreshIndex = (refreshIndex + 1) % sizeof(REFRESH_HZ);
  TCNT2 = 0;
  OCR2A = refreshCounts(REFRESH_HZ[refreshIndex]);
  Serial.print(F("[S0] refresh ")); Serial.print(REFRESH_HZ[refreshIndex]); Serial.println(F(" Hz"));
}

void toggleScanning() {
  setScanEnabled(!scanEnabled);
  Serial.print(F("[S0] scanning ")); Serial.println(scanEnabled ? F("on") : F("off"));
}

void onKeyDown(char k) {
  if (k == 'E') nextPattern();
  if (k == 'B') toggleScanMethod();
  if (k == 'M') nextSoundMode();
  if (k == 'F') nextRefreshRate();
  if (k == 'X') toggleScanning();
}

void printHeld() {
  Serial.print(F(" held:"));
  if (!keysHeld) Serial.print('-');
  for (uint8_t i = 0; i < 16; i++)
    if (keysHeld & (1u << i)) Serial.print(keyName(i));
  Serial.print(F(" last:"));
  Serial.println(lastKey >= 0 ? keyName(lastKey) : '-');
}

// Scans every 5 ms; a change counts once the keys read the same 4 times in a row,
// which hides the contact bounce that Wokwi simulates.
void keypadUpdate(uint32_t now) {
  static uint32_t lastScan = 0;
  static uint16_t candidate = 0;
  static uint8_t sameScans = 0;
  if (now - lastScan < 5) return;
  lastScan = now;
  uint16_t raw = keysRaw();
  if (raw != candidate) {
    candidate = raw;
    sameScans = 0;
    return;
  }
  if (sameScans < 3) sameScans++;
  if (sameScans < 3 || candidate == keysHeld) return;
  uint16_t changed = candidate ^ keysHeld;
  keysHeld = candidate;
  for (uint8_t i = 0; i < 16; i++) {
    if (!(changed & (1u << i))) continue;
    bool down = keysHeld & (1u << i);
    if (down) lastKey = i;
    else if (lastKey == (int8_t)i) lastKey = -1;
    Serial.print(F("[KEY] t="));
    Serial.print(now);
    Serial.print(down ? F(" DOWN ") : F(" UP   "));
    Serial.print(keyName(i));
    printHeld();
    if (down) onKeyDown(keyName(i));
  }
}

// Test commands over Serial need a '!' first (for example "!p"), so letters typed by
// accident into the Wokwi terminal do nothing. Such letters usually mean the keyboard is
// talking to the terminal instead of the buttons, so the sketch says so.
void serialCommands() {
  static bool commandNext = false;
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '!') {
      commandNext = true;
      continue;
    }
    if (!commandNext) {
      if (isAlpha(c))
        Serial.println(F("[S0] keys are going to the Serial terminal: click an empty spot of the circuit first"));
      continue;
    }
    commandNext = false;
    if (c == 'p') nextPattern();
    if (c == 's') toggleScanMethod();
    if (c == 'm') nextSoundMode();
    if (c == 'f') nextRefreshRate();
    if (c == 'x') toggleScanning();
    if (c == 'd') dumpScreen();
  }
}

// ================================================================= measurements
extern char* __brkval;
extern char __heap_start;
int freeRam() {
  char top;
  return &top - (__brkval ? __brkval : &__heap_start);
}

void report(uint32_t now) {
  static uint32_t lastReport = 0, loops = 0;
  loops++;
  if (now - lastReport < 1000) return;
  noInterrupts();
  uint32_t sum = scanSumTicks;
  uint16_t count = scanCount;
  uint8_t maxTicks = scanMaxTicks;
  scanSumTicks = 0;
  scanCount = 0;
  scanMaxTicks = 0;
  interrupts();
  uint32_t span = now - lastReport;
  Serial.print(F("[STAT] t=")); Serial.print(now / 1000);
  Serial.print(F(" refresh_hz=")); Serial.print(count * 1000UL / span / TOTAL_ROWS);
  Serial.print(F(" scan_us_avg=")); Serial.print(count ? sum * TICK_US / count : 0);
  Serial.print(F(" scan_us_max=")); Serial.print(maxTicks * TICK_US);
  Serial.print(F(" scan_cpu%=")); Serial.print(sum * TICK_US / (span * 10));
  Serial.print(F(" loops_per_s=")); Serial.print(loops * 1000UL / span);
  Serial.print(F(" scan=")); Serial.print(scanWithPorts ? F("ports") : F("digitalWrite"));
  Serial.print(F(" sound=")); Serial.print(soundMode);
  Serial.print(F(" pattern=")); Serial.print(pattern);
  Serial.print(F(" freeRAM=")); Serial.println(freeRam());
  lastReport = now;
  loops = 0;
}

// ================================================================= main
void setup() {
  Serial.begin(115200);
  for (uint8_t r = 0; r < KEY_ROWS; r++) pinMode(KEY_ROW_PINS[r], INPUT);
  for (uint8_t c = 0; c < KEY_COLS; c++) pinMode(KEY_COL_PINS[c], INPUT_PULLUP);
  for (uint8_t v = 0; v < BUZZER_COUNT; v++) pinMode(BUZZER_PINS[v], OUTPUT);
  // Arduino starts timers 1, 3, 4 and 5 even when nothing uses them, and Wokwi spends
  // time on every timer count: stop them. The buzzer timers restart when a note plays.
  TCCR1B = 0;
  TCCR3B = 0;
  TCCR4B = 0;
  TCCR5B = 0;
  scanBegin();
  Serial.println(F("[S0] VOID RUNNER stage 0: 16x24 LEDs + 3 status bars, 4x4 keys, 3 buzzers"));
  Serial.println(F("[S0] keys: E = pattern, B = scan method, M = sound, F = refresh rate, X = scanning"));
  Serial.println(F("[S0] ready"));
}

void loop() {
  uint32_t now = millis();
  keypadUpdate(now);
  serialCommands();
  soundUpdate(now);
  static uint32_t lastFrame = 0;
  if (now - lastFrame >= 20) {           // draw 50 pictures per second
    lastFrame = now;
    drawFrame(now);
    present();
  }
  report(now);
}
