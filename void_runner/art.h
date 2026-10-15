/*
 * art.h -- pixel art, stored in flash. One 16-bit word per row, the leftmost pixel is
 * the top bit, so the 1s in each binary number draw the picture.
 */
#pragma once
#include <avr/pgmspace.h>

// ================================================================= the player's ships (3 x 3)
const uint16_t SHIP_BOLT[3] PROGMEM = {        // BOLT: balanced
  0b0100000000000000,                          //  .#.
  0b1110000000000000,                          //  ###
  0b1110000000000000,                          //  ###
};
const uint16_t SHIP_DART[3] PROGMEM = {        // DART: fast
  0b0100000000000000,                          //  .#.
  0b1110000000000000,                          //  ###
  0b1010000000000000,                          //  #.#
};
const uint16_t SHIP_TWIN[3] PROGMEM = {        // TWIN: two guns
  0b1010000000000000,                          //  #.#
  0b1110000000000000,                          //  ###
  0b1110000000000000,                          //  ###
};
const uint16_t SHIP_LEFT[3] PROGMEM = {        // BOLT flying left (logo)
  0b0110000000000000,                          //  .##
  0b1110000000000000,                          //  ###
  0b0110000000000000,                          //  .##
};
const uint16_t* const SHIP_UP = SHIP_BOLT;

// ================================================================= enemies and rocks
const uint16_t ENEMY_DRONE[2] PROGMEM = {      // drone: slow, shoots straight down
  0b1010000000000000,                          //  #.#
  0b0100000000000000,                          //  .#.
};
const uint16_t ENEMY_WEAVER[2] PROGMEM = {     // weaver: zig-zags
  0b1110000000000000,                          //  ###
  0b1010000000000000,                          //  #.#
};
const uint16_t ENEMY_MINER[2] PROGMEM = {      // mine-layer: crosses the top, drops mines
  0b1110000000000000,                          //  ###
  0b0100000000000000,                          //  .#.
};
const uint16_t ENEMY_CRUISER[3] PROGMEM = {    // cruiser: tough, aims at you
  0b0111000000000000,                          //  .###.
  0b1101100000000000,                          //  ##.##
  0b1010100000000000,                          //  #.#.#
};
const uint16_t ROCK_SMALL[2] PROGMEM = {
  0b1100000000000000,                          //  ##
  0b1100000000000000,                          //  ##
};
const uint16_t ROCK_BIG[3] PROGMEM = {         // breaks into two small rocks
  0b0110000000000000,                          //  .##
  0b1110000000000000,                          //  ###
  0b1100000000000000,                          //  ##.
};

// ================================================================= pickups (blinking)
const uint16_t PICKUP_AMMO[3] PROGMEM = {      // ammo crate
  0b1110000000000000,                          //  ###
  0b1010000000000000,                          //  #.#
  0b1110000000000000,                          //  ###
};
const uint16_t PICKUP_LIFE[3] PROGMEM = {      // extra life
  0b0100000000000000,                          //  .#.
  0b1110000000000000,                          //  ###
  0b0100000000000000,                          //  .#.
};

// ================================================================= bosses
const uint16_t BOSS_WARDEN[4] PROGMEM = {      // phase 1: wide and slow, spread shots
  0b0011111100000000,                          //  ..######..
  0b0111111110000000,                          //  .########.
  0b1101111011000000,                          //  ##.####.##
  0b1001001001000000,                          //  #..#..#..#
};
const uint16_t BOSS_HUNTER[3] PROGMEM = {      // phase 2: small and fast, dashes and charges
  0b1000100000000000,                          //  #...#
  0b1111100000000000,                          //  #####
  0b0101000000000000,                          //  .#.#.
};
const uint16_t BOSS_KING[5] PROGMEM = {        // phase 3: the Void King, first form
  0b0011111111000000,                          //  ..########..
  0b0111111111100000,                          //  .##########.
  0b1110011001110000,                          //  ###..##..###
  0b1111111111110000,                          //  ############
  0b0101010101010000,                          //  .#.#.#.#.#.#
};
const uint16_t BOSS_KING_ANGRY[5] PROGMEM = {  // second form: the eye opens
  0b1011111111010000,                          //  #.########.#
  0b0111100111100000,                          //  .####..####.
  0b1110100101110000,                          //  ###.#..#.###
  0b1111111111110000,                          //  ############
  0b1010100101010000,                          //  #.#.#..#.#.#
};

// ================================================================= logo: "VOID", bold 10 x 5 letters
const uint8_t LOGO_W = 10, LOGO_H = 5, LOGO_X = 3;
const uint8_t LOGO_Y[4] = {0, 6, 12, 18};      // V in the red band ... D in the green band
const uint16_t LOGO_LETTERS[4][LOGO_H] PROGMEM = {
  {                                            // V
    0b1100000011000000,                        //  ##......##
    0b0110000110000000,                        //  .##....##.
    0b0011001100000000,                        //  ..##..##..
    0b0001111000000000,                        //  ...####...
    0b0000110000000000,                        //  ....##....
  },
  {                                            // O
    0b0111111110000000,                        //  .########.
    0b1100000011000000,                        //  ##......##
    0b1100000011000000,                        //  ##......##
    0b1100000011000000,                        //  ##......##
    0b0111111110000000,                        //  .########.
  },
  {                                            // I
    0b0011111100000000,                        //  ..######..
    0b0000110000000000,                        //  ....##....
    0b0000110000000000,                        //  ....##....
    0b0000110000000000,                        //  ....##....
    0b0011111100000000,                        //  ..######..
  },
  {                                            // D
    0b1111111000000000,                        //  #######...
    0b1100001100000000,                        //  ##....##..
    0b1100000110000000,                        //  ##.....##.
    0b1100001100000000,                        //  ##....##..
    0b1111111000000000,                        //  #######...
  },
};
