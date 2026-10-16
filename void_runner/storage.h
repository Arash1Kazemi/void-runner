/*
 * storage.h -- settings and the high score, kept in the Mega's EEPROM.
 *
 * Written with the EEPROM registers directly (no library):
 *   EEAR = address, EEDR = data byte,
 *   EECR: EERE starts a read; EEMPE then EEPE (within 4 clock cycles) starts a write.
 * A write takes about 3.4 ms, so the next access waits while EEPE is still set.
 * (Wokwi keeps the EEPROM while a simulation runs; a restart starts empty.)
 */
#pragma once
#include "config.h"

struct Settings {
  uint8_t brightness;                    // index into BRIGHTNESS_LEVELS
  uint8_t muted;
  uint8_t difficulty;
  uint8_t ship;                          // chosen in the hangar
  uint16_t best;                         // high score
  uint8_t bestPhase;                     // phase reached with the high score (4 = won)
};
Settings settings;
const uint8_t EE_MAGIC = 0xA7;           // marks an EEPROM that holds our settings

// Reads one byte of the EEPROM (first waits if a write is still busy).
uint8_t eepromRead(uint16_t address) {
  while (EECR & _BV(EEPE)) {}            // wait for a write in progress
  EEAR = address;
  EECR |= _BV(EERE);
  return EEDR;
}

// Writes one byte to the EEPROM, only if it changed (saves time and EEPROM wear).
void eepromWrite(uint16_t address, uint8_t value) {
  if (eepromRead(address) == value) return;   // saves EEPROM wear (and time)
  EEAR = address;
  EEDR = value;
  noInterrupts();                        // EEPE must follow EEMPE within 4 cycles
  EECR |= _BV(EEMPE);
  EECR |= _BV(EEPE);
  interrupts();
}

// Saves the settings and the best score: a marker byte, then the Settings struct.
void settingsSave() {
  eepromWrite(0, EE_MAGIC);
  const uint8_t* p = (const uint8_t*)&settings;
  for (uint8_t i = 0; i < sizeof(Settings); i++) eepromWrite(1 + i, p[i]);
}

// Loads the settings at start-up. If the EEPROM holds none (new chip, new simulation) or
// a value is out of range, the defaults are used.
void settingsLoad() {
  settings = {0, 0, DIFF_NORMAL, 0, 0, 0};
  if (eepromRead(0) == EE_MAGIC) {
    uint8_t* p = (uint8_t*)&settings;
    for (uint8_t i = 0; i < sizeof(Settings); i++) p[i] = eepromRead(1 + i);
  }
  if (settings.brightness >= sizeof(BRIGHTNESS_LEVELS)) settings.brightness = 0;
  if (settings.difficulty > DIFF_HARD) settings.difficulty = DIFF_NORMAL;
  if (settings.ship > 2) settings.ship = 0;
  settings.muted = settings.muted ? 1 : 0;
}
