/*
 * tunes.h -- music and sound effects (all in flash).
 *
 * Songs: one note list per buzzer, MIDI note numbers (60 = middle C, 0 = rest) and
 * lengths in ticks. In-game songs leave buzzer 3 empty, so sound effects never cut the
 * music. Effects: parts that slide between two frequencies (or noise for explosions).
 * The start-up melody lives in logo.h.
 */
#pragma once
#include "sound.h"

#define TRACK(notes) {notes, sizeof(notes) / sizeof(Note)}
#define NO_TRACK {NULL, 0}
#define SFX(parts) parts, sizeof(parts) / sizeof(SfxPart)

// ================================================================= phase 1: Asteroid Belt (A minor)
const Note P1_MELODY[] PROGMEM = {
  {76, 4}, {74, 2}, {72, 2}, {69, 8}, {72, 4}, {74, 2}, {76, 2}, {79, 4}, {76, 4},
};
const Note P1_BASS[] PROGMEM = {
  {45, 1}, {45, 1}, {57, 1}, {45, 1}, {45, 1}, {45, 1}, {43, 1}, {45, 1},
  {41, 1}, {41, 1}, {53, 1}, {41, 1}, {43, 1}, {43, 1}, {55, 1}, {43, 1},
  {45, 1}, {45, 1}, {57, 1}, {45, 1}, {48, 1}, {48, 1}, {60, 1}, {48, 1},
  {50, 1}, {50, 1}, {62, 1}, {50, 1}, {52, 1}, {52, 1}, {64, 1}, {52, 1},
};
const Track SONG_PHASE1[3] = {TRACK(P1_MELODY), TRACK(P1_BASS), NO_TRACK};

// ================================================================= phase 2: Nebula (D minor, slow)
const Note P2_MELODY[] PROGMEM = {
  {0, 4}, {77, 2}, {76, 2}, {74, 4}, {0, 4}, {69, 2}, {70, 2}, {69, 4}, {0, 4}, {74, 4},
};
const Note P2_BASS[] PROGMEM = {
  {50, 4}, {45, 4}, {46, 4}, {41, 4}, {43, 4}, {50, 4}, {45, 8},
};
const Track SONG_PHASE2[3] = {TRACK(P2_MELODY), TRACK(P2_BASS), NO_TRACK};

// ================================================================= phase 3: The Void (E minor, fast)
const Note P3_MELODY[] PROGMEM = {
  {71, 2}, {76, 2}, {79, 2}, {78, 2}, {76, 4}, {71, 4},
  {72, 2}, {76, 2}, {79, 2}, {81, 2}, {83, 4}, {0, 4},
};
const Note P3_BASS[] PROGMEM = {
  {40, 1}, {52, 1}, {40, 1}, {52, 1}, {40, 1}, {52, 1}, {50, 1}, {47, 1},
  {48, 1}, {60, 1}, {48, 1}, {60, 1}, {47, 1}, {59, 1}, {45, 1}, {47, 1},
  {40, 1}, {52, 1}, {40, 1}, {52, 1}, {40, 1}, {52, 1}, {50, 1}, {47, 1},
  {48, 1}, {60, 1}, {48, 1}, {60, 1}, {47, 1}, {59, 1}, {45, 1}, {47, 1},
};
const Track SONG_PHASE3[3] = {TRACK(P3_MELODY), TRACK(P3_BASS), NO_TRACK};

// ================================================================= boss fight (tense, chromatic)
const Note BOSS_MELODY[] PROGMEM = {
  {76, 1}, {77, 1}, {76, 1}, {75, 1}, {76, 4}, {81, 1}, {80, 1}, {79, 1}, {78, 1}, {77, 2}, {76, 2},
};
const Note BOSS_BASS[] PROGMEM = {
  {45, 1}, {45, 1}, {46, 1}, {45, 1}, {45, 1}, {45, 1}, {46, 1}, {45, 1},
  {45, 1}, {45, 1}, {46, 1}, {45, 1}, {48, 1}, {47, 1}, {46, 1}, {45, 1},
};
const Track SONG_BOSS[3] = {TRACK(BOSS_MELODY), TRACK(BOSS_BASS), NO_TRACK};

// ================================================================= short tunes (play once)
const Note STING_A[] PROGMEM = {{69, 2}, {76, 2}, {81, 4}};       // a phase begins
const Note STING_B[] PROGMEM = {{64, 2}, {69, 2}, {76, 4}};
const Note STING_C[] PROGMEM = {{45, 8}};
const Track SONG_STING[3] = {TRACK(STING_A), TRACK(STING_B), TRACK(STING_C)};

const Note CLEAR_A[] PROGMEM = {{76, 2}, {79, 2}, {84, 6}};       // a phase is cleared
const Note CLEAR_B[] PROGMEM = {{72, 2}, {76, 2}, {79, 6}};
const Note CLEAR_C[] PROGMEM = {{48, 10}};
const Track SONG_CLEAR[3] = {TRACK(CLEAR_A), TRACK(CLEAR_B), TRACK(CLEAR_C)};

const Note WIN_A[] PROGMEM = {{72, 2}, {76, 2}, {79, 2}, {84, 6}, {81, 2}, {83, 2}, {84, 8}};
const Note WIN_B[] PROGMEM = {{64, 2}, {67, 2}, {72, 2}, {76, 6}, {77, 2}, {79, 2}, {76, 8}};
const Note WIN_C[] PROGMEM = {{48, 6}, {55, 6}, {53, 4}, {48, 8}};
const Track SONG_VICTORY[3] = {TRACK(WIN_A), TRACK(WIN_B), TRACK(WIN_C)};

const Note OVER_A[] PROGMEM = {{64, 4}, {63, 4}, {62, 4}, {61, 12}};
const Note OVER_B[] PROGMEM = {{57, 4}, {56, 4}, {55, 4}, {52, 12}};
const Note OVER_C[] PROGMEM = {{45, 12}, {40, 12}};
const Track SONG_GAMEOVER[3] = {TRACK(OVER_A), TRACK(OVER_B), TRACK(OVER_C)};

// ================================================================= sound effects (buzzer 3)
// {from Hz, to Hz, ms, noise}; priorities: 1 shot ... 6 player hit
const SfxPart SFX_SHOT[] PROGMEM = {{2200, 1300, 35, 0}};
const SfxPart SFX_CHIRP[] PROGMEM = {{1500, 2600, 40, 0}};                  // auto-fire starts
const SfxPart SFX_EMPTY[] PROGMEM = {{2600, 2600, 8, 0}};                   // dry click
const SfxPart SFX_HIT[] PROGMEM = {{900, 650, 40, 0}};                      // enemy hit
const SfxPart SFX_BOOM[] PROGMEM = {{200, 900, 180, 1}};                    // enemy destroyed
const SfxPart SFX_BIG_BOOM[] PROGMEM = {{100, 600, 500, 1}, {600, 80, 300, 0}};
const SfxPart SFX_PLAYER_HIT[] PROGMEM = {{1200, 150, 350, 0}, {80, 300, 250, 1}};
const SfxPart SFX_PICKUP[] PROGMEM = {{800, 1600, 70, 0}, {1600, 2400, 70, 0}};
const SfxPart SFX_LIFE[] PROGMEM = {{660, 660, 80, 0}, {880, 880, 80, 0}, {1320, 1320, 140, 0}};
const SfxPart SFX_LOW_AMMO[] PROGMEM = {{1100, 1100, 60, 0}, {0, 0, 50, 0}, {1100, 1100, 60, 0}};
const SfxPart SFX_SIREN[] PROGMEM = {
  {600, 1200, 250, 0}, {1200, 600, 250, 0}, {600, 1200, 250, 0},
  {1200, 600, 250, 0}, {600, 1200, 250, 0}, {1200, 600, 250, 0},
};
const SfxPart SFX_ROAR[] PROGMEM = {{90, 400, 600, 1}};                     // boss changes form
const SfxPart SFX_TEST[] PROGMEM = {{400, 2000, 900, 0}};                   // LED test sweep
