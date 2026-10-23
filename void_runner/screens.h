/*
 * screens.h -- the screens around the game:
 *   HANGAR    choose a ship (A/D), R = take it, Q = back.
 *             Status bars: red = speed, yellow = ammo, green = fire power.
 *   OPTIONS   W/S = item, A/D or R = change: LITE (brightness), SND (sound),
 *             DIFF (difficulty), TEST (LED test). Saved in the EEPROM.
 *   BEST      the high score and how far it got.
 *   PAUSE     P or R = continue, Q = quit to the menu.
 *   GAME OVER / VICTORY   the score; R or Q = menu.
 *   LED TEST  every LED on, then row, column and checker patterns (hardware check).
 * Each xxxFrame() handles the keys, draws one picture and returns the screen to show
 * next (its own screen to stay).
 */
#pragma once
#include "display.h"
#include "input.h"
#include "sound.h"
#include "storage.h"
#include "world.h"
#include "game.h"

// Sets the LED brightness from the saved setting (100 / 50 / 20 %).
void applyBrightness() {
  displaySetBrightness(BRIGHTNESS_LEVELS[settings.brightness]);
}

// Small arrows at the left and right edge: "A/D changes this".
void drawSideArrows(int8_t y, uint16_t frameNo) {
  if (frameNo & 16) return;
  setPixel(1, y, true);
  setPixel(0, y + 1, true);
  setPixel(1, y + 2, true);
  setPixel(14, y, true);
  setPixel(15, y + 1, true);
  setPixel(14, y + 2, true);
}

// One dot per page at the bottom; the current page is a bigger block.
void drawPageDots(uint8_t count, uint8_t current) {
  int8_t x = (SCREEN_W - (count * 4 - 2)) / 2;
  for (uint8_t i = 0; i < count; i++, x += 4) {
    if (i == current) fillRect(x, 20, 2, 2, true);
    else setPixel(x, 21, true);
  }
}

// Switches off all three status bars.
void clearStatusBars() {
  for (uint8_t bar = 0; bar < STATUS_ROWS; bar++) setStatusBits(bar, 0);
}

// ================================================================= hangar
uint8_t hangarShip = 0;
uint32_t hangarSwitched = 0;             // millis() when the ship on show changed

// Opens the hangar on the ship that is in use now.
void hangarBegin() {
  hangarShip = settings.ship;
  hangarSwitched = millis();
}

// One picture of the hangar: A/D (or W/S) shows another ship, R takes it (saved), Q goes
// back. Draws name, big ship, arrows and page dots; the stats go on the status bars.
Screen hangarFrame(uint32_t now, uint16_t frameNo) {
  int8_t step = (keyPressed(KEY_RIGHT) || keyPressed(KEY_DOWN)) -
                (keyPressed(KEY_LEFT) || keyPressed(KEY_UP));
  if (step) {
    hangarShip = (hangarShip + 3 + step) % 3;
    hangarSwitched = now;
    soundBeep(1400, 25);
  }
  if (keyPressed(KEY_FIRE)) {
    settings.ship = hangarShip;
    settingsSave();
    soundSfx(SFX(SFX_PICKUP), 4);
    Serial.print(F("[VR] ship "));
    Serial.println((const __FlashStringHelper*)SHIPS[hangarShip].name);
    return SCR_MENU;
  }
  if (keyPressed(KEY_BACK)) return SCR_MENU;

  const ShipInfo& ship = SHIPS[hangarShip];
  clearScreen();
  drawTextCentered(0, ship.name, false);
  drawBitmapScaled(3, 6, ship.sprite, 3, 3, 3);   // the ship, 3 times bigger
  if (frameNo & 1) fillRect(6, 15, 3, 2, true);   // engine flame
  drawSideArrows(9, frameNo);
  drawPageDots(3, hangarShip);
  if (hangarShip == settings.ship) setPixel(SCREEN_W - 1, 23, true);   // the ship in use
  // stats on the status bars, growing in after each switch: 5 LEDs per level
  uint32_t grow = now - hangarSwitched;
  uint8_t levels[3] = {ship.speedLevel, ship.ammoLevel, ship.fireLevel};
  for (uint8_t bar = 0; bar < STATUS_ROWS; bar++) {
    uint8_t full = levels[bar] * 5;
    setStatusBar(bar, grow >= 300 ? full : full * grow / 300);
  }
  return SCR_HANGAR;
}

// ================================================================= options
enum OptionItem : uint8_t { OPT_LITE, OPT_SOUND, OPT_DIFF, OPT_TEST, OPTION_COUNT };
const char OPT_NAME_LITE[] PROGMEM = "LITE";
const char OPT_NAME_SND[] PROGMEM = "SND";
const char OPT_NAME_DIFF[] PROGMEM = "DIFF";
const char OPT_NAME_TEST[] PROGMEM = "TEST";
const char* const OPTION_NAMES[OPTION_COUNT] = {OPT_NAME_LITE, OPT_NAME_SND, OPT_NAME_DIFF, OPT_NAME_TEST};
const char DIFF_EASY_NAME[] PROGMEM = "EASY";
const char DIFF_NORMAL_NAME[] PROGMEM = "NORM";
const char DIFF_HARD_NAME[] PROGMEM = "HARD";
const char* const DIFF_NAMES[3] = {DIFF_EASY_NAME, DIFF_NORMAL_NAME, DIFF_HARD_NAME};
uint8_t optionItem = 0;

// One picture of the options: W/S picks LITE, SND, DIFF or TEST; A/D or R changes it
// (saved in the EEPROM); R on TEST starts the LED test; Q goes back.
Screen optionsFrame(uint16_t frameNo) {
  if (keyPressed(KEY_UP) || keyPressed(KEY_DOWN)) {
    optionItem = (optionItem + (keyPressed(KEY_UP) ? OPTION_COUNT - 1 : 1)) % OPTION_COUNT;
    soundBeep(1400, 25);
  }
  int8_t change = keyPressed(KEY_RIGHT) - keyPressed(KEY_LEFT);
  if (keyPressed(KEY_FIRE)) {
    if (optionItem == OPT_TEST) return SCR_LEDTEST;
    change = 1;
  }
  if (change && optionItem != OPT_TEST) {
    if (optionItem == OPT_LITE) {
      settings.brightness = (settings.brightness + 3 + change) % 3;
      applyBrightness();
    } else if (optionItem == OPT_SOUND) {
      settings.muted = !settings.muted;
      soundSetMuted(settings.muted);
    } else {
      settings.difficulty = (settings.difficulty + 3 + change) % 3;
    }
    settingsSave();
    soundBeep(2000, 40);
    Serial.print(F("[VR] option "));
    Serial.print((const __FlashStringHelper*)OPTION_NAMES[optionItem]);
    Serial.print(' ');
    Serial.println(optionItem == OPT_LITE ? BRIGHTNESS_LEVELS[settings.brightness]
                   : optionItem == OPT_SOUND ? !settings.muted : settings.difficulty);
  }
  if (keyPressed(KEY_BACK)) return SCR_MENU;

  clearScreen();
  clearStatusBars();
  drawTextCentered(0, OPTION_NAMES[optionItem], false);
  switch (optionItem) {
    case OPT_LITE:
      drawNumberCentered(9, BRIGHTNESS_LEVELS[settings.brightness], false);
      setStatusBar(1, 16 * BRIGHTNESS_LEVELS[settings.brightness] / 100);   // a brightness gauge
      break;
    case OPT_SOUND:
      drawTextCentered(9, settings.muted ? PSTR("OFF") : PSTR("ON"), false);
      break;
    case OPT_DIFF:
      drawTextCentered(9, DIFF_NAMES[settings.difficulty], false);
      setStatusBar(0, (settings.difficulty + 1) * 5);
      break;
    case OPT_TEST:
      if (frameNo & 16) drawTextCentered(9, PSTR("R"), false);
      break;
  }
  if (optionItem != OPT_TEST) drawSideArrows(15, frameNo);   // under the value
  drawPageDots(OPTION_COUNT, optionItem);
  return SCR_OPTIONS;
}

// ================================================================= best score
// One picture of the BEST page: the high score and how far it got (ZONE 1-3, or WIN).
Screen bestFrame(uint16_t frameNo) {
  if (keyPressed(KEY_BACK) || keyPressed(KEY_FIRE)) return SCR_MENU;
  clearScreen();
  clearStatusBars();
  drawTextCentered(0, PSTR("BEST"), false);
  if (settings.best) drawNumberCentered(9, settings.best, false);
  else drawTextCentered(9, PSTR("---"), false);
  if (settings.bestPhase >= 4) {
    drawTextCentered(18, PSTR("WIN"), false);
  } else if (settings.bestPhase) {       // the zone it reached: "ZONE", then "2"
    if ((frameNo / 60) % 2) drawNumberCentered(18, settings.bestPhase, false);
    else drawTextCentered(18, PSTR("ZONE"), false);
  }
  if (settings.best && (frameNo & 8)) {  // a little sparkle
    setPixel(1, 7, true);
    setPixel(14, 15, true);
  } else if (settings.best) {
    setPixel(14, 7, true);
    setPixel(1, 15, true);
  }
  return SCR_BEST;
}

// ================================================================= pause
// Draws the pause screen: the score, a big pause sign, and the keys as a page that
// changes every 1.2 s ("P:GO" = P continues, "Q:EX" = Q exits), with the game's bars.
void pauseDraw(uint32_t t, uint16_t frameNo) {
  clearScreen();
  drawNumberCentered(0, player.score, false);
  fillRect(4, 7, 3, 9, true);            // the pause sign: two bars
  fillRect(9, 7, 3, 9, true);
  drawTextCentered(18, (t / 1200) % 2 ? PSTR("Q:EX") : PSTR("P:GO"), false);
  drawGameStatus(frameNo);
}

// ================================================================= game over and victory
bool newBest = false;

// Saves the score as the new best (with the phase reached; 4 = won) if it beats the old
// one.
void recordScore(uint8_t phaseReached) {
  newBest = player.score > settings.best;
  if (!newBest) return;
  settings.best = player.score;
  settings.bestPhase = phaseReached;
  settingsSave();
  Serial.println(F("[VR] new best"));
}

// One picture of GAME OVER: the score (blinking if it is a record) and the best score;
// after 1 s, R or Q goes to the menu.
Screen gameOverFrame(uint32_t t, uint16_t frameNo) {
  if (t > 1000 && (keyPressed(KEY_FIRE) || keyPressed(KEY_BACK))) return SCR_MENU;
  clearScreen();
  clearStatusBars();
  drawTextCentered(0, PSTR("GAME"), false);
  drawTextCentered(6, PSTR("OVER"), false);
  if (!newBest || (frameNo & 8)) drawNumberCentered(12, player.score, false);   // blinks: record
  if ((t / 1500) % 2 == 0) drawTextCentered(18, newBest ? PSTR("NEW") : PSTR("BEST"), false);
  else drawNumberCentered(18, settings.best, false);
  return SCR_GAMEOVER;
}

uint32_t nextFirework = 0;

// One picture of the victory screen: fireworks, pages "YOU WIN!", "VOID SAFE" and the
// score, and racing status bars; after 2 s, R or Q goes to the menu.
Screen victoryFrame(uint32_t t) {
  if (t > 2000 && (keyPressed(KEY_FIRE) || keyPressed(KEY_BACK))) return SCR_MENU;
  if (t >= nextFirework) {               // fireworks in the red and green zones
    bool top = random(2);
    explode(2 + random(SCREEN_W - 4), top ? 1 + random(4) : 18 + random(4), 10, 12);
    if (t > 2600) soundSfx(SFX(SFX_BOOM), 3);   // after the fanfare
    nextFirework = t + 350 + random(400);
  }
  particlesStep();
  clearScreen();
  for (uint8_t i = 0; i < MAX_PARTICLES; i++)
    if (particles[i].life) setPixel(particles[i].x16 >> 4, particles[i].y16 >> 4, true);
  uint8_t page = (t / 1200) % 3;
  if (page == 0) {
    drawPage(PSTR("YOU"), PSTR("WIN!"));
  } else if (page == 1) {
    drawPage(PSTR("VOID"), PSTR("SAFE"));
  } else {
    clearRows(6, 12);
    drawNumberCentered(7, player.score, false);
    drawTextCentered(13, PSTR("PTS"), false);
  }
  for (uint8_t bar = 0; bar < STATUS_ROWS; bar++) {   // status bars: a fast chase
    uint8_t head = (t / 50 + bar * 4) % 16;
    uint16_t bits = 0;
    for (uint8_t k = 0; k < 3; k++) bits |= 1u << ((head + k) % 16);
    setStatusBits(bar, bits);
  }
  return SCR_VICTORY;
}

// ================================================================= LED test
// Lights every LED, then one row at a time (status bars first), one column at a time,
// and a checkerboard -- a quick way to see that every LED and wire works.
const uint16_t TEST_ALL_MS = 1500, TEST_STEP_MS = 90;

// Sets one matrix row, counted from the top as built (status bars first, then the game
// screen).
void ledTestSetRow(uint8_t fromTop, uint16_t bits) {
  frame[fromTop < STATUS_ROWS ? SCREEN_H + fromTop : fromTop - STATUS_ROWS] = bits;
}

// One picture of the LED test: all on, then each row, each column and a checkerboard,
// with a rising beep per step. Any key (or the end) goes back to the options.
Screen ledTestFrame(uint32_t t) {
  static uint8_t lastStep = 255;
  uint16_t skip = keysPressed & ~(keyBit(KEY_BRIGHTNESS) | keyBit(KEY_MUTE));
  const uint32_t rowsEnd = TEST_ALL_MS + (uint32_t)TOTAL_ROWS * TEST_STEP_MS;
  const uint32_t colsEnd = rowsEnd + (uint32_t)SCREEN_W * TEST_STEP_MS;
  const uint32_t checkEnd = colsEnd + 2000;
  if (skip || t >= checkEnd) {
    lastStep = 255;
    soundStop();
    return SCR_OPTIONS;
  }
  uint8_t step = 0;
  for (uint8_t r = 0; r < TOTAL_ROWS; r++) {
    uint16_t bits = 0xFFFF;                        // all on
    if (t >= colsEnd) {                            // checkerboard, swapping every 250 ms
      bits = ((r + t / 250) & 1) ? 0xAAAA : 0x5555;
    } else if (t >= rowsEnd) {                     // column sweep
      step = (t - rowsEnd) / TEST_STEP_MS;
      bits = 1u << step;
    } else if (t >= TEST_ALL_MS) {                 // row sweep
      step = (t - TEST_ALL_MS) / TEST_STEP_MS;
      bits = (r == step) ? 0xFFFF : 0;
    }
    ledTestSetRow(r, bits);
  }
  if (t >= TEST_ALL_MS && t < colsEnd && step != lastStep) {   // a rising tick per step
    lastStep = step;
    soundBeep(t < rowsEnd ? 300 + step * 40 : 600 + step * 60, 30);
  }
  return SCR_LEDTEST;
}
