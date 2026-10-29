/*
 * levels.h -- the three phases. Each phase is a list of spawns: wait (in 0.1 s after the
 * previous spawn), what, and x. SP_CLEAR waits until the screen is empty; SP_AMMO and
 * SP_LIFE drop a pickup. After the last entry (and an empty screen) the boss comes.
 * The enemies are slow, so they would pile up: the list also waits while the phase's
 * maximum number of enemies is on the screen (that keeps 16 x 24 LEDs readable).
 */
#pragma once
#include "world.h"
#include "tunes.h"

struct Spawn { uint8_t wait, what; int8_t x; };
const uint8_t SP_AMMO = 10, SP_LIFE = 11, SP_CLEAR = 12;
#define DR EN_DRONE
#define WV EN_WEAVER
#define MN EN_MINER
#define CR EN_CRUISER
#define RS EN_ROCK_S
#define RB EN_ROCK_B

// ---- phase 1: Rock Belt (asteroids) -- drones and lots of rocks
const Spawn PHASE1[] PROGMEM = {
  {15, RS, 3}, {12, DR, 2}, {8, DR, 11}, {12, RB, 6}, {12, DR, 6},
  {0, DR, 10}, {12, RS, 12}, {10, SP_CLEAR, 0}, {5, SP_AMMO, 7},
  {10, DR, 1}, {5, DR, 6}, {5, DR, 11}, {14, RB, 3}, {12, DR, 4},
  {0, DR, 9}, {12, RS, 7}, {10, DR, 2}, {4, DR, 7}, {4, DR, 12},
  {12, RB, 0}, {10, SP_CLEAR, 0}, {5, SP_AMMO, 3},
  {8, DR, 0}, {3, DR, 4}, {3, DR, 8}, {3, DR, 12}, {14, RB, 6},
  {12, RS, 12}, {10, SP_CLEAR, 0},
};

// ---- phase 2: Deep Mist (a nebula) -- weavers and mine-layers
const Spawn PHASE2[] PROGMEM = {
  {15, WV, 3}, {10, WV, 10}, {14, MN, 0}, {12, DR, 6}, {10, WV, 1},
  {6, WV, 12}, {14, DR, 7}, {10, MN, 15}, {12, DR, 2}, {0, DR, 11},
  {12, SP_CLEAR, 0}, {5, SP_AMMO, 6},
  {10, WV, 5}, {4, WV, 9}, {12, MN, 0}, {8, MN, 15}, {14, RB, 6},
  {10, DR, 1}, {4, DR, 7}, {4, DR, 12}, {12, WV, 2}, {6, WV, 11},
  {10, RS, 4}, {12, SP_CLEAR, 0}, {5, SP_LIFE, 9},
  {10, MN, 0}, {6, WV, 6}, {6, WV, 9}, {10, MN, 15}, {10, DR, 3},
  {4, DR, 10}, {12, SP_CLEAR, 0}, {5, SP_AMMO, 4},
};

// ---- phase 3: The Void -- cruisers, everything else, and rock showers
const Spawn PHASE3[] PROGMEM = {
  {15, CR, 5}, {14, DR, 1}, {4, DR, 12}, {12, RS, 3}, {8, RS, 12},
  {14, WV, 6}, {12, SP_CLEAR, 0}, {5, SP_AMMO, 5},
  {10, CR, 1}, {16, CR, 10}, {10, RB, 6}, {8, RS, 14},
  {12, WV, 2}, {4, WV, 11}, {12, MN, 0}, {12, SP_CLEAR, 0}, {5, SP_AMMO, 9},
  {8, RS, 2}, {6, RS, 10}, {8, RB, 5},
  {12, DR, 0}, {3, DR, 5}, {3, DR, 10}, {10, CR, 5}, {12, WV, 1},
  {4, WV, 12}, {12, SP_CLEAR, 0}, {5, SP_LIFE, 7}, {5, SP_AMMO, 3},
};

#undef DR
#undef WV
#undef MN
#undef CR
#undef RS
#undef RB

struct PhaseInfo {
  const Spawn* spawns;
  uint8_t count;
  uint8_t maxEnemies;                    // at most this many on the screen at once
  const char* name1;                     // the phase name: two words of up to 4
  const char* name2;                     // letters (in flash), shown as a page
  const Track* song;
  uint8_t songTickMs;
  uint8_t boss;
};
const char NAME_ROCK[] PROGMEM = "ROCK";
const char NAME_BELT[] PROGMEM = "BELT";
const char NAME_DEEP[] PROGMEM = "DEEP";
const char NAME_MIST[] PROGMEM = "MIST";
const char NAME_THE[] PROGMEM = "THE";
const char NAME_VOID[] PROGMEM = "VOID";
const PhaseInfo PHASES[3] = {
  {PHASE1, sizeof(PHASE1) / sizeof(Spawn), 4, NAME_ROCK, NAME_BELT, SONG_PHASE1, SONG_PHASE1_TICK_MS, BOSS_KIND_WARDEN},
  {PHASE2, sizeof(PHASE2) / sizeof(Spawn), 5, NAME_DEEP, NAME_MIST, SONG_PHASE2, SONG_PHASE2_TICK_MS, BOSS_KIND_HUNTER},
  {PHASE3, sizeof(PHASE3) / sizeof(Spawn), 6, NAME_THE, NAME_VOID, SONG_PHASE3, SONG_PHASE3_TICK_MS, BOSS_KIND_KING},
};

// Copies entry i of a phase's spawn list from flash into RAM.
Spawn spawnAt(uint8_t phase, uint8_t i) {
  Spawn s;
  memcpy_P(&s, &PHASES[phase].spawns[i], sizeof(Spawn));
  return s;
}
