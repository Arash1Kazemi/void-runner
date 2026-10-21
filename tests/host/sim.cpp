/*
 * sim.cpp -- runs the real VOID RUNNER code on a PC, driven by a test script.
 * No Wokwi minutes needed: a whole game takes a few seconds.
 *
 * Build (from the project folder, MSYS2 g++):
 *   g++ -std=c++17 -O2 -Itests/host -o build/sim.exe tests/host/sim.cpp
 * Run:
 *   build/sim.exe tests/host/<script>.txt > log.txt
 *
 * Script commands (one per line, # = comment):
 *   run <ms>                 let the simulated time run
 *   down <key> / up <key>    hold / release a keypad key (W, A, R, ...)
 *   tap <key> [ms]           press a key for ms (default 60), then wait 60 ms
 *   send <text>              type text into Serial (test commands like !d, !s)
 *   until <text> [ms]        run until the Serial output shows text (default limit 60 s)
 *   dump                     print the screen (same as send !d)
 *   record <file> <ms>       from now on, write the screen to <file> every <ms>
 *   stop                     stop recording
 *   sound <file>             log every buzzer frequency change to <file>
 *   reset                    power off and on again (the EEPROM keeps its data)
 *   seed <n>                 start the random generator at n (another game)
 *   times                    show the time (ms) in front of every Serial line
 * A failed "until" prints [SIM] FAIL and ends with exit code 1.
 */
#include "Arduino.h"
#include "../../void_runner/void_runner.ino"
#include <fstream>
#include <iostream>
#include <sstream>

uint8_t pinModes[70], pinLevels[70];
uint64_t simMicros = 0;
long randomState = 1;
std::string serialInput;

// ---- Serial output: printed, and kept so "until" can search it
std::string serialOutput;
size_t searchFrom = 0;                   // "until" only looks at output after this point

bool showTimes = false;                  // "times": put the time in front of every line
bool lineStart = true;

void serialWrite(const std::string& text) {
  for (char c : text) {
    if (showTimes && lineStart) std::cout << '[' << millis() << "] ";
    std::cout << c;
    lineStart = c == '\n';
  }
  serialOutput += text;
}

// ---- keypad: a column reads LOW when its key is held and its row is driven LOW
bool keyDown[KEY_ROWS][KEY_COLS];

int digitalRead(uint8_t pin) {
  for (uint8_t c = 0; c < KEY_COLS; c++) {
    if (KEY_COL_PINS[c] != pin) continue;
    for (uint8_t r = 0; r < KEY_ROWS; r++) {
      uint8_t rowPin = KEY_ROW_PINS[r];
      if (keyDown[r][c] && pinModes[rowPin] == OUTPUT && pinLevels[rowPin] == LOW) return LOW;
    }
    return HIGH;
  }
  return HIGH;
}

bool setKey(char key, bool down) {
  for (uint8_t r = 0; r < KEY_ROWS; r++)
    for (uint8_t c = 0; c < KEY_COLS; c++)
      if (KEY_NAMES[r][c] == key) {
        keyDown[r][c] = down;
        return true;
      }
  return false;
}

// ---- screen recording, in the same text format as the !d dump
std::ofstream recordFile;
uint32_t recordEveryMs = 0, recordNextMs = 0;

void recordScreen() {
  recordFile << "[DUMP] begin t=" << millis() << "\n";
  for (uint8_t i = 0; i < TOTAL_ROWS; i++) {
    uint8_t r = (i < STATUS_ROWS) ? SCREEN_H + i : i - STATUS_ROWS;
    if (i == STATUS_ROWS) recordFile << "----------------\n";
    for (uint8_t x = 0; x < SCREEN_W; x++) recordFile << ((shown[r] >> x) & 1 ? '#' : '.');
    recordFile << "\n";
  }
  recordFile << "[DUMP] end\n";
}

// ---- sound log: the frequency each buzzer timer plays (0 = silent)
std::ofstream soundFile;
uint16_t lastHz[BUZZER_COUNT];

void logSound() {
  for (uint8_t v = 0; v < BUZZER_COUNT; v++) {
    const TimerRegs& t = VOICE_TIMER[v];
    bool on = (*t.tccrB & 7) && (*t.tccrA & _BV(COM1A0));
    uint16_t hz = on ? F_CPU / (2UL * 256 * (*t.ocrA + 1)) : 0;
    if (hz != lastHz[v]) {
      lastHz[v] = hz;
      soundFile << millis() << " " << (int)v << " " << hz << "\n";
    }
  }
}

// ---- one millisecond of the Arduino's life
void tick() {
  simMicros += 1000;
  loop();
  if (recordFile.is_open() && millis() >= recordNextMs) {
    recordScreen();
    recordNextMs = millis() + recordEveryMs;
  }
  if (soundFile.is_open()) logSound();
}

void runMs(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++) tick();
}

bool runUntil(const std::string& text, uint32_t limitMs) {
  for (uint32_t i = 0; i <= limitMs; i++) {
    size_t at = serialOutput.find(text, searchFrom);
    if (at != std::string::npos) {
      searchFrom = at + text.size();
      return true;
    }
    tick();
  }
  return false;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: sim <script>\n";
    return 2;
  }
  std::ifstream script(argv[1]);
  if (!script) {
    std::cerr << "cannot open " << argv[1] << "\n";
    return 2;
  }
  memset(eepromData, 0xFF, sizeof(eepromData));   // a new chip: EEPROM erased
  setup();
  std::string line;
  int lineNo = 0;
  while (std::getline(script, line)) {
    lineNo++;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream in(line);
    std::string cmd;
    if (!(in >> cmd) || cmd[0] == '#') continue;
    std::string arg;
    std::getline(in >> std::ws, arg);
    if (cmd == "run") {
      runMs(std::stoul(arg));
    } else if (cmd == "down" || cmd == "up") {
      if (!setKey(arg[0], cmd == "down")) std::cout << "[SIM] unknown key " << arg << "\n";
    } else if (cmd == "tap") {
      std::istringstream a(arg);
      char key;
      uint32_t ms = 60;
      a >> key >> ms;
      setKey(key, true);
      runMs(ms);
      setKey(key, false);
      runMs(60);
    } else if (cmd == "send") {
      serialInput += arg;
      runMs(5);
    } else if (cmd == "dump") {
      serialInput += "!d";
      runMs(5);
    } else if (cmd == "until") {
      uint32_t limit = 60000;
      size_t q1 = arg.find('"'), q2 = arg.rfind('"');
      std::string text = arg;
      if (q1 != std::string::npos && q2 > q1) {
        text = arg.substr(q1 + 1, q2 - q1 - 1);
        std::istringstream rest(arg.substr(q2 + 1));
        rest >> limit;
      }
      if (!runUntil(text, limit)) {
        std::cout << "\n[SIM] FAIL line " << lineNo << ": no \"" << text << "\" within "
                  << limit << " ms (t=" << millis() << ")\n";
        return 1;
      }
    } else if (cmd == "record") {
      size_t lastSpace = arg.rfind(' ');           // the file name may contain spaces
      recordEveryMs = std::stoul(arg.substr(lastSpace + 1));
      recordFile.open(arg.substr(0, lastSpace));
      recordNextMs = millis();
    } else if (cmd == "stop") {
      recordFile.close();
    } else if (cmd == "sound") {
      soundFile.open(arg);
    } else if (cmd == "times") {
      showTimes = true;
    } else if (cmd == "seed") {
      randomState = std::stol(arg);
    } else if (cmd == "reset") {
      memset((void*)keyDown, 0, sizeof(keyDown));
      setup();
    } else {
      std::cout << "[SIM] unknown command on line " << lineNo << ": " << line << "\n";
      return 2;
    }
  }
  std::cout << "\n[SIM] PASS t=" << millis() << "\n";
  return 0;
}
