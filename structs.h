/*
  ═══════════════════════════════════════════════════════════════
  DATA STRUCTURES
  ═══════════════════════════════════════════════════════════════
*/

#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h>

// ── Road segment ─────────────────────────────────────────────
struct Segment {
  float    curve;
  float    y;
  int      spriteType;
  float    spriteOffset;
  bool     tunnel;
  int      buildL;
  int      buildR;
  uint16_t colorL;
  uint16_t colorR;
};

// ── Projected 2-D render point ───────────────────────────────
struct RenderPt {
  int16_t x;
  int16_t y;
  int16_t w;
  float   scale;
};

// ── Traffic car ───────────────────────────────────────────────
struct TrafficCar {
  float    offset;         // Lateral position on road  (-1.0 to +1.0)
  float    z;              // Position along track
  float    speed;          // Current speed (units/sec)
  uint16_t color;          // RGB565 body colour

  // ── NEW ──────────────────────────────────────────────────────
  int      type;           // CAR_SLOW/MEDIUM/FAST/ONCOMING/BRAKER
  float    baseSpeed;      // Normal speed before behaviour events
  int      behaviorTimer;  // Frames until next lane-change / brake event
};

#endif // STRUCTS_H
