/*
 * config.h -- settings of VOID RUNNER in one place (timing, keys, game balance).
 */
#pragma once
#include <Arduino.h>

// ---- timing
const uint8_t FRAME_MS   = 20;           // a new picture is drawn every 20 ms (50 per second)
const uint8_t REFRESH_HZ = 71;           // the LED matrix is scanned 71 times per second

// ---- brightness levels (key B): % of its time slot that each LED row stays lit
const uint8_t BRIGHTNESS_LEVELS[] = {100, 50, 20};

// ---- keys (labels of the keypad buttons = PC keyboard keys)
const char KEY_UP = 'W', KEY_LEFT = 'A', KEY_DOWN = 'S', KEY_RIGHT = 'D';
const char KEY_FIRE = 'R';               // fire, and "OK" in menus
const char KEY_BACK = 'Q';
const char KEY_PAUSE = 'P';
const char KEY_MUTE = 'M';
const char KEY_BRIGHTNESS = 'B';

// ---- the screens of the game
enum Screen : uint8_t {
  SCR_LOGO, SCR_TITLE, SCR_MENU, SCR_HANGAR, SCR_OPTIONS, SCR_BEST,
  SCR_GAME, SCR_PAUSE, SCR_GAMEOVER, SCR_VICTORY, SCR_LEDTEST
};

// ---- difficulty (OPTS): lives at the start, enemy fire speed, ammo per pack
enum Difficulty : uint8_t { DIFF_EASY, DIFF_NORMAL, DIFF_HARD };
const uint8_t DIFF_LIVES[3]      = {4, 3, 3};
const uint8_t DIFF_FIRE_SCALE[3] = {150, 100, 75};   // % of the enemy fire interval
const uint8_t DIFF_BOSS_HP[3]    = {80, 100, 130};   // % of the boss health
const uint8_t DIFF_PACK_AMMO[3]  = {20, 15, 12};

// ---- player
const uint8_t MAX_LIVES = 4;
const uint8_t LOW_AMMO = 5;              // warning beep + blinking ammo bar from here
const uint16_t MOVE_REPEAT_DELAY = 170;  // hold a move key this long before it glides
const uint16_t HIT_SHIELD_MS = 2000;     // blinking, can't be hit, after a hit
const uint8_t PLAYER_SHOT_MS = 35;       // ms per pixel of a player bullet

// ---- scoring
const uint16_t PHASE_BONUS = 100;        // x phase number, at the end of each phase
