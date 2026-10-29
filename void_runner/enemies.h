/*
 * enemies.h -- how each enemy and rock moves and shoots. Every one moves slowly (the
 * player is always faster); each type has its own shot:
 *   drone     sinks, shoots straight down
 *   weaver    sinks while zig-zagging, shoots straight down now and then
 *   miner     crosses the top of the screen and drops mines
 *   cruiser   stops near the top, slides sideways and fires shots aimed at the player
 *   rocks     fall and drift; a big rock breaks into two small ones
 */
#pragma once
#include "world.h"

const int8_t ENEMY_FIRE_MAX_Y = 9;

// True if enemy e, placed at (x, y), would touch another enemy: with gap = 1 also when they
// are only side by side with no empty pixel between them.
bool touchesOther(const Enemy& e, int8_t x, int8_t y, uint8_t gap) {
  const EnemyInfo& info = infoOf(e);
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    const Enemy& o = enemies[i];
    if (!o.alive || &o == &e) continue;
    const EnemyInfo& oi = infoOf(o);
    if (boxesTouch(x - gap, y - gap, info.w + 2 * gap, info.h + 2 * gap, o.x, o.y, oi.w, oi.h))
      return true;
  }
  return false;
}

// True if enemy e may move to (x, y). On 16 x 24 LEDs two enemies on the same pixels merge
// into one unreadable blob, so a move may not bring it closer than one empty pixel to
// another enemy (a move is still allowed if they were already that close, so two enemies
// side by side can't block each other forever).
bool canMoveTo(const Enemy& e, int8_t x, int8_t y) {
  if (touchesOther(e, x, y, 0) && !touchesOther(e, e.x, e.y, 0)) return false;
  return !touchesOther(e, x, y, 1) || touchesOther(e, e.x, e.y, 1);
}

// Moves an enemy one pixel down. If something is in the way it steps aside instead (its
// own direction first), or waits for this step.
void moveDown(Enemy& e) {
  if (canMoveTo(e, e.x, e.y + 1)) { e.y++; return; }
  const int8_t maxX = SCREEN_W - infoOf(e).w;
  int8_t d = e.dir ? e.dir : 1;
  for (uint8_t i = 0; i < 2; i++, d = -d) {
    int8_t x = e.x + d;
    if (x >= 0 && x <= maxX && canMoveTo(e, x, e.y)) { e.x = x; return; }
  }
}

// Moves an enemy one pixel sideways in its direction; if something is in the way (or the
// screen edge: only miners fly in and out at the sides) it turns around instead. Returns
// true if it moved.
bool moveSide(Enemy& e) {
  int8_t x = e.x + e.dir;
  bool inside = e.type == EN_MINER || (x >= 0 && x <= SCREEN_W - infoOf(e).w);
  if (inside && canMoveTo(e, x, e.y)) { e.x = x; return true; }
  e.dir = -e.dir;
  return false;
}

// Puts an enemy on the screen just above the top (miners come in from the side).
void spawnFromTable(uint8_t type, int8_t x) {
  const EnemyInfo& info = ENEMY_INFO[type];
  if (type == EN_MINER) {
    bool fromLeft = x < SCREEN_W / 2;
    Enemy* e = spawnEnemy(type, fromLeft ? -3 : SCREEN_W, 1 + random(3));
    if (!e) return;
    e->dir = fromLeft ? 1 : -1;
    while (e->x > -20 && e->x < SCREEN_W + 20 && touchesOther(*e, e->x, e->y, 1))
      e->x -= e->dir;                    // another one is there: queue up further outside
    return;
  }
  Enemy* e = spawnEnemy(type, constrain(x, 0, SCREEN_W - info.w), -info.h);
  if (!e) return;
  // Something is already entering there: take the nearest free column (+1, -1, +2, -2 ...),
  // and if the whole row is busy, queue up higher above the screen.
  for (int8_t d = 1; d < 2 * SCREEN_W && touchesOther(*e, e->x, e->y, 1); d++) {
    int8_t tryX = e->x + ((d & 1) ? (d + 1) / 2 : -d / 2);
    if (tryX >= 0 && tryX <= SCREEN_W - info.w && !touchesOther(*e, tryX, e->y, 1)) e->x = tryX;
  }
  while (e->y > -20 && touchesOther(*e, e->x, e->y, 1)) e->y--;
  if (type == EN_ROCK_S || type == EN_ROCK_B) e->dir = random(3) - 1;   // drift
}

// Moves one enemy or rock for one game step and lets it shoot; each type has its own
// pattern (see the list at the top). Anything that leaves at the bottom is removed.
void updateEnemy(Enemy& e) {
  const EnemyInfo& info = infoOf(e);
  // Enemies only shoot from the upper part of the screen: a bullet fired right next to
  // the ship could not be dodged. The ship may fly up to row 9 too, so a bullet must also
  // start at least 3 rows above it.
  bool canShoot = e.y >= 0 && e.y <= ENEMY_FIRE_MAX_Y && e.y + info.h + 3 <= player.y;
  switch (e.type) {
    case EN_DRONE:
      if (gameMs >= e.nextMove) { moveDown(e); e.nextMove += info.stepMs; }
      if (gameMs >= e.nextShot) {
        if (canShoot) enemyShot(e.x + 1, e.y + 2, 0, 1, 110);
        e.nextShot = gameMs + fireDelay(2600 + random(1800));
      }
      break;

    case EN_WEAVER:
      if (gameMs >= e.nextMove) { moveDown(e); e.nextMove += info.stepMs; }
      if (gameMs >= e.nextSide) {              // zig-zag: three steps one way, then back
        if (!moveSide(e)) e.steps = 0;         // blocked: zig-zags back early
        else if (++e.steps >= 3 || e.x <= 0 || e.x >= SCREEN_W - info.w) {
          e.dir = -e.dir;
          e.steps = 0;
        }
        e.nextSide = gameMs + 220;
      }
      if (gameMs >= e.nextShot) {
        if (canShoot) enemyShot(e.x + 1, e.y + 2, 0, 1, 100);
        e.nextShot = gameMs + fireDelay(4000 + random(2000));
      }
      break;

    case EN_MINER:
      if (gameMs >= e.nextMove) { moveSide(e); e.nextMove += info.stepMs; }   // blocked: turns back
      if (gameMs >= e.nextShot) {
        if (e.x >= 0 && e.x <= SCREEN_W - info.w) newShot(e.x + 1, e.y + 2, SHOT_MINE, 300);
        e.nextShot = gameMs + fireDelay(1300);
      }
      if ((e.dir > 0 && e.x > SCREEN_W) || (e.dir < 0 && e.x < -info.w)) e.alive = false;
      break;

    case EN_CRUISER:
      if (e.y < 3) {
        if (gameMs >= e.nextMove) { moveDown(e); e.nextMove += info.stepMs; }
      } else if (gameMs >= e.nextSide) {
        if (moveSide(e) && (e.x <= 0 || e.x >= SCREEN_W - info.w)) e.dir = -e.dir;
        e.nextSide = gameMs + 650;
      }
      if (gameMs >= e.nextShot) {
        if (e.y >= 0) aimedShot(e.x + 2, e.y + 3, 100);
        e.nextShot = gameMs + fireDelay(1800);
      }
      break;

    default:                                   // rocks
      if (gameMs >= e.nextMove) { moveDown(e); e.nextMove += info.stepMs; }
      if (e.dir && gameMs >= e.nextSide) {
        if (e.x + e.dir < 0 || e.x + e.dir > SCREEN_W - info.w) e.dir = -e.dir;   // edge
        else moveSide(e);
        e.nextSide = gameMs + 1400;
      }
      break;
  }
  if (e.y >= SCREEN_H) e.alive = false;        // left the screen at the bottom
  if (e.flash) e.flash--;
}

// Updates every enemy and rock that is alive (once per game step).
void updateEnemies() {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++)
    if (enemies[i].alive) updateEnemy(enemies[i]);
}

// Draws every enemy and rock; one that was just hit is hidden for a moment.
void drawEnemies() {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    const Enemy& e = enemies[i];
    if (!e.alive || e.flash) continue;   // a hit enemy disappears for a moment (120 ms)
    const EnemyInfo& info = infoOf(e);
    drawBitmap(e.x, e.y, info.sprite, info.w, info.h, true);
  }
}
