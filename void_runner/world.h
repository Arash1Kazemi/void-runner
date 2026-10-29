/*
 * world.h -- everything that lives on the game screen: the player, bullets, enemies and
 * rocks, pickups, explosion particles and the boss.
 *
 * Only fixed-size arrays (no malloc): the Mega has 8 KB of RAM and nothing may run out in
 * the middle of a game. Positions are in LED pixels, (0,0) = top-left of the game screen.
 * All timing uses gameMs, a clock that advances one frame at a time and stops on pause.
 */
#pragma once
#include "config.h"
#include "display.h"
#include "sound.h"
#include "tunes.h"
#include "art.h"
#include "storage.h"

uint32_t gameMs = 0;
const int8_t PLAYER_MIN_Y = 9, PLAYER_MAX_Y = SCREEN_H - 3;

// ================================================================= the three ships
struct ShipInfo {
  const char* name;                      // in flash
  const uint16_t* sprite;
  uint8_t moveMs;                        // ms per pixel while gliding
  uint16_t fireMs;                       // ms between shots while R is held
  uint8_t ammoMax;
  bool twin;                             // two guns: two bullets, two ammo per shot
  uint8_t speedLevel, ammoLevel, fireLevel;   // 1..3, shown in the hangar
};
const char NAME_BOLT[] PROGMEM = "BOLT";
const char NAME_DART[] PROGMEM = "DART";
const char NAME_TWIN[] PROGMEM = "TWIN";
const ShipInfo SHIPS[3] = {
  {NAME_BOLT, SHIP_BOLT, 70, 200, 60, false, 2, 2, 2},
  {NAME_DART, SHIP_DART, 45, 220, 40, false, 3, 1, 2},
  {NAME_TWIN, SHIP_TWIN, 95, 260, 80, true, 1, 3, 3},
};

// ================================================================= player
struct Player {
  int8_t x, y;                           // top-left of the 3 x 3 ship
  uint8_t ship, lives, ammo;
  uint16_t score;
  bool firing;                           // R was pressed and no move key cancelled it
  uint8_t burst;                         // shots since R was pressed
  char moveKey;                          // move key that is held (0 = none)
  bool gliding;                          // the key was held long enough to glide
  uint32_t nextShot, nextMove, shieldUntil;
  bool lowAmmoWarned;
};
Player player;
bool testShield = false;                 // test hook: the player can't be hit
bool autopilot = false;                  // test hook: the game plays itself

// The table entry (speed, fire rate, ammo ...) of the ship in use.
const ShipInfo& playerShip() {
  return SHIPS[player.ship];
}

// True if the player's ship has a lit pixel at screen position (x, y): exact hits.
bool shipPixel(int8_t x, int8_t y) {
  int8_t c = x - player.x, r = y - player.y;
  if (c < 0 || c > 2 || r < 0 || r > 2) return false;
  return pgm_read_word(playerShip().sprite + r) & (0x8000u >> c);
}

// Adds points to the score, which stops at 9999 (four digits fit on the screen).
void addScore(uint16_t points) {
  player.score = (player.score + points > 9999) ? 9999 : player.score + points;
}

// ================================================================= shots (player and enemy)
enum ShotKind : uint8_t { SHOT_PLAYER, SHOT_ENEMY, SHOT_MINE };
struct Shot {
  int8_t x, y, dx;                       // dx: sideways step (-1, 0, 1)
  uint8_t sideEvery, sideCount;          // sideways step every n down steps
  uint16_t stepMs;                       // ms per pixel
  uint8_t kind;
  uint32_t next;
  bool alive;
};
const uint8_t MAX_SHOTS = 28, MAX_PLAYER_SHOTS = 6;
Shot shots[MAX_SHOTS];

// Counts the player's bullets in the air (at most MAX_PLAYER_SHOTS).
uint8_t playerShotCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_SHOTS; i++)
    if (shots[i].alive && shots[i].kind == SHOT_PLAYER) n++;
  return n;
}

// Puts a new bullet or mine in a free slot of the shot pool and returns it, or NULL when
// all slots are in use.
Shot* newShot(int8_t x, int8_t y, uint8_t kind, uint16_t stepMs) {
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {
    Shot& s = shots[i];
    if (s.alive) continue;
    s = {x, y, 0, 1, 0, stepMs, kind, gameMs + stepMs, true};
    return &s;
  }
  return NULL;                           // pool full: the shot is simply not fired
}

// Fires an enemy bullet: straight down, or with dx = -1 / 1 one step sideways every
// sideEvery rows (spread shots).
void enemyShot(int8_t x, int8_t y, int8_t dx, uint8_t sideEvery, uint8_t stepMs) {
  Shot* s = newShot(x, y, SHOT_ENEMY, stepMs);
  if (s) {
    s->dx = dx;
    s->sideEvery = sideEvery;
  }
}

// A shot that heads for the player: it steps sideways once every few rows.
void aimedShot(int8_t x, int8_t y, uint8_t stepMs) {
  int8_t cols = (player.x + 1) - x, rows = player.y - y;
  if (rows < 1) rows = 1;
  int8_t dx = (cols > 0) - (cols < 0);
  uint8_t every = cols ? constrain(rows / abs(cols), 1, 6) : 1;
  enemyShot(x, y, dx, every, stepMs);
}

// ================================================================= enemies and rocks
enum EnemyType : uint8_t { EN_DRONE, EN_WEAVER, EN_MINER, EN_CRUISER, EN_ROCK_S, EN_ROCK_B };
struct EnemyInfo {
  const uint16_t* sprite;
  uint8_t w, h, hp;
  uint8_t score;
  uint16_t stepMs;                       // ms per pixel of its main movement
};
const EnemyInfo ENEMY_INFO[6] = {
  {ENEMY_DRONE, 3, 2, 1, 10, 450},
  {ENEMY_WEAVER, 3, 2, 1, 15, 600},
  {ENEMY_MINER, 3, 2, 2, 20, 280},
  {ENEMY_CRUISER, 5, 3, 5, 40, 500},
  {ROCK_SMALL, 3, 3, 1, 5, 360},
  {ROCK_BIG, 4, 3, 2, 10, 520},
};
struct Enemy {
  int8_t x, y, dir;
  uint8_t type, hp, flash, steps;
  uint32_t nextMove, nextSide, nextShot;
  bool alive;
};
const uint8_t MAX_ENEMIES = 16;
Enemy enemies[MAX_ENEMIES];

// The table entry (sprite, size, health, points, speed) of an enemy's type.
const EnemyInfo& infoOf(const Enemy& e) {
  return ENEMY_INFO[e.type];
}

// Scales an enemy fire interval by the difficulty (EASY slower, HARD faster).
uint16_t fireDelay(uint16_t ms) {
  return (uint32_t)ms * DIFF_FIRE_SCALE[settings.difficulty] / 100;
}

// Puts a new enemy or rock of a type at (x, y) in a free slot and returns it, or NULL
// when all slots are in use.
Enemy* spawnEnemy(uint8_t type, int8_t x, int8_t y) {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    Enemy& e = enemies[i];
    if (e.alive) continue;
    const EnemyInfo& info = ENEMY_INFO[type];
    e = {x, y, 1, type, info.hp, 0, 0, gameMs + info.stepMs, gameMs + 300,
         gameMs + fireDelay(1200 + random(1500)), true};
    return &e;
  }
  return NULL;
}

// Counts the enemies and rocks on the screen.
uint8_t enemiesAlive() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) n += enemies[i].alive;
  return n;
}

// ================================================================= pickups
enum PickupType : uint8_t { PK_AMMO, PK_LIFE };
struct Pickup {
  int8_t x, y;
  uint8_t type;
  uint32_t next;
  bool alive;
};
const uint8_t MAX_PICKUPS = 4;
Pickup pickups[MAX_PICKUPS];

// Drops a pickup (ammo crate or extra life) at (x, y) if a slot is free.
void spawnPickup(uint8_t type, int8_t x, int8_t y) {
  for (uint8_t i = 0; i < MAX_PICKUPS; i++)
    if (!pickups[i].alive) {
      pickups[i] = {(int8_t)constrain(x, 0, SCREEN_W - 3), y, type, gameMs + 350, true};
      return;
    }
}

// True if a pickup of this type is on the screen.
bool pickupAlive(uint8_t type) {
  for (uint8_t i = 0; i < MAX_PICKUPS; i++)
    if (pickups[i].alive && pickups[i].type == type) return true;
  return false;
}

// ================================================================= explosion particles
// Only the boss and the player's ship explode into flying pixels; ordinary enemies just
// vanish, so the small screen does not fill up with debris that looks like bullets.
struct Particle {
  int16_t x16, y16;                      // position in 1/16 pixel
  int8_t vx, vy;                         // speed in 1/16 pixel per frame
  uint8_t life;                          // frames left
};
const uint8_t MAX_PARTICLES = 40;
Particle particles[MAX_PARTICLES];

// Starts an explosion: up to `count` pixels fly out from (cx, cy) at random speeds.
void explode(int8_t cx, int8_t cy, uint8_t count, uint8_t speed) {
  for (uint8_t i = 0; i < MAX_PARTICLES && count; i++) {
    Particle& p = particles[i];
    if (p.life) continue;
    p = {(int16_t)(cx * 16 + 8), (int16_t)(cy * 16 + 8), (int8_t)random(-speed, speed + 1),
         (int8_t)random(-speed, speed + 1), (uint8_t)random(8, 18)};
    count--;
  }
}

// ================================================================= boss
enum BossKind : uint8_t { BOSS_KIND_WARDEN, BOSS_KIND_HUNTER, BOSS_KIND_KING };
struct Boss {
  bool alive;
  uint8_t kind, w, h, flash, form, mode;
  int8_t x, y, dir, targetX;
  int16_t hp, maxHp;
  uint32_t nextMove, nextShot, nextAction, modeUntil;
  uint8_t burst;
};
Boss boss;

// The picture of the current boss (the VOID KING has one per form).
const uint16_t* bossSprite() {
  if (boss.kind == BOSS_KIND_WARDEN) return BOSS_WARDEN;
  if (boss.kind == BOSS_KIND_HUNTER) return BOSS_HUNTER;
  return boss.form ? BOSS_KING_ANGRY : BOSS_KING;
}

// ================================================================= helpers
// True when two boxes overlap (the ship against enemies, rocks, the boss, pickups).
bool boxesTouch(int8_t ax, int8_t ay, uint8_t aw, uint8_t ah,
                int8_t bx, int8_t by, uint8_t bw, uint8_t bh) {
  return ax < bx + bw && bx < ax + aw && ay < by + bh && by < ay + ah;
}

uint8_t shakeFrames = 0;
// Shakes the screen for some pictures (a longer shake already running is not cut short).
void shake(uint8_t frames) {
  if (frames > shakeFrames) shakeFrames = frames;
}

// Empties the game screen: no shots, enemies, pickups, explosion pixels or boss.
void worldClear() {
  for (uint8_t i = 0; i < MAX_SHOTS; i++) shots[i].alive = false;
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = false;
  for (uint8_t i = 0; i < MAX_PICKUPS; i++) pickups[i].alive = false;
  for (uint8_t i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0;
  boss.alive = false;
}
