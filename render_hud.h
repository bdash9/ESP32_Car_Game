/*
  ═══════════════════════════════════════════════════════════════
  HUD AND INSTRUMENT RENDERING
  ═══════════════════════════════════════════════════════════════
*/

#ifndef RENDER_HUD_H
#define RENDER_HUD_H

#include <Arduino.h>

void drawHUD(float speed, float maxSpeed,
             float currentLapTime, float bestLapTime);
void drawSpeedometer(float speed, float maxSpeed);
void drawTachometer(float rpm, int gear);
void drawHGearPattern(int gear);
void drawTurboMeter(float charge, bool active);
void drawMiniMap();

#endif
