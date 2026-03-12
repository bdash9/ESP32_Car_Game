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

void buildTrack() {
  segCount = 0;

#if RANDOM_TRACK
  int   pendingReturnDir = 0;
  float pendingReturnMag = 0.0f;
  const int CLOSE_SEGS = 20;

  while (segCount < TOTAL_SEGS - CLOSE_SEGS) {
    int enter  = random(4, 8);
    // FIX: longer straights — was random(6,14), now random(25,50)
    int hold   = random(25, 50);
    int leave  = random(4, 8);
    int needed = enter + hold + leave;

    if (segCount + needed > TOTAL_SEGS - CLOSE_SEGS) break;

    // FIX: 45% of sections are pure straights (curve=0)
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

  // Level Y back to 0 at the end of the loop
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
  // Fixed circuit — more straights added between curves
  addRoad(5, 30, 5,  0,     0);   // Long start straight
  addRoad(8, 12, 8, -6.0,  5);
  addRoad(5, 25, 5,  0,    0);    // Long straight
  addRoad(8, 12, 8,  7.0,  0);
  addRoad(5, 30, 5,  0,    5);    // Long straight
  addRoad(8, 12, 8, -5.5, -5);
  addRoad(5, 25, 5,  0,    0);
  addRoad(8, 12, 8,  6.5,  5);
  addRoad(5, 30, 5,  0,   -5);    // Long straight
  addRoad(8, 12, 8, -7.5,  0);
  addRoad(5, 25, 5,  0,    5);
  addRoad(8, 12, 8,  5.0, -5);
  addRoad(5, 30, 5,  0,    0);    // Long straight
#endif

  while (segCount < TOTAL_SEGS) addSeg(0, 0, false);
  trackLength = (float)TOTAL_SEGS * SEG_LEN;

  // Single tunnel
  int tunnelStart = TOTAL_SEGS / 3;
  int tunnelLen   = min(60, TOTAL_SEGS - tunnelStart - 1);
  for (int i = tunnelStart; i < tunnelStart + tunnelLen; i++) {
    segments[i].tunnel = true;
    segments[i].buildL = 0;
    segments[i].buildR = 0;
  }

  // Buildings
  int buildCounterL = 0, buildCounterR = 0;
  int curBuildL = 0,     curBuildR = 0;
  uint16_t curColL = 0,  curColR = 0;

  for (int i = 0; i < TOTAL_SEGS; i++) {
    if (segments[i].tunnel) continue;

    if (buildCounterL <= 0) {
      if (random(0, 10) < 6) {
        curBuildL     = random(BUILDING_H_MIN, BUILDING_H_MAX);
        curColL       = rgb(random(40,140), random(40,120), random(50,130));
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
        curColR       = rgb(random(40,140), random(40,120), random(50,130));
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

  // Trees in gaps
  for (int n = 5; n < segCount; n++) {
    if (segments[n].tunnel ||
        segments[n].buildL > 0 ||
        segments[n].buildR > 0) continue;
    int r2 = random(0, 100);
    if      (r2 < 10) addSprite(n, random(0, 3), -1.5f);
    else if (r2 < 20) addSprite(n, random(0, 3),  1.5f);
  }

} // ← end of buildTrack()

// ── Traffic colors (PROGMEM) ──────────────────────────────────
const uint16_t PROGMEM trafficColors[] = {
  0x051C, 0xDDE0, 0xC618, 0x05A0, 0xFC60, 0xA01C,
  0x05BC, 0xB1E8, 0xFDB2, 0x6318, 0x0780, 0xC5E0
};

// ── Traffic init ──────────────────────────────────────────────
void initTraffic(float mxSpd) {

  // 2 cars: 1 slow blocker + 1 oncoming
  // Sparse enough that each encounter feels meaningful
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
      // Place at segment 20 — well inside draw distance but visible
      int seg = 20 + aheadCount * 80;
      if (seg >= TOTAL_SEGS) seg = TOTAL_SEGS - 15;
      trafficCars[i].z = seg * (float)SEG_LEN + random(0, SEG_LEN);
      aheadCount++;
    }
  }
}
