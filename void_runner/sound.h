/*
 * sound.h -- three buzzers = three voices.
 *
 * Each buzzer pin is the "A" output of its own 16-bit timer (pin 5 = Timer3, pin 6 =
 * Timer4, pin 11 = Timer1). In CTC mode with "toggle on compare match" the timer flips
 * the pin by itself: a note costs no CPU time, and three notes can sound at once
 * (Arduino's tone() can only play one). The timers run at 16 MHz / 256: notes up to
 * about C7 stay within 1 % of their pitch, and Wokwi simulates this slow clock far
 * faster than /8 (measured in Stage 0).
 *
 * Music is written as one list of notes per voice (MIDI note numbers, 60 = middle C)
 * with lengths in "ticks". Effects (glides, beeps) take over a voice for a while.
 */
#pragma once
#include "pins.h"

struct TimerRegs {
  volatile uint8_t* tccrA;
  volatile uint8_t* tccrB;
  volatile uint16_t* ocrA;
  volatile uint16_t* tcnt;
};
const TimerRegs VOICE_TIMER[BUZZER_COUNT] = {
  { &TCCR3A, &TCCR3B, &OCR3A, &TCNT3 },  // pin 5  = OC3A
  { &TCCR4A, &TCCR4B, &OCR4A, &TCNT4 },  // pin 6  = OC4A
  { &TCCR1A, &TCCR1B, &OCR1A, &TCNT1 },  // pin 11 = OC1A
};

struct Note { uint8_t pitch; uint8_t ticks; };   // pitch 0 = rest
struct Track { const Note* notes; uint8_t count; };

struct Voice {
  const Note* notes;                     // track in flash (NULL = no music on this voice)
  uint8_t count, index;
  uint32_t noteStart;
  uint16_t noteMs;
  uint8_t pitch;
  bool effect;                           // an effect has taken over the voice
  uint16_t effectHz;
  uint32_t effectEnd;                    // 0 = until released
  uint16_t playingHz;                    // what the buzzer plays right now
};
Voice voices[BUZZER_COUNT];
uint8_t songTickMs = 50;
uint32_t songStart = 0;
bool songLoops = false;
bool soundMuted = false;
const uint8_t NOTE_GAP_MS = 12;          // short silence at the end of each note

// ================================================================= hardware
// Makes buzzer v play hz (0 = silent) by setting up its timer; does nothing if it already
// plays that frequency.
void buzzerTone(uint8_t v, uint16_t hz) {
  if (soundMuted) hz = 0;
  if (hz == voices[v].playingHz) return;
  voices[v].playingHz = hz;
  const TimerRegs& t = VOICE_TIMER[v];
  *t.tccrB = 0;                          // stop the timer while changing it
  if (hz == 0) {
    *t.tccrA = 0;                        // disconnect the pin from the timer
    digitalWrite(BUZZER_PINS[v], LOW);
    return;
  }
  // Mode bits first: in Arduino's default PWM mode a new OCRnA value would wait for the
  // end of a PWM cycle that a stopped timer never reaches, and the note would be lost.
  *t.tccrA = _BV(COM1A0);                // toggle pin on match (same bit in timers 1, 3, 4)
  *t.tcnt = 0;
  *t.ocrA = F_CPU / (2UL * 256 * hz) - 1;  // f = 16 MHz / (2 * 256 * (OCRnA + 1))
  *t.tccrB = _BV(WGM12) | _BV(CS12);     // CTC mode, prescaler 256: start
}

// Stops the timers Arduino starts by itself and makes the buzzer pins outputs.
void soundBegin() {
  // Arduino starts timers 1, 3, 4 and 5 even when nothing uses them, and Wokwi spends
  // time on every timer count: stop them. A buzzer's timer restarts when it plays.
  // Clearing the A registers too leaves Arduino's PWM mode (see buzzerTone).
  TCCR1B = 0; TCCR1A = 0;
  TCCR3B = 0; TCCR3A = 0;
  TCCR4B = 0; TCCR4A = 0;
  TCCR5B = 0; TCCR5A = 0;
  for (uint8_t v = 0; v < BUZZER_COUNT; v++) pinMode(BUZZER_PINS[v], OUTPUT);
}

// MIDI note number -> frequency: one octave of exact values, halved per octave down.
uint16_t noteHz(uint8_t pitch) {
  static const uint16_t OCTAVE8[12] PROGMEM = {4186, 4435, 4699, 4978, 5274, 5588,
                                               5920, 6272, 6645, 7040, 7459, 7902};
  return pgm_read_word(&OCTAVE8[pitch % 12]) >> (9 - pitch / 12);
}

// ================================================================= music
// Copies the voice's current note from flash and works out its length in ms.
void loadNote(Voice& v) {
  Note n;
  memcpy_P(&n, &v.notes[v.index], sizeof(Note));
  v.pitch = n.pitch;
  v.noteMs = (uint16_t)n.ticks * songTickMs;
}

// Starts a song: one track per buzzer (count 0 = that buzzer stays silent), tickMs =
// length of one tick, loop = start again at the end.
void soundPlaySong(const Track* tracks, uint8_t tickMs, bool loop) {
  uint32_t now = millis();
  songTickMs = tickMs;
  songStart = now;
  songLoops = loop;
  for (uint8_t i = 0; i < BUZZER_COUNT; i++) {
    Voice& v = voices[i];
    v.notes = tracks[i].count ? tracks[i].notes : NULL;
    v.count = tracks[i].count;
    v.index = 0;
    v.noteStart = now;
    if (v.notes) loadNote(v);
  }
}

// ms since the song started; the logo uses it so its pictures land on the notes.
uint32_t songTime() {
  return millis() - songStart;
}

// ================================================================= sound effects (voice 3)
// An effect is a short list of parts in flash. Each part slides from one frequency to
// another (fromHz == toHz: a steady tone, 0 Hz: a pause) or, with noise set, jumps to a
// random frequency in that range every few ms (explosions). A new effect only replaces
// the one playing if it is at least as important (priority).
struct SfxPart { uint16_t fromHz, toHz, ms; uint8_t noise; };

const SfxPart* sfxParts = NULL;
uint8_t sfxCount = 0, sfxIndex = 0, sfxPriority = 0;
SfxPart sfxPart;                         // copy of the part playing now
uint32_t sfxPartStart = 0, sfxNoiseAt = 0;
uint16_t sfxNoiseHz = 0;

// Starts a sound effect on buzzer 3, unless a more important one is playing.
void soundSfx(const SfxPart* parts, uint8_t count, uint8_t priority) {
  if (sfxCount && priority < sfxPriority) return;
  sfxParts = parts;
  sfxCount = count;
  sfxIndex = 0;
  sfxPriority = priority;
  sfxPartStart = millis();
  memcpy_P(&sfxPart, &parts[0], sizeof(SfxPart));
}

// Stops all music and effects and silences the three buzzers.
void soundStop() {
  sfxCount = 0;
  for (uint8_t i = 0; i < BUZZER_COUNT; i++) {
    voices[i].notes = NULL;
    voices[i].effect = false;
    buzzerTone(i, 0);
  }
}

// Plays the current effect on buzzer 3: moves on to the next part when one ends, slides
// the frequency, or jumps to a random frequency every 6 ms (noise for explosions).
void sfxUpdate(uint32_t now) {
  if (!sfxCount) return;
  Voice& v = voices[BUZZER_COUNT - 1];
  while (now - sfxPartStart >= sfxPart.ms) {
    sfxPartStart += sfxPart.ms;
    if (++sfxIndex == sfxCount) {
      sfxCount = 0;
      v.effect = false;
      return;
    }
    memcpy_P(&sfxPart, &sfxParts[sfxIndex], sizeof(SfxPart));
  }
  uint16_t hz;
  if (sfxPart.noise) {
    if ((int32_t)(now - sfxNoiseAt) >= 0) {
      sfxNoiseHz = random(sfxPart.fromHz, sfxPart.toHz);
      sfxNoiseAt = now + 6;
    }
    hz = sfxNoiseHz;
  } else {
    int32_t span = (int32_t)sfxPart.toHz - (int32_t)sfxPart.fromHz;
    hz = sfxPart.fromHz + span * (int32_t)(now - sfxPartStart) / sfxPart.ms;
  }
  v.effect = true;
  v.effectHz = hz;
  v.effectEnd = 0;
}

// ================================================================= effects
// An effect takes over buzzer v at hz until soundRelease (logo whoosh and crackle).
void soundEffect(uint8_t v, uint16_t hz) {
  voices[v].effect = true;
  voices[v].effectHz = hz;
  voices[v].effectEnd = 0;
}

// Gives buzzer v back to the music.
void soundRelease(uint8_t v) {
  voices[v].effect = false;
}

// A short beep on buzzer 3 (menus, alerts); it replaces any effect.
void soundBeep(uint16_t hz, uint16_t ms) {
  sfxCount = 0;                                    // a beep replaces any effect
  Voice& v = voices[BUZZER_COUNT - 1];
  v.effect = true;
  v.effectHz = hz;
  v.effectEnd = millis() + ms;
  if (v.effectEnd == 0) v.effectEnd = 1;
}

// Turns all sound off or on (M key); the music keeps running silently.
void soundSetMuted(bool muted) {
  soundMuted = muted;
  for (uint8_t i = 0; i < BUZZER_COUNT; i++) buzzerTone(i, 0);
}

// Call as often as possible: moves every voice on to its next note and drives the buzzers.
void soundUpdate(uint32_t now) {
  sfxUpdate(now);
  for (uint8_t i = 0; i < BUZZER_COUNT; i++) {
    Voice& v = voices[i];
    while (v.notes && now - v.noteStart >= v.noteMs) {   // next note (catch up if late)
      v.noteStart += v.noteMs;
      if (++v.index == v.count) {
        if (!songLoops) {
          v.notes = NULL;
          break;
        }
        v.index = 0;
      }
      loadNote(v);
    }
    if (v.effect && v.effectEnd && (int32_t)(now - v.effectEnd) >= 0) v.effect = false;
    uint16_t hz = 0;
    if (v.effect) hz = v.effectHz;
    else if (v.notes && v.pitch && now - v.noteStart < (uint32_t)v.noteMs - NOTE_GAP_MS)
      hz = noteHz(v.pitch);
    buzzerTone(i, hz);
  }
}
