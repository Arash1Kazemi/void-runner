/*
 * display.h -- the LED screen.
 *
 * 16 x 24 game screen + 3 status bars of single LEDs, wired as ONE matrix:
 * 27 rows (LED anodes, pins 22-48) and 16 columns (LED cathodes, A0-A15).
 *
 * The picture lives in `frame`: one 16-bit word per LED row, bit x = column x.
 * A Timer2 interrupt lights one row at a time; all 27 rows are shown 71 times per
 * second, so the picture stays steady whatever the game code is doing.
 *
 * Blinking: Wokwi shows each LED's brightness averaged over 100 ms, so anything that
 * should visibly blink stays on and off for longer than that (frameNo & 8 = 160 ms).
 */
#pragma once
#include "pins.h"
#include "config.h"
#include "font.h"

uint16_t frame[TOTAL_ROWS];              // the game draws here
volatile uint16_t shown[TOTAL_ROWS];     // the scan interrupt shows this copy
volatile uint8_t scanRow = 0;            // row that is lit right now
const uint8_t SCAN_TICK_US = 16;         // Timer2 runs at 16 MHz / 256

// ================================================================= scanning
// Every Timer2 match: old row off, columns of the next row, next row on.
// Column x is wired to pin Ax, so A0-A7 are the bits of PORTF and A8-A15 the bits of
// PORTK: 8 columns are written in one instruction (12 us per row instead of 140 us with
// digitalWrite, measured in Stage 0).
ISR(TIMER2_COMPA_vect) {
  uint8_t row = scanRow;
  digitalWrite(ROW_PINS[row], LOW);
  row = (row + 1 == TOTAL_ROWS) ? 0 : row + 1;
  uint16_t bits = shown[row];
  PORTF = (uint8_t)~lowByte(bits);       // a LOW column lights its LED
  PORTK = (uint8_t)~highByte(bits);
  digitalWrite(ROW_PINS[row], HIGH);
  scanRow = row;
}

// Brightness: a second match part way through the time slot switches the row off early.
ISR(TIMER2_COMPB_vect) {
  digitalWrite(ROW_PINS[scanRow], LOW);
}

// Starts the LED screen: row and column pins as outputs with every LED off, then Timer2
// in CTC mode, so the scan interrupt runs 27 x 71 times per second.
void displayBegin() {
  for (uint8_t r = 0; r < TOTAL_ROWS; r++) {
    pinMode(ROW_PINS[r], OUTPUT);
    digitalWrite(ROW_PINS[r], LOW);
  }
  for (uint8_t x = 0; x < SCREEN_W; x++) {
    pinMode(COL_PINS[x], OUTPUT);
    digitalWrite(COL_PINS[x], HIGH);
  }
  TCCR2A = _BV(WGM21);                   // CTC mode: count 0..OCR2A, then start again
  TCCR2B = _BV(CS22) | _BV(CS21);        // prescaler 256: one count = 16 us
  OCR2A = 1000000UL / ((uint32_t)REFRESH_HZ * TOTAL_ROWS * SCAN_TICK_US) - 1;
  TIMSK2 = _BV(OCIE2A);
}

// Sets how long each row stays lit inside its time slot (100 = all of it). Below 100 %
// the second Timer2 match (COMPB) switches the row off early.
void displaySetBrightness(uint8_t percent) {
  if (percent >= 100) {
    TIMSK2 &= ~_BV(OCIE2B);
    return;
  }
  OCR2B = (uint16_t)(OCR2A + 1) * percent / 100;
  TIMSK2 |= _BV(OCIE2B);
}

// Copies the finished picture to the scan buffer in one go, so the interrupt never
// shows a half-drawn picture.
void present() {
  noInterrupts();
  for (uint8_t r = 0; r < TOTAL_ROWS; r++) shown[r] = frame[r];
  interrupts();
}

// ================================================================= drawing (game screen)
// Switches off every LED of the game screen in `frame` (the status bars stay).
void clearScreen() {
  for (uint8_t y = 0; y < SCREEN_H; y++) frame[y] = 0;
}

int8_t shakeX = 0, shakeY = 0;           // screen shake: everything is drawn shifted

// Turns one LED of the game screen on or off in `frame`, shifted by the screen shake.
// Pixels outside the screen are ignored, so sprites may stick out at the edges.
void setPixel(int16_t x, int16_t y, bool on) {
  x += shakeX;
  y += shakeY;
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  if (on) frame[y] |= 1u << x;
  else frame[y] &= ~(1u << x);
}

// Turns a w x h block of LEDs on or off (bars, ribbons, clearing an area).
void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool on) {
  for (int16_t j = y; j < y + h; j++)
    for (int16_t i = x; i < x + w; i++) setPixel(i, j, on);
}

// Bitmaps are stored in flash, one 16-bit word per row, leftmost pixel = top bit.
void drawBitmap(int16_t x, int16_t y, const uint16_t* rows, uint8_t w, uint8_t h, bool on) {
  for (uint8_t r = 0; r < h; r++) {
    uint16_t bits = pgm_read_word(rows + r);
    for (uint8_t c = 0; c < w; c++)
      if (bits & (0x8000u >> c)) setPixel(x + c, y + r, on);
  }
}

// ================================================================= text
// small = 3x5 font (4 pixels per letter), big = 5x7 font (6 pixels per letter).
// Strings are kept in flash: call with PSTR("TEXT").
// Draws one letter or digit with its top-left at (x, y); unknown characters are skipped.
// scale = 2 draws every pixel as a 2 x 2 block (the big phase number).
void drawChar(int16_t x, int16_t y, char ch, bool big, uint8_t scale = 1) {
  const char* chars = big ? FONT5x7_CHARS : FONT3x5_CHARS;
  const char* hit = ch ? strchr_P(chars, ch) : NULL;
  if (!hit) return;
  uint8_t w = big ? 5 : 3, h = big ? 7 : 5;
  const uint8_t* glyph = (big ? FONT5x7 : FONT3x5) + (hit - chars) * w;
  for (uint8_t c = 0; c < w; c++) {
    uint8_t bits = pgm_read_byte(glyph + c);
    for (uint8_t r = 0; r < h; r++)
      if (bits & (1 << r)) fillRect(x + c * scale, y + r * scale, scale, scale, true);
  }
}

// Width in pixels of a text in the small or big font (for centring).
int16_t textWidth(const char* text, bool big) {
  uint8_t n = strlen_P(text);
  return n ? n * (big ? 6 : 4) - 1 : 0;
}

// Draws a text from flash, letter by letter; letters outside the screen are skipped.
void drawText(int16_t x, int16_t y, const char* text, bool big) {
  uint8_t step = big ? 6 : 4;
  for (const char* p = text; char ch = pgm_read_byte(p); p++, x += step)
    if (x > -step && x < SCREEN_W) drawChar(x, y, ch, big);
}

// Draws a text centred horizontally at row y.
void drawTextCentered(int16_t y, const char* text, bool big) {
  drawText((SCREEN_W - textWidth(text, big)) / 2, y, text, big);
}

// ================================================================= pages
// Messages are shown as still "pages" of short words (up to 4 small letters per line),
// one after another, never as scrolling text: Wokwi averages each LED's brightness, so
// moving letters smear into a white block that nobody can read.
const uint16_t PAGE_MS = 900;            // how long one page stays

// Which page of a sequence of `count` pages to show t ms after it started (the last one
// stays).
uint8_t pageAt(uint32_t t, uint8_t count) {
  uint32_t page = t / PAGE_MS;
  return page < count ? page : count - 1;
}

// Switches off h rows of the game screen from row y (a clean background for a page).
void clearRows(int16_t y, uint8_t h) {
  fillRect(0, y, SCREEN_W, h, false);
}

// Draws a page of one or two short lines (from flash) in the white zone, rows 6-17,
// which is cleared first. line2 may be NULL.
void drawPage(const char* line1, const char* line2) {
  clearRows(6, 12);
  if (line2) {
    drawTextCentered(7, line1, false);
    drawTextCentered(13, line2, false);
  } else {
    drawTextCentered(10, line1, false);
  }
}

// ================================================================= numbers
// Counts the digits of a number (for its width).
uint8_t numberDigits(uint16_t n) {
  uint8_t d = 1;
  while (n >= 10) {
    n /= 10;
    d++;
  }
  return d;
}

// Width in pixels of a number in the small or big font.
int16_t numberWidth(uint16_t n, bool big) {
  return numberDigits(n) * (big ? 6 : 4) - 1;
}

// Draws a number digit by digit, starting from the right (no leading zeros).
void drawNumber(int16_t x, int16_t y, uint16_t n, bool big) {
  uint8_t step = big ? 6 : 4;
  for (int8_t i = numberDigits(n) - 1; i >= 0; i--) {
    drawChar(x + i * step, y, '0' + n % 10, big);
    n /= 10;
  }
}

// Draws a number centred horizontally at row y.
void drawNumberCentered(int16_t y, uint16_t n, bool big) {
  drawNumber((SCREEN_W - numberWidth(n, big)) / 2, y, n, big);
}

// Bitmap drawn `scale` times bigger (ship preview in the hangar).
void drawBitmapScaled(int16_t x, int16_t y, const uint16_t* rows, uint8_t w, uint8_t h, uint8_t scale) {
  for (uint8_t r = 0; r < h; r++) {
    uint16_t bits = pgm_read_word(rows + r);
    for (uint8_t c = 0; c < w; c++)
      if (bits & (0x8000u >> c)) fillRect(x + c * scale, y + r * scale, scale, scale, true);
  }
}

// ================================================================= status bars
// Three bars of 16 LEDs above the game screen (matrix rows 24, 25, 26).
// Sets all 16 LEDs of one bar at once (bit x = LED x).
void setStatusBits(uint8_t bar, uint16_t bits) {
  frame[SCREEN_H + bar] = bits;
}

// Fills a status bar from the left with `level` LEDs (0-16), like a gauge.
void setStatusBar(uint8_t bar, uint8_t level) {
  setStatusBits(bar, level >= 16 ? 0xFFFF : (uint16_t)((1u << level) - 1));
}

// ================================================================= test helper
// Prints the picture as text over Serial ('#' = on), status bars first. Used by the
// automatic tests (command !d).
void displayDump() {
  Serial.println(F("[DUMP] begin"));
  for (uint8_t i = 0; i < TOTAL_ROWS; i++) {
    uint8_t r = (i < STATUS_ROWS) ? SCREEN_H + i : i - STATUS_ROWS;   // top to bottom
    if (i == STATUS_ROWS) Serial.println(F("----------------"));
    for (uint8_t x = 0; x < SCREEN_W; x++) Serial.print(frame[r] & (1u << x) ? '#' : '.');
    Serial.println();
  }
  Serial.println(F("[DUMP] end"));
}
