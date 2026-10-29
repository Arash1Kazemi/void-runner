/*
 * input.h -- the 3 x 3 push-button keypad (the 9 keys of the game).
 *
 * Same method as the course: drive one row LOW and read the three columns
 * (INPUT_PULLUP, so a pressed key reads LOW). Rows that are not being read are set to
 * INPUT instead of HIGH: with two keys of one column pressed, a HIGH and a LOW output
 * would otherwise be shorted together through the buttons.
 */
#pragma once
#include "pins.h"

uint16_t keysHeld = 0;                   // debounced: bit (row * 4 + column) = key is down
uint16_t keysPressed = 0;                // keys that went down since the last picture

// Sets up the keypad pins: rows switched off (INPUT), columns with pull-up resistors.
void inputBegin() {
  for (uint8_t r = 0; r < KEY_ROWS; r++) pinMode(KEY_ROW_PINS[r], INPUT);
  for (uint8_t c = 0; c < KEY_COLS; c++) pinMode(KEY_COL_PINS[c], INPUT_PULLUP);
}

// The bit of a key (by its label, e.g. 'R') in keysHeld and keysPressed.
uint16_t keyBit(char key) {
  for (uint8_t i = 0; i < KEY_ROWS * KEY_COLS; i++)
    if (KEY_NAMES[i / KEY_COLS][i % KEY_COLS] == key) return 1u << i;
  return 0;
}

// True while the key is held down.
bool keyHeld(char key) { return keysHeld & keyBit(key); }
// True only in the picture in which the key went down (one press = one action).
bool keyPressed(char key) { return keysPressed & keyBit(key); }
// Forgets this picture's key presses (called at the end of every picture).
void inputEndFrame() { keysPressed = 0; }

// Reads the whole keypad once: drives one row LOW at a time and reads the three columns.
// Returns one bit per key that is down (no debouncing here).
uint16_t readKeys() {
  uint16_t mask = 0;
  for (uint8_t r = 0; r < KEY_ROWS; r++) {
    pinMode(KEY_ROW_PINS[r], OUTPUT);
    digitalWrite(KEY_ROW_PINS[r], LOW);
    delayMicroseconds(5);                // let the line settle
    for (uint8_t c = 0; c < KEY_COLS; c++)
      if (digitalRead(KEY_COL_PINS[c]) == LOW) mask |= 1u << (r * KEY_COLS + c);
    pinMode(KEY_ROW_PINS[r], INPUT);
  }
  return mask;
}

// Reads the keypad every 5 ms. A change only counts once the keys read the same
// four times in a row: that hides the contact bounce that Wokwi simulates.
void inputUpdate(uint32_t now) {
  static uint32_t lastScan = 0;
  static uint16_t candidate = 0;
  static uint8_t sameScans = 0;
  if (now - lastScan < 5) return;
  lastScan = now;
  uint16_t raw = readKeys();
  if (raw != candidate) {
    candidate = raw;
    sameScans = 0;
    return;
  }
  if (sameScans < 3) sameScans++;
  if (sameScans < 3 || candidate == keysHeld) return;
  uint16_t changed = candidate ^ keysHeld;
  keysPressed |= changed & candidate;
  keysHeld = candidate;
  for (uint8_t i = 0; i < KEY_ROWS * KEY_COLS; i++) {
    if (!(changed & (1u << i))) continue;
    Serial.print((keysHeld & (1u << i)) ? F("[KEY] DOWN ") : F("[KEY] UP "));
    Serial.println(KEY_NAMES[i / KEY_COLS][i % KEY_COLS]);
  }
}
