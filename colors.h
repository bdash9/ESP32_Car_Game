/*
  ═══════════════════════════════════════════════════════════════
  COLOR AND PALETTE MANAGEMENT
  ═══════════════════════════════════════════════════════════════
*/

#ifndef COLORS_H
#define COLORS_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════
//  COLOR PALETTE (Global Variables)
// ═══════════════════════════════════════════════════════════════
extern uint16_t colSky1, colSky2, colSky3;
extern uint16_t colGrassL, colGrassD;
extern uint16_t colRoadL, colRoadD;
extern uint16_t colRumbleL, colRumbleD;
extern uint16_t colLane, colFog;

// ═══════════════════════════════════════════════════════════════
//  FUNCTIONS
// ═══════════════════════════════════════════════════════════════

// Convert RGB to RGB565 format
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b);

// Darken a color
uint16_t darkenCol(uint16_t c, float f);

// Interpolate between two colors
uint16_t lerpCol(uint16_t c1, uint16_t c2, float t);

// Initialize colors based on time of day
extern int trackTheme;   // 0 = normal, 1 = ocean
void initColors(int timeOfDay, int theme = 0);

#endif // COLORS_H
