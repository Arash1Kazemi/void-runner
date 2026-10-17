/*
 * player.h -- the player's ship: moving, firing, ammo and lives.
 *
 * Key rules (agreed in the design):
 *   move   tap = one pixel; hold = after a short delay the ship glides at its speed
 *   fire   press R = one bullet; keep holding = auto-fire (a chirp, the nose blinks);
 *          release R = stop
 *   The newest key wins: a move key stops the fire (press R again to fire again), and
 *   while R is held the ship does not glide.
 * Every shot costs ammo (TWIN: two bullets, two ammo). At LOW_AMMO a warning beeps and
 * the yellow bar blinks. A hit costs a life and gives two seconds of blinking shield.
 */
#pragma once
#include "world.h"
#include "input.h"

const char MOVE_KEYS[4] = {KEY_UP, KEY_LEFT, KEY_DOWN, KEY_RIGHT};

// Gets the ship ready: bottom centre, full ammo, not firing. newGame = also the ship from
// the hangar, the lives of the difficulty and score 0 (a new phase keeps those).
void playerStart(bool newGame) {
  if (newGame) {
    player.ship = settings.ship;
    player.lives = DIFF_LIVES[settings.difficulty];
    player.score = 0;
  }
  player.x = (SCREEN_W - 3) / 2;
  player.y = SCREEN_H - 4;
  player.ammo = playerShip().ammoMax;    // every phase starts with a full magazine
  player.firing = false;
  player.moveKey = 0;
  player.shieldUntil = 0;
  player.lowAmmoWarned = false;
}

// True while the ship can't be hit (2 s after a hit, or the test hook !i).
bool playerShielded() {
  return testShield || gameMs < player.shieldUntil;
}

// Moves the ship one pixel in the direction of a move key, staying on the screen and in
// rows 9-23.
void movePlayer(char key) {
  int8_t dx = (key == KEY_RIGHT) - (key == KEY_LEFT);
  int8_t dy = (key == KEY_DOWN) - (key == KEY_UP);
  player.x = constrain(player.x + dx, 0, SCREEN_W - 3);
  player.y = constrain(player.y + dy, PLAYER_MIN_Y, PLAYER_MAX_Y);
}

// Fires one shot (TWIN: two bullets) if there is ammo and a free bullet slot. Plays the
// shot sound (a chirp when auto-fire starts) and warns once when ammo gets low.
void playerFire() {
  const ShipInfo& ship = playerShip();
  uint8_t bullets = (ship.twin && player.ammo >= 2) ? 2 : 1;
  if (player.ammo == 0) {
    soundSfx(SFX(SFX_EMPTY), 2);
    player.nextShot = gameMs + ship.fireMs;
    return;
  }
  if (playerShotCount() + bullets > MAX_PLAYER_SHOTS) return;   // try again next step
  if (bullets == 2) {
    newShot(player.x, player.y - 1, SHOT_PLAYER, PLAYER_SHOT_MS);
    newShot(player.x + 2, player.y - 1, SHOT_PLAYER, PLAYER_SHOT_MS);
  } else {
    newShot(player.x + 1, player.y - 1, SHOT_PLAYER, PLAYER_SHOT_MS);
  }
  player.ammo -= bullets;
  player.nextShot = gameMs + ship.fireMs;
  if (++player.burst == 2) soundSfx(SFX(SFX_CHIRP), 2);   // auto-fire has started
  else soundSfx(SFX(SFX_SHOT), 1);
  if (player.ammo <= LOW_AMMO && !player.lowAmmoWarned) {
    player.lowAmmoWarned = true;
    soundSfx(SFX(SFX_LOW_AMMO), 4);
  }
}

// Keys: called once per picture (key presses are counted per picture).
void playerKeys() {
  if (autopilot) return;
  for (uint8_t i = 0; i < 4; i++)
    if (keyPressed(MOVE_KEYS[i])) {      // newest move key takes over and stops the fire
      player.moveKey = MOVE_KEYS[i];
      player.gliding = false;
      player.nextMove = gameMs;          // first pixel right away
      player.firing = false;
    }
  if (keyPressed(KEY_FIRE)) {
    player.firing = true;
    player.burst = 0;
    player.nextShot = gameMs;            // first bullet right away
  }
  if (player.moveKey && !keyHeld(player.moveKey)) {   // let go: another held move key?
    player.moveKey = 0;
    for (uint8_t i = 0; i < 4; i++)
      if (keyHeld(MOVE_KEYS[i])) {
        player.moveKey = MOVE_KEYS[i];
        player.gliding = true;
        player.nextMove = gameMs + playerShip().moveMs;
      }
  }
}

// Movement and fire: called once per game step (20 ms of game time).
void playerStep() {
  if (player.lives == 0) return;
  if (player.firing) {
    if (!autopilot && !keyHeld(KEY_FIRE)) player.firing = false;   // R released
    else if (gameMs >= player.nextShot) playerFire();
  }
  if (!player.firing && player.moveKey && gameMs >= player.nextMove) {
    movePlayer(player.moveKey);
    player.nextMove = gameMs + (player.gliding ? playerShip().moveMs : MOVE_REPEAT_DELAY);
    player.gliding = true;
  }
}

// The ship is hit (unless shielded): one life less, 2 s shield, explosion, shake and sound.
// cause says what hit it, for the Serial log, e.g. F("shot").
void playerHit(const __FlashStringHelper* cause) {
  if (playerShielded() || player.lives == 0) return;
  player.lives--;
  player.shieldUntil = gameMs + HIT_SHIELD_MS;
  player.firing = false;
  explode(player.x + 1, player.y + 1, 12, 14);
  shake(12);
  soundSfx(SFX(SFX_PLAYER_HIT), 6);
  Serial.print(F("[VR] hit by "));
  Serial.print(cause);
  Serial.print(F(", lives "));
  Serial.println(player.lives);
}

// Draws the ship and its engine flame; it blinks while shielded, and its nose blinks
// during auto-fire.
void drawPlayer(uint16_t frameNo) {
  if (player.lives == 0) return;
  if (gameMs < player.shieldUntil && (frameNo & 8)) return;    // blinks while shielded
  const uint16_t* sprite = playerShip().sprite;
  drawBitmap(player.x, player.y, sprite, 3, 3, true);
  if (player.firing && player.burst >= 2 && (frameNo & 8))      // auto-fire: the nose blinks
    drawBitmap(player.x, player.y, sprite, 3, 1, false);
  if (frameNo & 1) setPixel(player.x + 1, player.y + 3, true);  // engine flame
}
