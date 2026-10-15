/*
 * logo.h -- the start-up logo, played in sync with the welcome melody.
 *
 * The picture is driven by the song's own clock (songTime()), so every movement lands
 * on its note. Timeline (ms):
 *     0 - 1200  warp: star streaks rush past and speed up      rising "whoosh" + low drone
 *  1200 - 2000  the ship rises from the bottom and brakes       3 falling braking notes
 *  2000 - 2400  the ship shoots up and out of the screen        quick rising glide
 *  2400 - 3600  V, O, I, D light up one by one, flickering on;  one note per letter
 *               enemy drones fly past in the side margins       + crackle while it flickers
 *  3600 - 4800  "RUNNER" crosses on a dark ribbon, towed by     melody, harmony and bass
 *               the ship
 *  4800 - 5600  the whole screen flashes, then the sign glows   final A-minor chord
 * The status bars fill up during the logo like a start-up progress bar.
 */
#pragma once
#include "display.h"
#include "sound.h"
#include "art.h"

// ---- welcome melody: one track per buzzer, 1 tick = 50 ms (MIDI note numbers)
const Note BOOT_MELODY[] PROGMEM = {
  {0, 24},                                          // warp (whoosh is an effect)
  {0, 16},                                          // ship arrives
  {0, 8},                                           // ship leaves (glide is an effect)
  {69, 6}, {72, 6}, {76, 6}, {81, 6},               // V O I D: A4 C5 E5 A5
  {76, 3}, {74, 3}, {72, 3}, {74, 3}, {76, 3}, {76, 3}, {76, 6},   // RUNNER tune
  {76, 8}, {0, 8},                                  // final chord, top note E5
};
const Note BOOT_HARMONY[] PROGMEM = {
  {0, 24},
  {64, 4}, {60, 4}, {57, 6}, {0, 2},                // braking: E4 C4 A3
  {0, 32},
  {72, 6}, {71, 6}, {69, 6}, {71, 6},               // C5 B4 A4 B4
  {72, 8}, {0, 8},                                  // final chord, middle note C5
};
const Note BOOT_BASS[] PROGMEM = {
  {45, 24},                                         // A2 drone under the warp
  {0, 24},
  {0, 24},                                          // (crackle effects)
  {45, 3}, {45, 3}, {52, 3}, {45, 3}, {45, 3}, {45, 3}, {43, 3}, {45, 3},  // bass line
  {57, 8}, {0, 8},                                  // final chord, low note A3
};
const Track BOOT_SONG[BUZZER_COUNT] = {
  {BOOT_MELODY, sizeof(BOOT_MELODY) / sizeof(Note)},
  {BOOT_HARMONY, sizeof(BOOT_HARMONY) / sizeof(Note)},
  {BOOT_BASS, sizeof(BOOT_BASS) / sizeof(Note)},
};
const uint8_t BOOT_TICK_MS = 50;
const uint16_t LOGO_END_MS = 5600;
const uint16_t LETTER_START[4] = {2400, 2700, 3000, 3300};
const uint8_t LETTER_FLICKER_MS = 120;

// ---- warp stars: y is stored in 1/16 pixel so they can move smoothly
const uint8_t STAR_COUNT = 12;
int8_t starX[STAR_COUNT];
int16_t starY16[STAR_COUNT];

// Draws letter i of the VOID sign (V, O, I, D from top to bottom).
void drawLogoLetter(uint8_t i) {
  drawBitmap(LOGO_X, LOGO_Y[i], LOGO_LETTERS[i], LOGO_W, LOGO_H, true);
}

// Starts the logo: scatters the warp stars and starts the welcome melody.
void logoBegin() {
  for (uint8_t i = 0; i < STAR_COUNT; i++) {
    starX[i] = random(SCREEN_W);
    starY16[i] = random(SCREEN_H * 16);
  }
  soundPlaySong(BOOT_SONG, BOOT_TICK_MS, false);
}

// Star speed in 1/16 pixel per picture: speeds up, brakes, then drifts.
uint8_t starSpeed(uint32_t t) {
  if (t < 1200) return 4 + 28 * t / 1200;
  if (t < 2000) return 32 - 29 * (t - 1200) / 800;
  return 3;
}

// Moves and draws the warp stars; the faster they fly, the longer their streaks.
void drawStars(uint32_t t) {
  uint8_t speed = starSpeed(t);
  uint8_t len = 1 + speed / 10;                    // faster = longer streak
  for (uint8_t i = 0; i < STAR_COUNT; i++) {
    starY16[i] += speed;
    if (starY16[i] / 16 - len > SCREEN_H) {        // fell off the bottom: start again on top
      starY16[i] = -16 * (int16_t)random(4);
      starX[i] = random(SCREEN_W);
    }
    for (uint8_t k = 0; k < len; k++) setPixel(starX[i], starY16[i] / 16 - k, true);
  }
}

// Draws the ship flying up, with a flickering engine flame below it.
void drawShipUp(int16_t x, int16_t y, uint16_t frameNo) {
  drawBitmap(x, y, SHIP_UP, 3, 3, true);
  if (frameNo & 1) setPixel(x + 1, y + 3, true);   // flickering engine flame
}

// Sound effects that follow the picture: whoosh, glide and letter crackle.
void logoEffects(uint32_t t) {
  if (t < 1200) soundEffect(0, 150 + 1350 * t / 1200);
  else if (t >= 2000 && t < 2300) soundEffect(0, 400 + 1200 * (t - 2000) / 300);
  else soundRelease(0);
  bool crackle = false;
  for (uint8_t i = 0; i < 4; i++)
    if (t >= LETTER_START[i] && t < LETTER_START[i] + LETTER_FLICKER_MS) crackle = true;
  if (crackle) soundEffect(2, random(1500, 3500));
  else soundRelease(2);
}

// Draws one picture of the logo. Returns true when the logo is over.
bool logoUpdate(uint16_t frameNo) {
  uint32_t t = songTime();
  if (t >= LOGO_END_MS) {
    soundRelease(0);
    soundRelease(2);
    return true;
  }
  logoEffects(t);
  clearScreen();
  if (t < 2400) drawStars(t);
  if (t >= 1200 && t < 2000) {                     // ship rises and brakes
    uint32_t p = (t - 1200) * 100 / 800, rest = 100 - p;
    drawShipUp(6, 19 + 5 * rest * rest / 10000, frameNo);
  } else if (t >= 2000 && t < 2400) {              // ship shoots up and out
    uint32_t q = (t - 2000) * 100 / 400;
    int16_t y = 19 - (int16_t)(23 * q * q / 10000);
    drawShipUp(6, y, frameNo);
    setPixel(7, y + 4, true);                      // longer flame
  }
  if (t >= LETTER_START[0] && t < 4800) {
    for (uint8_t i = 0; i < 4; i++) {              // letters light up, flickering at first
      if (t < LETTER_START[i]) continue;
      uint32_t age = t - LETTER_START[i];
      if (age >= LETTER_FLICKER_MS || (age / 30) % 2 == 0) drawLogoLetter(i);
    }
    if (t < 3600) {                                // drones fly past in the side margins
      drawBitmap(0, -2 + (int16_t)((t - 2400) * 26 / 1200), ENEMY_DRONE, 3, 2, true);
      if (t >= 2800) drawBitmap(13, -2 + (int16_t)((t - 2800) * 26 / 1200), ENEMY_DRONE, 3, 2, true);
    }
  }
  if (t >= 3600 && t < 4800) {                     // "RUNNER" on a dark ribbon, towed by the ship
    fillRect(0, 8, SCREEN_W, 9, false);
    for (uint8_t x = 0; x < SCREEN_W; x++)
      if ((x + t / 60) % 3) {                      // ribbon edges move like a conveyor
        setPixel(x, 8, true);
        setPixel(x, 16, true);
      }
    int16_t x = 16 - (int16_t)((t - 3600) * 51 / 1200);
    drawText(x, 9, PSTR("RUNNER"), true);
    drawBitmap(x - 4, 11, SHIP_LEFT, 3, 3, true);
  }
  if (t >= 4800) {                                 // final hit: flash, then the sign glows
    if (t < 4900) fillRect(0, 0, SCREEN_W, SCREEN_H, true);
    else for (uint8_t i = 0; i < 4; i++) drawLogoLetter(i);
  }
  uint8_t level = t >= 4800 ? 16 : t * 16 / 4800;  // status bars: start-up progress
  for (uint8_t b = 0; b < STATUS_ROWS; b++) setStatusBar(b, level);
  return false;
}
