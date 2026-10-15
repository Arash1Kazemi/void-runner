/*
 * menu.h -- main menu: PLAY, SHIP, OPTS, BEST (W/S to choose, R to open, Q back).
 * The chosen item is underlined.
 */
#pragma once
#include "display.h"
#include "input.h"
#include "sound.h"

const char MENU_PLAY[] PROGMEM = "PLAY";
const char MENU_SHIP[] PROGMEM = "SHIP";
const char MENU_OPTS[] PROGMEM = "OPTS";
const char MENU_BEST[] PROGMEM = "BEST";
const char* const MENU_ITEMS[] PROGMEM = {MENU_PLAY, MENU_SHIP, MENU_OPTS, MENU_BEST};
enum MenuItem : int8_t { MENU_ITEM_PLAY, MENU_ITEM_SHIP, MENU_ITEM_OPTS, MENU_ITEM_BEST };
const uint8_t MENU_COUNT = 4;
uint8_t menuIndex = 0;

// Text of menu item i (the list of texts is stored in flash).
const char* menuItem(uint8_t i) {
  return (const char*)pgm_read_ptr(&MENU_ITEMS[i]);
}

const int8_t MENU_NONE = -1, MENU_BACK = -2;

// Handles the keys; returns the item to open, MENU_BACK or MENU_NONE.
int8_t menuUpdate() {
  if (keyPressed(KEY_UP)) {
    menuIndex = (menuIndex + MENU_COUNT - 1) % MENU_COUNT;
    soundBeep(1400, 25);
  }
  if (keyPressed(KEY_DOWN)) {
    menuIndex = (menuIndex + 1) % MENU_COUNT;
    soundBeep(1400, 25);
  }
  if (keyPressed(KEY_FIRE)) {
    soundBeep(2000, 60);
    return menuIndex;
  }
  if (keyPressed(KEY_BACK)) return MENU_BACK;
  return MENU_NONE;
}

// Draws the four menu items, underlines the chosen one and switches the status bars off.
void menuDraw() {
  clearScreen();
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    uint8_t y = i * 6;
    if (i == menuIndex) fillRect(0, y + 5, 15, 1, true); // underline in the gap below
    drawText(0, y, menuItem(i), false);
  }
  for (uint8_t bar = 0; bar < STATUS_ROWS; bar++) setStatusBits(bar, 0);
}
