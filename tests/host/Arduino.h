/*
 * Arduino.h (PC version) -- lets the real VOID RUNNER code compile and run on a PC for
 * automated tests (tests/host/sim.cpp). It is NOT used for the Arduino build.
 *
 * Everything the sketch takes from Arduino is imitated just well enough:
 *   - flash access (PROGMEM, pgm_read_..., PSTR, F) reads normal memory
 *   - timer and port registers are plain variables; the harness reads the buzzer
 *     timers back to log the notes
 *   - the EEPROM registers drive a 4 KB array
 *   - millis()/micros() come from the harness's simulated clock
 *   - digitalRead() of a keypad column answers from the keys the test script holds
 *   - random() is avr-libc's generator, so runs are repeatable
 */
#pragma once
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

// ---- flash memory = normal memory on a PC
#define PROGMEM
#define PSTR(s) (s)
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_ptr(p) (*(const void* const*)(p))
#define memcpy_P memcpy
#define strchr_P strchr
#define strlen_P strlen

// ---- pins and basic helpers
const uint8_t LOW = 0, HIGH = 1;
const uint8_t INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2;
const uint8_t A0 = 54, A1 = 55, A2 = 56, A3 = 57, A4 = 58, A5 = 59, A6 = 60, A7 = 61,
              A8 = 62, A9 = 63, A10 = 64, A11 = 65, A12 = 66, A13 = 67, A14 = 68, A15 = 69;
#define _BV(bit) (1 << (bit))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
inline bool isAlpha(int c) { return isalpha(c) != 0; }
inline void noInterrupts() {}
inline void interrupts() {}
inline void delayMicroseconds(unsigned) {}
#define ISR(vector) void vector()
const unsigned long F_CPU = 16000000UL;

extern uint8_t pinModes[70], pinLevels[70];
inline void pinMode(uint8_t pin, uint8_t mode) { pinModes[pin] = mode; }
inline void digitalWrite(uint8_t pin, uint8_t level) { pinLevels[pin] = level; }
int digitalRead(uint8_t pin);            // in sim.cpp (keypad)
inline int analogRead(uint8_t) { return 0; }

// ---- simulated clock
extern uint64_t simMicros;
inline unsigned long millis() { return (unsigned long)(simMicros / 1000); }
inline unsigned long micros() { return (unsigned long)simMicros; }

// ---- random(): the same generator as avr-libc
extern long randomState;
inline long avrRandom() {
  long x = randomState ? randomState : 123459876L;
  long hi = x / 127773L, lo = x % 127773L;
  x = 16807L * lo - 2836L * hi;
  if (x < 0) x += 0x7fffffffL;
  randomState = x;
  return x;
}
inline long random(long howBig) { return howBig ? avrRandom() % howBig : 0; }
inline long random(long howSmall, long howBig) {
  return howSmall >= howBig ? howSmall : howSmall + random(howBig - howSmall);
}
inline void randomSeed(unsigned long seed) { if (seed) randomState = (long)seed; }

// ---- registers (timers 1-5, ports, EEPROM)
#define REG8(name) inline volatile uint8_t name = 0
#define REG16(name) inline volatile uint16_t name = 0
REG8(TCCR1A); REG8(TCCR1B); REG16(OCR1A); REG16(TCNT1);
REG8(TCCR2A); REG8(TCCR2B); REG8(OCR2A); REG8(OCR2B); REG8(TIMSK2);
REG8(TCCR3A); REG8(TCCR3B); REG16(OCR3A); REG16(TCNT3);
REG8(TCCR4A); REG8(TCCR4B); REG16(OCR4A); REG16(TCNT4);
REG8(TCCR5A); REG8(TCCR5B); REG16(OCR5A); REG16(TCNT5);
REG8(PORTF); REG8(PORTK);
const uint8_t WGM12 = 3, CS12 = 2, COM1A0 = 6;
const uint8_t WGM21 = 1, CS22 = 2, CS21 = 1, OCIE2A = 1, OCIE2B = 2;

inline uint8_t eepromData[4096];
inline volatile uint16_t EEAR = 0;
inline volatile uint8_t EEDR = 0;
const uint8_t EERE = 0, EEPE = 1, EEMPE = 2;
struct EecrRegister {                    // setting EERE reads, setting EEPE writes
  operator uint8_t() const { return 0; } // never busy
  EecrRegister& operator|=(int bits) {
    if (bits & _BV(EERE)) EEDR = eepromData[EEAR % 4096];
    if (bits & _BV(EEPE)) eepromData[EEAR % 4096] = EEDR;
    return *this;
  }
};
inline EecrRegister EECR;

// ---- Serial: prints to the harness, reads the commands of the test script
void serialWrite(const std::string& text);   // in sim.cpp
extern std::string serialInput;
struct SerialPort {
  void begin(unsigned long) {}
  int available() { return (int)serialInput.size(); }
  int read() {
    if (serialInput.empty()) return -1;
    char c = serialInput[0];
    serialInput.erase(0, 1);
    return (unsigned char)c;
  }
  void print(const __FlashStringHelper* s) { serialWrite(reinterpret_cast<const char*>(s)); }
  void print(const char* s) { serialWrite(s); }
  void print(char c) { serialWrite(std::string(1, c)); }
  void print(int n) { serialWrite(std::to_string(n)); }
  void print(unsigned n) { serialWrite(std::to_string(n)); }
  void print(long n) { serialWrite(std::to_string(n)); }
  void print(unsigned long n) { serialWrite(std::to_string(n)); }
  void print(unsigned char n) { serialWrite(std::to_string(n)); }
  template <typename T> void println(T value) { print(value); serialWrite("\n"); }
  void println() { serialWrite("\n"); }
};
inline SerialPort Serial;
