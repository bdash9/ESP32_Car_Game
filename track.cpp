/*
  ═══════════════════════════════════════════════════════════════
  TRACK GENERATION IMPLEMENTATION
  ═══════════════════════════════════════════════════════════════
*/

#include "track.h"
#include "utils.h"
#include "colors.h"
#include "config.h"
#include <Arduino.h>

Segment    segments[TOTAL_SEGS];
int        segCount = 0;
float      trackLength;
TrafficCar trafficCars[MAX_CARS];

int16_t mapPtsX[TOTAL_SEGS];
int16_t mapPtsY[TOTAL_SEGS];
int16_t mapMinX = 0, mapMaxX = 0, mapMinY = 0, mapMaxY = 0;

int currentTrack = 0;

float lastY() {
  return (segCount == 0) ? 0 : segments[(segCount - 1) % TOTAL_SEGS].y;
}

void addSeg(float curve, float y, bool isTunnel) {
  if (segCount >= TOTAL_SEGS) return;
  segments[segCount].curve        = curve;
  segments[segCount].y            = y;
  segments[segCount].spriteType   = -1;
  segments[segCount].spriteOffset = 0;
  segments[segCount].tunnel       = isTunnel;
  segments[segCount].buildL       = 0;
  segments[segCount].buildR       = 0;
  segments[segCount].colorL       = 0;
  segments[segCount].colorR       = 0;
  segCount++;
}

void addRoad(int enter, int hold, int leave, float curve, float hillY) {
  float sY    = lastY();
  float eY    = sY + hillY * SEG_LEN;
  int   total = enter + hold + leave;
  for (int n = 0; n < enter; n++)
    addSeg(easeIn(0, curve, (float)n / enter),
           easeInOut(sY, eY, (float)n / total));
  for (int n = 0; n < hold; n++)
    addSeg(curve, easeInOut(sY, eY, (float)(enter + n) / total));
  for (int n = 0; n < leave; n++)
    addSeg(easeInOut(curve, 0, (float)n / leave),
           easeInOut(sY, eY, (float)(enter + hold + n) / total));
}

void addSprite(int idx, int type, float off) {
  if (idx >= 0 && idx < segCount) {
    segments[idx].spriteType   = type;
    segments[idx].spriteOffset = off;
  }
}

// ── Minimap path computation ───────────────────────────────────
// Per-track HSCALE forces heading to integrate to exactly 2π.
// Track 0 city:  4 × 90°  corners → HSCALE = 0.00747
// Track 1 ocean: 3 × 120° corners → HSCALE = 0.00779
// Track 2 grass: 2 × 180° corners → HSCALE = 0.01442
static void computeMinimapPoints() {
  const float HSCALE_TABLE[3] = { 0.00747f, 0.00779f, 0.01442f };
  float HSCALE = HSCALE_TABLE[currentTrack < 3 ? currentTrack : 0];

  float x = 0.0f, y = 0.0f, hdg = 0.0f;
  float rawX[TOTAL_SEGS], rawY[TOTAL_SEGS];
  float mnX =  1e9f, mxX = -1e9f;
  float mnY =  1e9f, mxY = -1e9f;

  for (int i = 0; i < TOTAL_SEGS; i++) {
    rawX[i] = x;
    rawY[i] = y;
    hdg += segments[i].curve * HSCALE;
    x   += sinf(hdg);
    y   -= cosf(hdg);
    if (x < mnX) mnX = x;
    if (x > mxX) mxX = x;
    if (y < mnY) mnY = y;
    if (y > mxY) mxY = y;
  }

  float rng = (mxX - mnX > mxY - mnY) ? (mxX - mnX) : (mxY - mnY);
  float sc  = (rng > 0.001f) ? 28000.0f / rng : 1.0f;

  mapMinX = (int16_t)(mnX * sc);
  mapMaxX = (int16_t)(mxX * sc);
  mapMinY = (int16_t)(mnY * sc);
  mapMaxY = (int16_t)(mxY * sc);

  for (int i = 0; i < TOTAL_SEGS; i++) {
    mapPtsX[i] = (int16_t)(rawX[i] * sc);
    mapPtsY[i] = (int16_t)(rawY[i] * sc);
  }
}

static uint16_t oceanBuildingColor() {
  switch (random(0, 5)) {
    case 0: return rgb(235, 228, 215);  // Cream white
    case 1: return rgb(185, 215, 240);  // Pastel blue
    case 2: return rgb(248, 218, 175);  // Sandy peach
    case 3: return rgb(210, 238, 205);  // Mint green
    default:return rgb(255, 235, 215);  // Warm white
  }
}

void buildTrack() {
  segCount = 0;

#if RANDOM_TRACK
  int   pendingReturnDir = 0;
  float pendingReturnMag = 0.0f;
  const int CLOSE_SEGS = 20;

  while (segCount < TOTAL_SEGS - CLOSE_SEGS) {
    int enter  = random(4, 8);
    int hold   = random(25, 50);
    int leave  = random(4, 8);
    int needed = enter + hold + leave;

    if (segCount + needed > TOTAL_SEGS - CLOSE_SEGS) break;

    float curve = 0.0f;
    if (random(0, 100) >= 45) {
      curve = (float)random(-80, 81) / 10.0f;
      if (curve > -2.0f && curve < 2.0f) curve = 0.0f;
    }

    float hill = 0.0f;
    if (pendingReturnDir != 0) {
      hill = (float)pendingReturnDir * pendingReturnMag;
      pendingReturnDir = 0;
    } else {
      float currentY = lastY();
      if (fabsf(currentY) > SEG_LEN * 4) {
        hill = (currentY > 0) ? -5.0f : 5.0f;
      } else {
        hill = (float)random(-5, 6);
        if (hill > -3.0f && hill < 3.0f) hill = 0.0f;
        if (hill != 0.0f) {
          pendingReturnDir = (hill > 0.0f) ? -1 : 1;
          pendingReturnMag = max(3.0f, fabsf(hill) * 0.5f);
        }
      }
    }

    addRoad(enter, hold, leave, curve, hill);
  }

  {
    float currentY = lastY();
    if (fabsf(currentY) > SEG_LEN * 0.5f) {
      int closeLeft = TOTAL_SEGS - CLOSE_SEGS - segCount;
      int enter = max(4, closeLeft / 3);
      int leave = max(4, closeLeft / 3);
      float hillY = clampF(-currentY / (float)SEG_LEN, -5.0f, 5.0f);
      addRoad(enter, 2, leave, 0.0f, hillY);
    }
  }

#else
  if (currentTrack == 0) {
    // ── Track 1: rectangular city circuit ─────────────────────
    // s1=16, s2=28, s3=16, s4=28 → opposite sides equal → closes
    addRoad(0, 16, 0,  0.0f,  0);   // Side 1                  16
    addRoad(4, 20, 4,  9.0f,  2);   // Corner 1 (90° CW)       28
    addRoad(0,  4, 0,  0.0f,  2);   //                           4
    addRoad(3,  5, 3, -7.0f,  0);   // Chicane left             11
    addRoad(0,  2, 0,  0.0f,  0);   // Gap                       2
    addRoad(3,  5, 3,  7.0f,  0);   // Chicane right            11
    // Side 2 total = 4+11+2+11 = 28 ✓
    addRoad(4, 20, 4,  9.0f, -2);   // Corner 2 (90° CW)       28
    addRoad(0, 16, 0,  0.0f,  0);   // Side 3                  16
    addRoad(4, 20, 4,  9.0f,  2);   // Corner 3 (90° CW)       28
    addRoad(0, 28, 0,  0.0f,  0);   // Side 4                  28
    addRoad(4, 20, 4,  9.0f, -2);   // Corner 4 (90° CW)       28
    // Total: 16+28+28+28+16+28+28+28 = 200 ✓

  } else if (currentTrack == 1) {
    // ── Track 2: coastal triangle circuit ─────────────────────
    // s1=s2=42, s3=41 → nearly equilateral → closes cleanly
    addRoad(0, 42, 0,   0.0f,  0);  // Side 1                  42
    addRoad(5, 15, 5,  14.0f,  3);  // Corner 1 (120°) uphill  25
    addRoad(0, 42, 0,   0.0f, -3);  // Side 2 coastal          42
    addRoad(5, 15, 5,  14.0f,  0);  // Corner 2 (120°)         25
    addRoad(0, 41, 0,   0.0f,  0);  // Side 3                  41
    addRoad(5, 15, 5,  14.0f,  0);  // Corner 3 (120°)         25
    // Total: 42+25+42+25+41+25 = 200 ✓

  } else {
    // Track 3: grass oval — two clean 180° sweepers
    // Side A = Side B = 70 segs → perfect oval closure on minimap
    // No chicane — keeps the line drawing clean at minimap scale
    addRoad(0, 70, 0,  0.0f,  3);  // Side A: start straight, uphill     70
    addRoad(5, 20, 5,  9.0f, -3);  // Corner 1: 180° right sweep         30
    addRoad(0, 70, 0,  0.0f,  0);  // Side B: back straight, flat        70
    addRoad(5, 20, 5,  9.0f,  0);  // Corner 2: 180° right sweep         30
    // Total: 70+30+70+30 = 200 ✓
    // Corner curve sum = 2 × 217.8 = 435.6 → HSCALE 0.01442 unchanged ✓
  }
#endif

  while (segCount < TOTAL_SEGS) addSeg(0, 0, false);
  trackLength = (float)TOTAL_SEGS * SEG_LEN;

  // ── Tunnel placement ────────────────────────────────────────
  if (currentTrack == 0) {
    int tunnelStart = TOTAL_SEGS / 3;
    int tunnelLen   = min(60, TOTAL_SEGS - tunnelStart - 1);
    for (int i = tunnelStart; i < tunnelStart + tunnelLen; i++) {
      segments[i].tunnel = true;
      segments[i].buildL = 0;
      segments[i].buildR = 0;
    }
  } else if (currentTrack == 1) {
    int tunnelStart = 80;
    int tunnelLen   = 25;
    for (int i = tunnelStart; i < tunnelStart + tunnelLen; i++) {
      segments[i].tunnel = true;
      segments[i].buildL = 0;
      segments[i].buildR = 0;
    }
  }
  // Track 2 (grass): no tunnel — open fields throughout

  // ── Buildings (city and ocean only) ─────────────────────────
  if (currentTrack != 2) {
    int buildCounterL = 0, buildCounterR = 0;
    int curBuildL = 0,     curBuildR = 0;
    uint16_t curColL = 0,  curColR = 0;

    for (int i = 0; i < TOTAL_SEGS; i++) {
      if (segments[i].tunnel) continue;

      if (buildCounterL <= 0) {
        if (random(0, 10) < 6) {
          curBuildL     = random(BUILDING_H_MIN, BUILDING_H_MAX);
          curColL       = (currentTrack == 1)
                          ? oceanBuildingColor()
                          : rgb(random(40,140), random(40,120), random(50,130));
          buildCounterL = random(BUILDING_SEG_MIN, BUILDING_SEG_MAX);
        } else {
          curBuildL     = 0;
          buildCounterL = random(BUILDING_GAP_MIN, BUILDING_GAP_MAX);
        }
      }
      segments[i].buildL = curBuildL;
      segments[i].colorL = curColL;
      buildCounterL--;

      if (buildCounterR <= 0) {
        if (random(0, 10) < 6) {
          curBuildR     = random(BUILDING_H_MIN, BUILDING_H_MAX);
          curColR       = (currentTrack == 1)
                          ? oceanBuildingColor()
                          : rgb(random(40,140), random(40,120), random(50,130));
          buildCounterR = random(BUILDING_SEG_MIN, BUILDING_SEG_MAX);
        } else {
          curBuildR     = 0;
          buildCounterR = random(BUILDING_GAP_MIN, BUILDING_GAP_MAX);
        }
      }
      segments[i].buildR = curBuildR;
      segments[i].colorR = curColR;
      buildCounterR--;
    }
  }

  // ── Roadside sprites ─────────────────────────────────────────
  if (currentTrack == 2) {
    // Grass: dense tree lines both sides
    for (int n = 2; n < segCount; n++) {
      if (segments[n].tunnel) continue;
      int r2 = random(0, 100);
      if      (r2 < 20) addSprite(n, 0, -1.5f);  // Pine left
      else if (r2 < 40) addSprite(n, 0,  1.5f);  // Pine right
      else if (r2 < 46) addSprite(n, 1, -1.8f);  // Round tree left
      else if (r2 < 52) addSprite(n, 1,  1.8f);  // Round tree right
      else if (r2 < 54) addSprite(n, 2, -1.6f);  // Bush left
      else if (r2 < 56) addSprite(n, 2,  1.6f);  // Bush right
    }
  } else {
    // City and ocean: sparse sprites in building gaps
    for (int n = 5; n < segCount; n++) {
      if (segments[n].tunnel ||
          segments[n].buildL > 0 ||
          segments[n].buildR > 0) continue;
      int r2 = random(0, 100);
      if      (r2 < 10) addSprite(n, random(0, 3), -1.5f);
      else if (r2 < 20) addSprite(n, random(0, 3),  1.5f);
    }
  }

  computeMinimapPoints();
}

void switchToNextTrack(float maxSpeed) {
  currentTrack = (currentTrack + 1) % NUM_TRACKS;
  buildTrack();
  initTraffic(maxSpeed);

  // Reset player to 1st gear, clear motion state
  extern int   currentGear;
  extern float rpm;
  extern float turboCharge;
  extern float turboTimeLeft;
  extern bool  turboActive;
  extern float speed;
  extern float acceleration;
  extern float velocityX;

  currentGear   = 0;
  rpm           = 800.0f;
  turboTimeLeft = 0.0f;
  turboActive   = false;
  speed         = 0.0f;
  acceleration  = 0.0f;
  velocityX     = 0.0f;
  // turboCharge preserved — player keeps earned progress
}

// ── Traffic colors ────────────────────────────────────────────
const uint16_t PROGMEM trafficColors[] = {
  0x051C, 0xDDE0, 0xC618, 0x05A0, 0xFC60, 0xA01C,
  0x05BC, 0xB1E8, 0xFDB2, 0x6318, 0x0780, 0xC5E0
};

void initTraffic(float mxSpd) {
  const int typeOrder[MAX_CARS] = {
    CAR_SLOW,
    CAR_ONCOMING
  };

  int aheadCount = 0;

  for (int i = 0; i < MAX_CARS; i++) {
    int t = typeOrder[i];
    trafficCars[i].type = t;

    float    spd;
    uint16_t col;
    float    off;
    int      btimer;

    switch (t) {
      case CAR_SLOW:
        spd    = mxSpd * (0.15f + random(0, 10) / 100.0f);
        col    = rgb(180, 90, 40);
        off    = (random(0, 2) == 0) ? -0.35f : 0.35f;
        btimer = random(250, 500);
        break;
      case CAR_ONCOMING:
        spd    = mxSpd * (0.35f + random(0, 15) / 100.0f);
        col    = rgb(240, 220, 30);
        off    = (random(0, 2) == 0) ? -0.55f : 0.55f;
        btimer = random(300, 600);
        break;
      default:
        spd    = mxSpd * 0.40f;
        col    = pgm_read_word(&trafficColors[i % 12]);
        off    = 0.0f;
        btimer = 200;
        break;
    }

    trafficCars[i].speed         = spd;
    trafficCars[i].baseSpeed     = spd;
    trafficCars[i].color         = col;
    trafficCars[i].offset        = off;
    trafficCars[i].behaviorTimer = btimer;

    if (t == CAR_ONCOMING) {
      int seg = RESPAWN_ONCOMING_MIN + random(0, 20);
      trafficCars[i].z = seg * (float)SEG_LEN + random(0, SEG_LEN);
    } else {
      int seg = 20 + aheadCount * 80;
      if (seg >= TOTAL_SEGS) seg = TOTAL_SEGS - 15;
      trafficCars[i].z = seg * (float)SEG_LEN + random(0, SEG_LEN);
      aheadCount++;
    }
  }
}