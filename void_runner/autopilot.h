/*
 * autopilot.h -- test hook (!x): the ship flies itself, like a careful player.
 *
 * Used by the automatic tests to play whole games and to check that every phase can be
 * won without the shield. It follows the same rule as a player (it either moves or
 * fires, never both) and stays on the bottom row.
 *
 * How it decides, each time the ship could move one pixel:
 *   1. For every column it works out how many ms are left before a bullet, an enemy or
 *      a rock would touch the ship standing there ("time to impact").
 *   2. A column is reachable if the ship can fly there, one pixel per moveMs, without
 *      passing a column where something hits it on the way.
 *   3. Of the reachable columns that stay safe for a while, it takes the one nearest its
 *      target (an ammo crate when ammo is low, else the boss or the lowest enemy); if
 *      none is safe, the one where the danger arrives last.
 *   4. Lined up under the target in a safe column, it fires.
 */
#pragma once
#include "world.h"
#include "player.h"

const int32_t AUTO_NO_DANGER = 100000;   // "nothing will hit this column"
const int32_t AUTO_SAFE_MS = 450;        // a column must stay clear this long to stop in
#ifndef AUTOPILOT_REACT_MS
#define AUTOPILOT_REACT_MS 0             // PC tests: > 0 = rethinks only this often (slower)
#endif

// Returns the smaller of two times (ms).
int32_t minMs(int32_t a, int32_t b) {
  return a < b ? a : b;
}

// ms until something touches the ship if it stands with its left edge in column c.
int32_t impactMs(int8_t c) {
  int8_t top = player.y, bottom = player.y + 2;
  int32_t first = AUTO_NO_DANGER;
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {               // enemy bullets and mines
    const Shot& s = shots[i];
    if (!s.alive || s.kind == SHOT_PLAYER || s.y > bottom) continue;
    for (int8_t r = top; r <= bottom; r++) {
      int8_t down = r - s.y;                               // rows still to fall
      if (down < 0) continue;
      int8_t col = s.x + (s.dx ? s.dx * ((down + s.sideCount) / s.sideEvery) : 0);
      if (col < c || col > c + 2) continue;
      int32_t t = down ? (int32_t)(s.next - gameMs) + (int32_t)(down - 1) * s.stepMs : 0;
      first = minMs(first, t);
    }
  }
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {             // enemies and rocks coming down
    const Enemy& e = enemies[i];
    bool staysUp = e.type == EN_MINER || (e.type == EN_CRUISER && e.y >= 3);   // no longer sink
    if (!e.alive || staysUp || e.y > bottom) continue;
    const EnemyInfo& info = infoOf(e);
    int8_t margin = e.type == EN_WEAVER ? 2 : (e.type >= EN_ROCK_S ? 1 : 0);   // side moves
    if (e.x - margin > c + 2 || e.x + info.w - 1 + margin < c) continue;
    int8_t down = top - (e.y + info.h - 1);
    int32_t wait = (int32_t)(e.nextMove - gameMs);        // until its next step down
    int32_t t = down <= 0 ? 0 : (wait > 0 ? wait : 0) + (int32_t)(down - 1) * info.stepMs;
    first = minMs(first, t);
  }
  if (boss.alive && boss.y + boss.h > top && boss.x <= c + 2 && boss.x + boss.w > c)
    first = 0;                                             // the boss itself
  return first;
}

// Chooses what the autopilot aims at: an ammo crate when ammo is low, otherwise the boss,
// otherwise the lowest enemy above the ship. Returns that column, or -1 if there is none.
int8_t autopilotTarget() {
  if (player.ammo <= LOW_AMMO)
    for (uint8_t i = 0; i < MAX_PICKUPS; i++)
      if (pickups[i].alive && pickups[i].type == PK_AMMO) return pickups[i].x;
  if (boss.alive) return boss.x + boss.w / 2 - 1;
  int8_t target = -1, lowest = -100;
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    const Enemy& e = enemies[i];
    if (e.alive && e.y > lowest && e.y < player.y - 6 && e.x >= 0 && e.x < SCREEN_W) {
      lowest = e.y;
      target = e.x + infoOf(e).w / 2 - 1;
    }
  }
  return target < 0 ? -1 : constrain(target, 0, SCREEN_W - 3);
}

// The autopilot's turn (every game step while !x or the demo is on): when the ship may
// move again it picks the best reachable column and moves one pixel toward it, or fires
// when it is lined up under its target.
void autopilotStep() {
  if (player.lives == 0 || gameMs < player.nextMove) return;
  if (player.y < PLAYER_MAX_Y) {                           // go to the bottom row first
    movePlayer(KEY_DOWN);
    player.firing = false;
    player.nextMove = gameMs + playerShip().moveMs;
    return;
  }
  int32_t moveMs = playerShip().moveMs;
  static int8_t plan = -1;                                 // column chosen last time
  static uint32_t nextThink = 0;
  if (AUTOPILOT_REACT_MS && gameMs < nextThink && plan >= 0) {   // still reacting
    if (plan != player.x) {
      movePlayer(plan > player.x ? KEY_RIGHT : KEY_LEFT);
      player.firing = false;
    }
    player.nextMove = gameMs + moveMs;
    return;
  }
  int32_t impact[SCREEN_W - 2];
  for (int8_t c = 0; c <= SCREEN_W - 3; c++) impact[c] = impactMs(c);

  int8_t target = autopilotTarget();
  int8_t best = player.x;
  int32_t bestSpare = -AUTO_NO_DANGER;                     // fallback: latest danger
  int16_t bestDistance = 1000;                             // among safe columns
  bool bestSafe = false;
  for (int8_t dir = -1; dir <= 1; dir += 2) {
    for (int8_t c = player.x, steps = 0; c >= 0 && c <= SCREEN_W - 3; c += dir, steps++) {
      if (dir > 0 && steps == 0) continue;                 // the own column is checked once
      int32_t arrive = steps * moveMs;
      if (steps && impact[c] <= arrive + moveMs) break;    // would be hit on the way
      int32_t spare = impact[c] - arrive;
      bool safe = spare > AUTO_SAFE_MS;
      int16_t distance = (target >= 0 ? abs(c - target) * 4 : 0) + steps;
      if ((safe && (!bestSafe || distance < bestDistance)) || (!bestSafe && !safe && spare > bestSpare)) {
        best = c;
        bestSafe = safe;
        bestSpare = spare;
        bestDistance = distance;
      }
    }
  }
  plan = best;
  nextThink = gameMs + AUTOPILOT_REACT_MS;
  bool linedUp = target >= 0 && abs(target - player.x) <= 1 && player.ammo > 0;
  if (best != player.x) {
    movePlayer(best > player.x ? KEY_RIGHT : KEY_LEFT);   // moving: no fire
    player.firing = false;
  } else if (linedUp && impact[player.x] > AUTO_SAFE_MS) {
    if (!player.firing) player.burst = 0;                  // like pressing R
    player.firing = true;
  } else {
    player.firing = false;
  }
  player.nextMove = gameMs + moveMs;
}
