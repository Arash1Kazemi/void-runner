/*
 * title.h -- title screen: the VOID sign stays lit, ships keep flying past it, and a
 * banner "VOID RUNNER - PRESS R" crosses the screen every 7 seconds.
 */
#pragma once
#include "display.h"
#include "art.h"
#include "logo.h"

// Draws one picture of the title screen at time t (ms since it opened).
void titleDraw(uint32_t t, uint16_t frameNo) {
  clearScreen();
  for (uint8_t i = 0; i < 4; i++) drawLogoLetter(i);
  drawBitmap(0, -2 + (int16_t)(t % 2400 * 26 / 2400), ENEMY_DRONE, 3, 2, true);   // drone down the left
  int16_t shipY = 24 - (int16_t)(t % 2000 * 28 / 2000);                          // ship up the right
  drawBitmap(13, shipY, SHIP_UP, 3, 3, true);
  if (frameNo & 1) setPixel(14, shipY + 3, true);
  uint32_t b = t % 7000;
  if (b < 3600) {                                  // the banner
    fillRect(0, 8, SCREEN_W, 9, false);
    for (uint8_t x = 0; x < SCREEN_W; x++)
      if ((x + t / 60) % 3) {
        setPixel(x, 8, true);
        setPixel(x, 16, true);
      }
    drawText(16 - (int16_t)(b * 40 / 1000), 9, PSTR("VOID RUNNER - PRESS R"), true);
  }
  for (uint8_t bar = 0; bar < STATUS_ROWS; bar++) {      // status bars: a slow chase
    uint8_t head = (t / 120 + bar * 5) % 16;
    setStatusBits(bar, (1u << head) | (1u << ((head + 15) % 16)));
  }
}
