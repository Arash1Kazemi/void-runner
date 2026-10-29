/*
 * game.h -- one game, from PLAY to game over or victory.
 *
 * Each of the three phases runs through these stages:
 *   INTRO      the phase number and name as pages, a short fanfare
 *   WAVES      enemies and rocks arrive from the phase's spawn list (levels.h)
 *   WARNING    "WARN" and a blinking warning sign, a siren, the red bar charges up
 *   BOSS       boss fight with its own music; the red bar shows the boss's health
 *   BOSS_DOWN  the boss blows up, the screen shakes
 *   CLEAR      bonus points and one extra life, then the next phase (after the last
 *              boss the game goes straight to the victory screen)
 * and DEAD when the last life is lost.
 *
 * Status bars: red = phase progress / boss health, yellow = ammo (blinks when low),
 * green = lives (one block of three LEDs per life).
 * The game runs in steps of 20 ms of game time (gameMs); a picture is drawn after each
 * step. Pause simply stops calling it. Test hook !f runs several steps per picture.
 */
#pragma once
#include "world.h"
#include "player.h"
#include "autopilot.h"
#include "enemies.h"
#include "boss.h"
#include "levels.h"

enum GameStage : uint8_t { GS_INTRO, GS_WAVES, GS_WARNING, GS_BOSS, GS_BOSS_DOWN, GS_CLEAR, GS_DEAD };
enum GameResult : uint8_t { GAME_ON, GAME_OVER, GAME_WON };

struct Game {
  uint8_t phase, stage, spawnIndex;
  uint32_t stageStart;                   // gameMs
  uint32_t nextSpawn, nextAmmoHelp, nextBoom;
  uint16_t bonus;
  bool lifeUp;                           // the phase clear gave an extra life
};
Game game;
uint8_t gameSpeed = 1;                   // steps per picture (test hook !f: fast)
bool demoMode = false;                   // the title screen's demo: the autopilot plays

const uint16_t INTRO_MS = 3 * PAGE_MS, WARNING_MS = 3000, BOSS_DOWN_MS = 2500;
const uint16_t CLEAR_MS = 3200, DEAD_MS = 2500;
const uint16_t PICKUP_STEP_MS = 250;
const uint8_t DROP_CHANCE = 12;          // % chance that a destroyed enemy drops ammo

// ms of game time since the current stage began.
uint32_t stageTime() {
  return gameMs - game.stageStart;
}

// Switches the phase to another stage and restarts the stage clock.
void setStage(uint8_t stage) {
  game.stage = stage;
  game.stageStart = gameMs;
}

// The music that belongs to the current stage (also used after a pause).
void playStageMusic() {
  const PhaseInfo& ph = PHASES[game.phase];
  if (game.stage == GS_WAVES) soundPlaySong(ph.song, ph.songTickMs, true);
  else if (game.stage == GS_BOSS) soundPlaySong(SONG_BOSS, SONG_BOSS_TICK_MS, true);
}

// Starts a phase: empties the screen, puts the ship back with full ammo, plays the
// fanfare and begins the INTRO stage.
void phaseBegin(uint8_t phase) {
  game.phase = phase;
  game.spawnIndex = 0;
  game.nextAmmoHelp = 0;
  worldClear();
  playerStart(false);
  setStage(GS_INTRO);
  soundStop();
  soundPlaySong(SONG_STING, 100, false);
  Serial.print(F("[VR] phase "));
  Serial.println(phase + 1);
}

// Starts a new game: game clock at 0, the chosen ship, lives and score 0, then the phase
// (normally phase 1; the test hooks !2 and !3 start later).
void gameStart(uint8_t phase) {
  gameMs = 0;
  playerStart(true);
  phaseBegin(phase);
}

// ================================================================= hits and pickups
// Removes an enemy or rock with a boom; a big rock breaks into two small ones. Destroyed
// by the player (byPlayer), it also gives points and maybe drops a pickup.
void destroyEnemy(Enemy& e, bool byPlayer) {
  const EnemyInfo& info = infoOf(e);
  int8_t x = e.x, y = e.y;
  uint8_t type = e.type;
  e.alive = false;                       // it just vanishes: flying debris only for the
  soundSfx(SFX(SFX_BOOM), 3);            // boss and the player's ship (easier to read)
  if (type == EN_ROCK_B) {               // a big rock breaks into two small ones
    const int8_t maxX = SCREEN_W - ENEMY_INFO[EN_ROCK_S].w;
    Enemy* a = spawnEnemy(EN_ROCK_S, constrain(x - 1, 0, maxX), y);
    Enemy* b = spawnEnemy(EN_ROCK_S, constrain(x + 2, 0, maxX), y);
    if (a) a->dir = -1;
    if (b) b->dir = 1;
  }
  if (!byPlayer) return;
  addScore(info.score);
  if (type == EN_CRUISER && player.lives < MAX_LIVES) spawnPickup(PK_LIFE, x + 1, y);
  else if (random(100) < DROP_CHANCE && !pickupAlive(PK_AMMO)) spawnPickup(PK_AMMO, x, y);
}

// A player bullet hits an enemy: it loses one health point and disappears for a moment,
// or it is destroyed when it had only one left.
void hitEnemy(Enemy& e) {
  if (e.hp > 1) {
    e.hp--;
    e.flash = 6;
    soundSfx(SFX(SFX_HIT), 2);
  } else {
    destroyEnemy(e, true);
  }
}

// True when the point (px, py) lies inside the box at (x, y) of size w x h.
bool pointIn(int8_t px, int8_t py, int8_t x, int8_t y, uint8_t w, uint8_t h) {
  return px >= x && px < x + w && py >= y && py < y + h;
}

// A player bullet against enemies, the boss and mines.
void playerShotHits(Shot& s) {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    Enemy& e = enemies[i];
    if (e.alive && pointIn(s.x, s.y, e.x, e.y, infoOf(e).w, infoOf(e).h)) {
      s.alive = false;
      hitEnemy(e);
      return;
    }
  }
  if (game.stage == GS_BOSS && boss.alive && boss.hp > 0 &&
      pointIn(s.x, s.y, boss.x, boss.y, boss.w, boss.h)) {
    s.alive = false;
    boss.hp--;                           // (no flash: the red bar and the sound show it)
    soundSfx(SFX(SFX_HIT), 2);
    return;
  }
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {   // mines can be shot too
    Shot& m = shots[i];
    if (m.alive && m.kind == SHOT_MINE && m.x == s.x && (m.y == s.y || m.y == s.y + 1)) {
      m.alive = s.alive = false;
      addScore(2);
      soundSfx(SFX(SFX_HIT), 2);
      return;
    }
  }
}

// Moves every bullet and mine one pixel when its time comes (player bullets up, enemy
// bullets down and maybe sideways) and checks what each of them hits.
void shotsStep() {
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {
    Shot& s = shots[i];
    if (!s.alive) continue;
    if (gameMs >= s.next) {
      s.next += s.stepMs;
      s.y += (s.kind == SHOT_PLAYER) ? -1 : 1;
      if (s.dx && ++s.sideCount >= s.sideEvery) {
        s.x += s.dx;
        s.sideCount = 0;
      }
      if (s.y < 0 || s.y >= SCREEN_H || s.x < 0 || s.x >= SCREEN_W) {
        s.alive = false;
        continue;
      }
    }
    if (s.kind == SHOT_PLAYER) playerShotHits(s);
    else if (shipPixel(s.x, s.y) && !playerShielded() && player.lives) {
      s.alive = false;
      playerHit(s.kind == SHOT_MINE ? F("mine") : F("shot"));
    }
  }
}

// True when a lit pixel of the enemy or rock lies on a lit pixel of the ship, so the empty
// corners of a hollow rock (or between a drone's wings) don't count as a crash.
bool enemyTouchesShip(const Enemy& e) {
  const EnemyInfo& info = infoOf(e);
  if (!boxesTouch(e.x, e.y, info.w, info.h, player.x, player.y, 3, 3)) return false;
  for (uint8_t r = 0; r < info.h; r++) {
    uint16_t bits = pgm_read_word(info.sprite + r);
    for (uint8_t c = 0; c < info.w; c++)
      if ((bits & (0x8000u >> c)) && shipPixel(e.x + c, e.y + r)) return true;
  }
  return false;
}

// Flying into an enemy, a rock or the boss also costs a life.
void contactsStep() {
  if (playerShielded() || player.lives == 0) return;
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) {
    Enemy& e = enemies[i];
    if (e.alive && enemyTouchesShip(e)) {
      bool rock = e.type >= EN_ROCK_S;
      destroyEnemy(e, false);
      playerHit(rock ? F("rock") : F("enemy"));
      return;
    }
  }
  if (game.stage == GS_BOSS && boss.alive &&
      boxesTouch(boss.x, boss.y, boss.w, boss.h, player.x, player.y, 3, 3))
    playerHit(F("boss"));
}

// The ship caught a pickup: ammo crate = more ammo (up to the ship's maximum), plus =
// one extra life (up to 4). Both give 5 points.
void collect(Pickup& p) {
  p.alive = false;
  addScore(5);
  if (p.type == PK_AMMO) {
    uint8_t room = playerShip().ammoMax - player.ammo, pack = DIFF_PACK_AMMO[settings.difficulty];
    player.ammo += pack < room ? pack : room;
    if (player.ammo > LOW_AMMO) player.lowAmmoWarned = false;
    soundSfx(SFX(SFX_PICKUP), 4);
    Serial.println(F("[VR] pickup ammo"));
  } else {
    if (player.lives < MAX_LIVES) player.lives++;
    soundSfx(SFX(SFX_LIFE), 4);
    Serial.println(F("[VR] pickup life"));
  }
}

// Lets the pickups fall, checks whether the ship catches one, and sends an ammo crate
// when the player is about to run out.
void pickupsStep() {
  for (uint8_t i = 0; i < MAX_PICKUPS; i++) {
    Pickup& p = pickups[i];
    if (!p.alive) continue;
    if (gameMs >= p.next) {
      p.next += PICKUP_STEP_MS;
      if (++p.y >= SCREEN_H) p.alive = false;
    }
    if (p.alive && player.lives && boxesTouch(p.x, p.y, 3, 3, player.x, player.y, 3, 3)) collect(p);
  }
  // Running out of ammo: a crate is sent, so the game can always go on.
  bool fighting = game.stage == GS_WAVES || game.stage == GS_BOSS;
  if (fighting && player.ammo <= LOW_AMMO && !pickupAlive(PK_AMMO) && gameMs >= game.nextAmmoHelp) {
    spawnPickup(PK_AMMO, random(SCREEN_W - 2), -3);
    game.nextAmmoHelp = gameMs + 5000;
  }
}

// Moves the explosion pixels and counts down their life (one game step).
void particlesStep() {
  for (uint8_t i = 0; i < MAX_PARTICLES; i++) {
    Particle& p = particles[i];
    if (!p.life) continue;
    p.x16 += p.vx;
    p.y16 += p.vy;
    p.life--;
  }
}

// ================================================================= phase flow
// Works through the phase's spawn list: when the wait is over, puts the next enemy,
// rock or pickup on the screen. Waits while the screen is full, and at SP_CLEAR until
// it is empty.
void spawnerStep() {
  const PhaseInfo& ph = PHASES[game.phase];
  while (game.spawnIndex < ph.count && gameMs >= game.nextSpawn) {
    Spawn s = spawnAt(game.phase, game.spawnIndex);
    if (s.what == SP_CLEAR && enemiesAlive()) return;       // wait for an empty screen
    if (s.what < SP_AMMO && enemiesAlive() >= ph.maxEnemies) return;   // screen is full
    if (s.what == SP_AMMO) spawnPickup(PK_AMMO, s.x, -3);
    else if (s.what == SP_LIFE) spawnPickup(PK_LIFE, s.x, -3);
    else if (s.what < SP_AMMO) spawnFromTable(s.what, s.x);
    if (++game.spawnIndex < ph.count)
      game.nextSpawn = gameMs + spawnAt(game.phase, game.spawnIndex).wait * 100UL;
  }
}

// The boss has no health left: points, its bullets and helpers vanish, big explosion and
// screen shake; begins the BOSS_DOWN stage.
void bossDown() {
  addScore(BOSS_SCORE[boss.kind]);
  for (uint8_t i = 0; i < MAX_SHOTS; i++)           // its bullets vanish ...
    if (shots[i].kind != SHOT_PLAYER) shots[i].alive = false;
  for (uint8_t i = 0; i < MAX_ENEMIES; i++)         // ... and so do its helpers
    enemies[i].alive = false;
  setStage(GS_BOSS_DOWN);
  game.nextBoom = gameMs;
  soundStop();
  soundSfx(SFX(SFX_BIG_BOOM), 5);
  shake(40);
  Serial.println(F("[VR] boss down"));
}

// A phase is won: bonus points (100 x phase), one extra life and the 'clear' tune.
void phaseClear() {
  game.bonus = PHASE_BONUS * (game.phase + 1);
  addScore(game.bonus);
  game.lifeUp = player.lives < MAX_LIVES;
  if (game.lifeUp) player.lives++;
  setStage(GS_CLEAR);
  soundPlaySong(SONG_CLEAR, 90, false);
  Serial.println(F("[VR] phase clear"));
}

// Runs the current stage for one game step and decides when the next one begins
// (INTRO -> WAVES -> WARNING -> BOSS -> BOSS_DOWN -> CLEAR -> next phase).
// Returns GAME_OVER or GAME_WON when the game ends.
GameResult stageStep() {
  const PhaseInfo& ph = PHASES[game.phase];
  switch (game.stage) {
    case GS_INTRO:
      if (stageTime() >= INTRO_MS) {
        setStage(GS_WAVES);
        playStageMusic();
        game.nextSpawn = gameMs + spawnAt(game.phase, 0).wait * 100UL;
      }
      break;
    case GS_WAVES:
      spawnerStep();
      if (game.spawnIndex >= ph.count && !enemiesAlive()) {
        setStage(GS_WARNING);
        soundStop();
        soundSfx(SFX(SFX_SIREN), 5);
        Serial.println(F("[VR] warning"));
      }
      break;
    case GS_WARNING:
      if (stageTime() >= WARNING_MS) {
        bossStart(ph.boss);
        setStage(GS_BOSS);
        playStageMusic();
        Serial.println(F("[VR] boss"));
      }
      break;
    case GS_BOSS:
      updateBoss();
      if (boss.hp <= 0) bossDown();
      break;
    case GS_BOSS_DOWN:
      boss.flash = 2;                                 // keeps blinking while it burns
      if (stageTime() < 1500 && gameMs >= game.nextBoom) {
        explode(boss.x + random(boss.w), boss.y + random(boss.h), 6, 12);
        game.nextBoom = gameMs + 120;
      }
      if (boss.alive && stageTime() >= 1500) {        // final blast
        boss.alive = false;
        explode(boss.x + boss.w / 2, boss.y + boss.h / 2, 30, 16);
        shake(20);
        soundSfx(SFX(SFX_BIG_BOOM), 5);
      }
      if (stageTime() >= BOSS_DOWN_MS) {
        if (game.phase == 2) {                        // last boss: bonus, then straight to
          addScore(PHASE_BONUS * 3);                  // the victory screen (no ZONE DONE,
          return GAME_WON;                            // no extra life)
        }
        phaseClear();
      }
      break;
    case GS_CLEAR:
      if (stageTime() >= CLEAR_MS) {
        if (game.phase == 2) return GAME_WON;
        phaseBegin(game.phase + 1);
      }
      break;
    case GS_DEAD:
      if (stageTime() >= DEAD_MS) return GAME_OVER;
      break;
  }
  return GAME_ON;
}

// One game step (20 ms of game time): the ship, enemies, bullets, hits, pickups and
// explosions each move once, then the stage logic runs.
GameResult gameStep() {
  gameMs += FRAME_MS;
  if (autopilot) autopilotStep();
  playerStep();
  updateEnemies();
  shotsStep();
  contactsStep();
  pickupsStep();
  particlesStep();
  if (player.lives == 0 && game.stage != GS_DEAD) {   // the last life is gone
    setStage(GS_DEAD);
    explode(player.x + 1, player.y + 1, 25, 16);
    shake(30);
    soundStop();
    soundSfx(SFX(SFX_BIG_BOOM), 6);
    Serial.print(F("[VR] game over score="));
    Serial.println(player.score);
  }
  return stageStep();
}

// ================================================================= drawing
// Sets the three status bars: red = phase number / wave progress / boss health,
// yellow = ammo (blinks when low), green = one block of 3 LEDs per life.
void drawGameStatus(uint16_t frameNo) {
  uint8_t red = 0;                       // red: phase progress / boss charging / boss health
  if (game.stage == GS_WAVES) red = (uint16_t)game.spawnIndex * 16 / PHASES[game.phase].count;
  if (game.stage == GS_WARNING && (frameNo & 8)) red = stageTime() * 16 / WARNING_MS;
  if (game.stage == GS_BOSS && boss.hp > 0) red = (boss.hp * 16 + boss.maxHp - 1) / boss.maxHp;
  if (game.stage == GS_CLEAR && (frameNo & 8)) red = 16;
  if (game.stage == GS_INTRO) {          // phase number: one block per phase
    uint16_t blocks = 0;
    for (uint8_t i = 0; i <= game.phase; i++) blocks |= 0x7u << (i * 4);
    setStatusBits(0, blocks);
  } else {
    setStatusBar(0, red);
  }
  uint8_t ammoMax = playerShip().ammoMax;   // yellow: ammo, blinks when low
  uint8_t yellow = ((uint16_t)player.ammo * 16 + ammoMax - 1) / ammoMax;
  if (player.ammo <= LOW_AMMO && (frameNo & 8)) yellow = 0;
  setStatusBar(1, yellow);
  uint16_t lives = 0;                    // green: one block of three LEDs per life
  for (uint8_t i = 0; i < player.lives; i++) lives |= 0x7u << (i * 4);
  setStatusBits(2, lives);
}

// The messages on top of the game, as still pages (see display.h):
//   INTRO    "ZONE" + the phase number big, then the phase name, then "GO!"
//   WARNING  "WARN" in the red zone and a blinking warning sign
//   CLEAR    "ZONE DONE", the bonus points, "LIFE UP" (if a life was added)
//   demo     every 9 s: "DEMO", then "ANY KEY"
void drawGameTexts() {
  const PhaseInfo& ph = PHASES[game.phase];
  uint32_t t = stageTime();
  if (game.stage == GS_INTRO) {
    uint8_t page = pageAt(t, 3);
    if (page == 0) {
      clearRows(0, 18);
      drawTextCentered(0, PSTR("ZONE"), false);
      drawChar(5, 7, '1' + game.phase, false, 2);      // 6 x 10 pixels
    } else if (page == 1) {
      drawPage(ph.name1, ph.name2);
    } else {
      drawPage(PSTR("GO!"), NULL);
    }
  } else if (game.stage == GS_WARNING) {
    clearRows(0, 5);
    drawTextCentered(0, PSTR("WARN"), false);
    if ((t / 300) % 2) drawBitmap(2, 7, WARNING_SIGN, 11, 10, true);
  } else if (game.stage == GS_CLEAR) {
    uint8_t page = pageAt(t, 3);
    if (page == 1) {
      clearRows(6, 12);
      drawNumberCentered(7, game.bonus, false);
      drawTextCentered(13, PSTR("PTS"), false);
    } else if (page == 2 && game.lifeUp) {
      drawPage(PSTR("LIFE"), PSTR("UP"));
    } else {
      drawPage(PSTR("ZONE"), PSTR("DONE"));
    }
  } else if (demoMode && gameMs % 9000 < 2 * PAGE_MS) {
    if (pageAt(gameMs % 9000, 2) == 0) drawPage(PSTR("DEMO"), NULL);
    else drawPage(PSTR("ANY"), PSTR("KEY"));
  }
}

// Draws one picture of the game: pickups, enemies, boss, bullets, ship and explosions
// (shifted while the screen shakes), then the texts on top and the status bars.
void drawGame(uint16_t frameNo) {
  if (shakeFrames) {                     // screen shake: everything moves one pixel
    shakeFrames--;
    shakeX = random(-1, 2);
    shakeY = random(-1, 2);
  }
  clearScreen();
  for (uint8_t i = 0; i < MAX_PICKUPS; i++) {
    const Pickup& p = pickups[i];
    if (p.alive && (p.type == PK_AMMO || (frameNo & 8)))   // the life blinks
      drawBitmap(p.x, p.y, p.type == PK_AMMO ? PICKUP_AMMO : PICKUP_LIFE, 3, 3, true);
  }
  drawEnemies();
  drawBoss(frameNo);
  for (uint8_t i = 0; i < MAX_SHOTS; i++) {
    const Shot& s = shots[i];
    if (!s.alive) continue;
    if (s.kind == SHOT_PLAYER) {         // player bullet: a short streak
      setPixel(s.x, s.y, true);
      setPixel(s.x, s.y + 1, true);
    } else {                             // enemy bullet or mine: one steady pixel
      setPixel(s.x, s.y, true);
    }
  }
  drawPlayer(frameNo);
  for (uint8_t i = 0; i < MAX_PARTICLES; i++)
    if (particles[i].life) setPixel(particles[i].x16 >> 4, particles[i].y16 >> 4, true);
  shakeX = shakeY = 0;

  drawGameTexts();
  drawGameStatus(frameNo);
}

// One picture of the game: keys, game steps, drawing.
GameResult gameFrame(uint16_t frameNo) {
  playerKeys();
  GameResult result = GAME_ON;
  for (uint8_t i = 0; i < gameSpeed && result == GAME_ON; i++) result = gameStep();
  drawGame(frameNo);
  return result;
}

// ================================================================= test hooks
// Test hook !b: removes the enemies and ends the spawn list, so the boss comes next.
void gameSkipToBoss() {
  for (uint8_t i = 0; i < MAX_ENEMIES; i++) enemies[i].alive = false;
  if (game.stage > GS_WAVES) return;
  game.spawnIndex = PHASES[game.phase].count;
  if (game.stage == GS_INTRO) setStage(GS_WAVES);
}

// Test hook !s: prints phase, stage, score, lives, ammo, ship position, enemies, boss
// health and game time over Serial.
void gamePrintState() {
  Serial.print(F("[VR] state phase="));
  Serial.print(game.phase + 1);
  Serial.print(F(" stage="));
  Serial.print(game.stage);
  Serial.print(F(" score="));
  Serial.print(player.score);
  Serial.print(F(" lives="));
  Serial.print(player.lives);
  Serial.print(F(" ammo="));
  Serial.print(player.ammo);
  Serial.print(F(" x="));
  Serial.print(player.x);
  Serial.print(F(" y="));
  Serial.print(player.y);
  Serial.print(F(" enemies="));
  Serial.print(enemiesAlive());
  Serial.print(F(" boss="));
  Serial.print(boss.alive ? boss.hp : 0);
  Serial.print(F(" t="));
  Serial.println(gameMs);
}
