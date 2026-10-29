/*
 * VOID RUNNER -- a vertical space shooter on an Arduino Mega.
 *
 * Hardware (see diagram.json; pins.h is generated with it by tools/gen_circuit.py):
 *   - 16 x 24 game screen + 3 status bars, 432 single LEDs in one row/column matrix
 *   - 3 x 3 keypad of single push buttons (W A S D move, R fire / OK, Q back,
 *     P pause, M sound on/off, B brightness)
 *   - 4 buzzers, one hardware timer each: lead, bass, effects, rhythm guitar
 *
 * Code tabs:
 *   config.h   settings              display.h  LED screen, drawing, text
 *   input.h    keypad                sound.h    buzzers, music engine, effects
 *   tunes.h    songs and effects     storage.h  settings + high score in EEPROM
 *   art.h      pixel art             font.h     two pixel fonts (generated)
 *   logo.h     start-up logo         title.h    title screen
 *   menu.h     main menu             screens.h  hangar, options, best, pause, end screens
 *   world.h    everything on the game screen (player, shots, enemies, boss ...)
 *   player.h   the player's ship     enemies.h  enemy and rock behaviour
 *   boss.h     the three bosses      levels.h   the three phases (spawn lists)
 *   game.h     one game: phases, collisions, status bars
 *   autopilot.h  the ship flies itself (the title screen's demo, automatic tests)
 *
 * This file: the screen state machine, the Serial test commands and the main loop.
 */
#include "pins.h"
#include "config.h"
#include "display.h"
#include "input.h"
#include "sound.h"
#include "storage.h"
#include "logo.h"
#include "title.h"
#include "menu.h"
#include "game.h"
#include "screens.h"

Screen screen = SCR_LOGO;
uint32_t screenStart = 0;                // when the current screen started (ms)
uint16_t frameNo = 0;                    // counts pictures, for blinking
uint32_t frameWorkUs = 0, frameWorkMaxUs = 0;   // CPU time of one picture (test info)
const uint32_t DEMO_AFTER_MS = 20000;    // title screen untouched this long: demo game

const char SCREEN_NAMES[][9] PROGMEM = {
  "logo", "title", "menu", "hangar", "options", "best",
  "game", "pause", "gameover", "victory", "ledtest"
};

// Switches to another screen: remembers when it opened, logs it, and does what that
// screen needs first (start the logo music, stop sounds, save the score, ...).
void goTo(Screen next) {
  Screen from = screen;
  screen = next;
  screenStart = millis();
  Serial.print(F("[VR] screen "));
  Serial.println((const __FlashStringHelper*)SCREEN_NAMES[next]);
  switch (next) {
    case SCR_LOGO:     logoBegin(); break;
    case SCR_TITLE:
      soundStop();
      if (demoMode) {                    // the demo ends
        demoMode = false;
        autopilot = false;
      }
      break;
    case SCR_MENU:
      if (from == SCR_PAUSE || from == SCR_GAMEOVER || from == SCR_VICTORY) soundStop();
      break;
    case SCR_HANGAR:   hangarBegin(); break;
    case SCR_PAUSE:    soundStop(); break;
    case SCR_GAME:
      if (from == SCR_PAUSE) playStageMusic();   // a new game is started by gameStart()
      break;
    case SCR_GAMEOVER:
      recordScore(game.phase + 1);
      soundPlaySong(SONG_GAMEOVER, 150, false);
      break;
    case SCR_VICTORY:
      recordScore(4);
      nextFirework = 0;
      soundPlaySong(SONG_VICTORY, 100, false);
      Serial.print(F("[VR] victory score="));
      Serial.println(player.score);
      break;
    case SCR_LEDTEST:  soundSfx(SFX(SFX_TEST), 5); break;
    default: break;
  }
}

// Keys that work on every screen: B = brightness, M = sound on/off (both saved).
void globalKeys() {
  if (keyPressed(KEY_BRIGHTNESS)) {
    settings.brightness = (settings.brightness + 1) % sizeof(BRIGHTNESS_LEVELS);
    applyBrightness();
    settingsSave();
    soundBeep(1200, 40);
    Serial.print(F("[VR] brightness "));
    Serial.println(BRIGHTNESS_LEVELS[settings.brightness]);
  }
  if (keyPressed(KEY_MUTE)) {
    settings.muted = !settings.muted;
    soundSetMuted(settings.muted);
    settingsSave();
    soundBeep(1200, 40);
    Serial.println(settings.muted ? F("[VR] sound off") : F("[VR] sound on"));
  }
}

// Test commands over Serial start with '!', so letters typed by mistake into the Wokwi
// terminal do nothing (they usually mean the keyboard is talking to the terminal
// instead of the buttons):
//   !d  print the screen as text      !s  print the game state
//   !1 !2 !3  start a game at phase 1, 2 or 3
//   !b  skip to the boss              !w  the boss loses all its health
//   !i  shield on/off (no hits)       !x  autopilot on/off
//   !f  fast mode on/off (8 game steps per picture)
//   !k  lose the last life
void serialCommands() {
  static bool command = false;
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '!') {
      command = true;
      continue;
    }
    if (!command) {
      if (isAlpha(c))
        Serial.println(F("[VR] keys are going to the Serial terminal: click an empty spot of the circuit"));
      continue;
    }
    command = false;
    bool inGame = screen == SCR_GAME;
    switch (c) {
      case 'd': displayDump(); break;
      case 's':
        gamePrintState();
        Serial.print(F("[VR] frame work us="));
        Serial.print(frameWorkUs);
        Serial.print(F(" max="));
        Serial.println(frameWorkMaxUs);
        frameWorkMaxUs = 0;
        break;
      case '1': case '2': case '3':
        gameStart(c - '1');
        goTo(SCR_GAME);
        break;
      case 'b': if (inGame) gameSkipToBoss(); break;
      case 'w': if (inGame && game.stage == GS_BOSS) boss.hp = 0; break;
      case 'i':
        testShield = !testShield;
        Serial.println(testShield ? F("[VR] shield on") : F("[VR] shield off"));
        break;
      case 'x':
        autopilot = !autopilot;
        Serial.println(autopilot ? F("[VR] autopilot on") : F("[VR] autopilot off"));
        break;
      case 'f':
        gameSpeed = gameSpeed == 1 ? 8 : 1;
        Serial.println(gameSpeed > 1 ? F("[VR] fast on") : F("[VR] fast off"));
        break;
      case 'k':
        if (inGame && player.lives) {
          testShield = false;
          player.shieldUntil = 0;
          player.lives = 1;
          playerHit(F("test"));
        }
        break;
    }
  }
}

// One picture of the current screen: handles its keys, draws it and switches to another
// screen when needed (the screen state machine).
void drawScreen(uint32_t now) {
  uint32_t t = now - screenStart;
  Screen next = screen;
  switch (screen) {
    case SCR_LOGO: {
      uint16_t skipKeys = keysPressed & ~(keyBit(KEY_BRIGHTNESS) | keyBit(KEY_MUTE));
      if (logoUpdate(frameNo) || skipKeys) next = SCR_TITLE;   // any other key skips the logo
      break;
    }
    case SCR_TITLE:
      titleDraw(t, frameNo);
      if (keyPressed(KEY_FIRE)) {
        soundBeep(2000, 60);
        next = SCR_MENU;
      } else if (t >= DEMO_AFTER_MS) {   // nobody is playing: show a demo game
        demoMode = true;
        autopilot = true;
        gameStart(0);
        next = SCR_GAME;
        Serial.println(F("[VR] demo"));
      }
      break;
    case SCR_MENU: {
      int8_t choice = menuUpdate();
      if (choice == MENU_BACK) next = SCR_TITLE;
      else if (choice == MENU_ITEM_PLAY) {
        gameStart(0);
        next = SCR_GAME;
      }
      else if (choice == MENU_ITEM_SHIP) next = SCR_HANGAR;
      else if (choice == MENU_ITEM_OPTS) next = SCR_OPTIONS;
      else if (choice == MENU_ITEM_BEST) next = SCR_BEST;
      else menuDraw();
      break;
    }
    case SCR_HANGAR:   next = hangarFrame(now, frameNo); break;
    case SCR_OPTIONS:  next = optionsFrame(frameNo); break;
    case SCR_BEST:     next = bestFrame(frameNo); break;
    case SCR_GAME: {
      if (demoMode) {                    // demo: any key (but B and M) goes back to the title
        uint16_t anyKey = keysPressed & ~(keyBit(KEY_BRIGHTNESS) | keyBit(KEY_MUTE));
        if (anyKey || gameFrame(frameNo) != GAME_ON) next = SCR_TITLE;
        break;
      }
      if (keyPressed(KEY_PAUSE) || keyPressed(KEY_BACK)) {
        next = SCR_PAUSE;
        break;
      }
      GameResult result = gameFrame(frameNo);
      if (result == GAME_OVER) next = SCR_GAMEOVER;
      if (result == GAME_WON) next = SCR_VICTORY;
      break;
    }
    case SCR_PAUSE:
      pauseDraw(t, frameNo);
      if (keyPressed(KEY_PAUSE) || keyPressed(KEY_FIRE)) next = SCR_GAME;
      if (keyPressed(KEY_BACK)) {
        recordScore(game.phase + 1);
        next = SCR_MENU;
      }
      break;
    case SCR_GAMEOVER: next = gameOverFrame(t, frameNo); break;
    case SCR_VICTORY:  next = victoryFrame(t); break;
    case SCR_LEDTEST:  next = ledTestFrame(t); break;
  }
  if (next != screen) goTo(next);
}

// Runs once at power-on: Serial, LED screen, keypad, buzzers and the saved settings,
// then the start-up logo.
void setup() {
  Serial.begin(115200);
  displayBegin();
  inputBegin();
  soundBegin();
  settingsLoad();
  applyBrightness();
  soundSetMuted(settings.muted);
  randomSeed(analogRead(A15) ^ micros());
  Serial.println(F("[VR] VOID RUNNER"));
  Serial.print(F("[VR] saved: best="));
  Serial.print(settings.best);
  Serial.print(F(" ship="));
  Serial.print(settings.ship);
  Serial.print(F(" difficulty="));
  Serial.println(settings.difficulty);
  Serial.println(F("[VR] ready"));
  goTo(SCR_LOGO);
}

// Runs over and over: reads the keypad, keeps the music going, reads test commands, and
// every 20 ms draws a new picture.
void loop() {
  uint32_t now = millis();
  inputUpdate(now);
  soundUpdate(now);
  serialCommands();
  static uint32_t lastFrame = 0;
  if (now - lastFrame >= FRAME_MS) {     // one new picture every 20 ms
    lastFrame = now;
    frameNo++;
    uint32_t workStart = micros();
    globalKeys();
    drawScreen(now);
    present();
    inputEndFrame();
    frameWorkUs = micros() - workStart;
    if (frameWorkUs > frameWorkMaxUs) frameWorkMaxUs = frameWorkUs;
  }
}
