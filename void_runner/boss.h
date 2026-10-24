/*
 * boss.h -- the three bosses (health is shown on the red status bar).
 *   WARDEN     (phase 1) wide and slow, slides along the top, 3-way spread shots,
 *              drops rocks; gets faster below half health
 *   HUNTER     (phase 2) small and FAST: dashes to a new spot, fires an aimed pair,
 *              and every few seconds charges straight down at the player
 *   VOID KING  (phase 3) huge; 5-way spread and calls in drones; below half health it
 *              changes form (the eye opens), fires aimed volleys and makes rocks rain
 */
#pragma once
#include "world.h"
#include "enemies.h"

enum BossMode : uint8_t { BM_ENTER, BM_FIGHT, BM_DASH, BM_AIM, BM_CHARGE_DOWN, BM_CHARGE_UP };
const uint16_t BOSS_SCORE[3] = {200, 300, 500};

// Gets the boss of a phase ready: its size, its health (scaled by the difficulty), a start
// just above the screen so it slides in, and the times of its first shot and action.
void bossStart(uint8_t kind) {
  boss = {};
  boss.alive = true;
  boss.kind = kind;
  const uint8_t W[3] = {10, 5, 12}, H[3] = {4, 3, 5}, HP[3] = {24, 22, 40};
  boss.w = W[kind];
  boss.h = H[kind];
  boss.maxHp = (int16_t)HP[kind] * DIFF_BOSS_HP[settings.difficulty] / 100;
  boss.hp = boss.maxHp;
  boss.x = (SCREEN_W - boss.w) / 2;
  boss.y = -boss.h;
  boss.dir = 1;
  boss.mode = BM_ENTER;
  boss.nextMove = gameMs;
  boss.nextShot = gameMs + 2500;
  boss.nextAction = gameMs + 4000;
}

// True when the boss has lost half its health or more (it then attacks harder).
bool bossAngry() {
  return boss.hp * 2 <= boss.maxHp;
}

// Moves the boss one pixel sideways every `ms`, turning around at the screen edges.
void bossSlide(uint16_t ms) {
  if (gameMs < boss.nextMove) return;
  boss.x += boss.dir;
  if (boss.x <= 0 || boss.x >= SCREEN_W - boss.w) boss.dir = -boss.dir;
  boss.nextMove = gameMs + ms;
}

// WARDEN's turn: slides along the top, fires a 3-way spread (5-way when angry) and drops
// rocks every 5 seconds.
void updateWarden() {
  bossSlide(bossAngry() ? 90 : 150);
  if (gameMs >= boss.nextShot) {       // 3-way spread from the three "guns"
    int8_t y = boss.y + boss.h;
    enemyShot(boss.x + 1, y, -1, 2, 90);
    enemyShot(boss.x + 4, y, 0, 1, 90);
    enemyShot(boss.x + 8, y, 1, 2, 90);
    if (bossAngry()) {
      enemyShot(boss.x, y, -1, 1, 90);
      enemyShot(boss.x + 9, y, 1, 1, 90);
    }
    boss.nextShot = gameMs + fireDelay(bossAngry() ? 1000 : 1600);
  }
  if (gameMs >= boss.nextAction) {     // drops rocks
    spawnFromTable(EN_ROCK_S, random(SCREEN_W - 3));
    if (bossAngry()) spawnFromTable(EN_ROCK_S, random(SCREEN_W - 3));
    boss.nextAction = gameMs + 5000;
  }
}

// HUNTER's turn, a small state machine (boss.mode): FIGHT picks the next move, DASH flies
// fast to a random column, AIM fires a pair of aimed shots, CHARGE_DOWN / CHARGE_UP dives
// at the player and climbs back.
void updateHunter() {
  switch (boss.mode) {
    case BM_FIGHT:
      if (gameMs < boss.modeUntil) break;        // short rest after shooting
      if (gameMs >= boss.nextAction) {           // time for a charge
        boss.mode = BM_CHARGE_DOWN;
        boss.flash = 15;
        boss.burst = 0;
        boss.nextMove = gameMs + 300;            // short warning blink first
      } else {
        boss.targetX = random(SCREEN_W - boss.w + 1);
        boss.mode = BM_DASH;
      }
      break;
    case BM_DASH:                                // fast: one pixel every 40 ms
      if (gameMs >= boss.nextMove) {
        boss.x += (boss.targetX > boss.x) - (boss.targetX < boss.x);
        boss.nextMove = gameMs + (bossAngry() ? 30 : 40);
        if (boss.x == boss.targetX) {
          boss.mode = BM_AIM;
          boss.modeUntil = gameMs + 350;
          boss.burst = 0;
        }
      }
      break;
    case BM_AIM:                                 // pause, then an aimed pair of shots
      if (gameMs >= boss.modeUntil) {
        aimedShot(boss.x + 2, boss.y + boss.h, 85);
        boss.modeUntil = gameMs + 160;
        if (++boss.burst >= (bossAngry() ? 3 : 2)) {
          boss.mode = BM_FIGHT;
          boss.modeUntil = gameMs + fireDelay(500);
        }
      }
      break;
    case BM_CHARGE_DOWN:                         // dives, steering toward the player
      if (gameMs >= boss.nextMove) {
        boss.y++;
        if (++boss.burst % 3 == 0) boss.x += (player.x - 1 > boss.x) - (player.x - 1 < boss.x);
        boss.x = constrain(boss.x, 0, SCREEN_W - boss.w);
        boss.nextMove = gameMs + 35;
        if (boss.y >= 15) boss.mode = BM_CHARGE_UP;
      }
      break;
    case BM_CHARGE_UP:
      if (gameMs >= boss.nextMove) {
        boss.y--;
        boss.nextMove = gameMs + 60;
        if (boss.y <= 1) {
          boss.mode = BM_FIGHT;
          boss.nextAction = gameMs + (bossAngry() ? 5000 : 7000);
        }
      }
      break;
  }
}

// VOID KING's turn. Below half health it changes form once (eye opens, roar, shake).
// Form 1: 5-way spread and calls in drones. Form 2: aimed triple volleys and rock rain.
void updateKing() {
  if (!boss.form && bossAngry()) {               // second form
    boss.form = 1;
    boss.flash = 20;
    shake(25);
    explode(boss.x + boss.w / 2, boss.y + 2, 14, 12);
    soundSfx(SFX(SFX_ROAR), 6);
    Serial.println(F("[VR] boss form 2"));
  }
  bossSlide(boss.form ? 200 : 260);
  int8_t y = boss.y + boss.h;
  if (gameMs >= boss.nextShot) {
    if (!boss.form) {                            // 5-way spread
      enemyShot(boss.x + 1, y, -1, 1, 95);
      enemyShot(boss.x + 3, y, -1, 2, 95);
      enemyShot(boss.x + 6, y, 0, 1, 95);
      enemyShot(boss.x + 8, y, 1, 2, 95);
      enemyShot(boss.x + 10, y, 1, 1, 95);
      boss.nextShot = gameMs + fireDelay(2200);
    } else {                                     // aimed volley from three points
      aimedShot(boss.x + 2, y, 90);
      aimedShot(boss.x + 6, y, 90);
      aimedShot(boss.x + 9, y, 90);
      boss.nextShot = gameMs + fireDelay(1700);
    }
  }
  if (gameMs >= boss.nextAction) {
    if (!boss.form) {                            // calls in drones
      uint8_t drones = 0;
      for (uint8_t i = 0; i < MAX_ENEMIES; i++)
        drones += enemies[i].alive && enemies[i].type == EN_DRONE;
      if (drones < 2) spawnFromTable(EN_DRONE, random(SCREEN_W - 3));
      boss.nextAction = gameMs + 6000;
    } else {                                     // rock rain
      spawnFromTable(EN_ROCK_S, random(SCREEN_W - 3));
      boss.nextAction = gameMs + 1200;
    }
  }
}

// Moves the boss for one game step: first it slides in from the top, then it runs the
// behaviour of its kind.
void updateBoss() {
  if (!boss.alive) return;
  if (boss.flash) boss.flash--;
  if (boss.mode == BM_ENTER) {                   // slides in from the top
    if (gameMs >= boss.nextMove) {
      boss.y++;
      boss.nextMove = gameMs + 120;
      if (boss.y >= 1) {
        boss.mode = BM_FIGHT;
        boss.nextAction = gameMs + 4000;
      }
    }
    return;
  }
  if (boss.kind == BOSS_KIND_WARDEN) updateWarden();
  else if (boss.kind == BOSS_KIND_HUNTER) updateHunter();
  else updateKing();
}

// Draws the boss; while boss.flash counts down it blinks (before a dive, at the form
// change and while it burns).
void drawBoss(uint16_t frameNo) {
  if (!boss.alive || (boss.flash && (frameNo & 8))) return;   // blinks (warning, burning)
  drawBitmap(boss.x, boss.y, bossSprite(), boss.w, boss.h, true);
}
