/*
  ═══════════════════════════════════════════════════════════════
  HUD AND INSTRUMENT RENDERING IMPLEMENTATION
  ═══════════════════════════════════════════════════════════════
*/

#include "render_hud.h"
#include "rendering.h"
#include "config.h"
#include "colors.h"
#include "physics.h"
#include "track.h"
#include "utils.h"

// ── H-pattern gear display ────────────────────────────────────
// [1][3]  top row
// [2][4]  bottom row
void drawHGearPattern(int gear) {
  const int SX = 26;
  const int SY = 172;
  const int BW = 15;
  const int BH = 10;
  const int GX = 4;
  const int GY = 3;

  int totW = BW * 2 + GX + 4;
  int totH = BH * 2 + GY + 4;

  spr.fillRect(SX - 2, SY - 2, totW, totH, rgb(10, 10, 10));
  spr.drawRect(SX - 2, SY - 2, totW, totH, rgb(45, 45, 45));

  int vcx = SX + BW + GX / 2;
  spr.drawFastVLine(vcx, SY - 1, totH - 2, rgb(55, 55, 55));

  const int gearCol[4] = { 0, 0, 1, 1 };
//  const int gearRow[4] = { 1, 0, 1, 0 }; //first in bottom left
  const int gearRow[4] = { 0, 1, 0, 1 };
  for (int g = 0; g < NUM_GEARS; g++) {
    int bx = SX + gearCol[g] * (BW + GX);
    int by = SY + gearRow[g] * (BH + GY);

    bool     active = (g == gear);
    uint16_t bgCol  = active ? rgb(180, 20, 20) : rgb(20, 20, 20);
    uint16_t bdrCol = active ? rgb(220, 60, 60) : rgb(45, 45, 45);
    uint16_t txtCol = active ? TFT_WHITE         : rgb(80, 80, 80);

    spr.fillRect(bx, by, BW, BH, bgCol);
    spr.drawRect(bx, by, BW, BH, bdrCol);
    spr.setTextSize(1);
    spr.setTextColor(txtCol);
    spr.setCursor(bx + (BW / 2) - 3, by + (BH / 2) - 3);
    spr.print(g + 1);
  }
}

// ── Turbo meter ───────────────────────────────────────────────
// Top center, radius 16 (half of tachometer).
// Arc fills left-to-right as charge builds:
//   0-33%  → RED   (bottom-left section lights up)
//   33-66% → ORANGE (top section lights up)
//   66-100% → GREEN (right section lights up, blinks when full)
// When active: full arc pulses yellow.
void drawTurboMeter(float charge, bool active) {
  const int cx = SCR_CX;   // 160 — top center
  const int cy = 22;
  const int r  = 16;

  bool blink = ((millis() / 300) % 2) == 0;
  bool ready = (charge >= 1.0f) && !active;

  // ── Background circle ─────────────────────────────────────
  spr.fillCircle(cx, cy, r + 2, rgb(15, 15, 15));
  spr.drawCircle(cx, cy, r + 2, TFT_WHITE);
  spr.drawCircle(cx, cy, r + 1, rgb(40, 40, 40));

  // ── Arc drawing helper ────────────────────────────────────
  // Draws dense radial lines from inner to outer rim
  auto drawArc = [&](int startDeg, int endDeg, uint16_t col) {
    for (int deg = startDeg; deg <= endDeg; deg += 2) {
      float a  = (float)deg * PI / 180.0f;
      int   x1 = cx + (int)(cosf(a) * (r - 4));
      int   y1 = cy + (int)(sinf(a) * (r - 4));
      int   x2 = cx + (int)(cosf(a) * (r - 1));
      int   y2 = cy + (int)(sinf(a) * (r - 1));
      spr.drawLine(x1, y1, x2, y2, col);
    }
  };

  // ── Zone boundaries ───────────────────────────────────────
  // RED   zone: -225° to -135° (0%  → 33% of sweep)
  // ORANGE zone: -135° to  -45° (33% → 66%)
  // GREEN  zone:  -45° to  +45° (66% → 100%)

  // Always draw dim background for all 3 zones
  drawArc(-225, -135, rgb(35,  5,  5));   // Dim red
  drawArc(-135,  -45, rgb(35, 18,  0));   // Dim orange
  drawArc( -45,   45, rgb(  0, 25, 5));   // Dim green

  if (active) {
    // ── ACTIVE: full arc pulses yellow ───────────────────────
    uint16_t pulseCol = blink ? rgb(255, 220, 0) : rgb(200, 160, 0);
    drawArc(-225, 45, pulseCol);

  } else {
    // ── BUILDING: reveal charge left-to-right ────────────────
    // fillEndDeg is how far the charge arc has progressed
    float fillEndDeg = -225.0f + charge * 270.0f;

    // RED zone (0-33%)
    if (fillEndDeg > -225.0f) {
      int zEnd = (int)min(fillEndDeg, -135.0f);
      drawArc(-225, zEnd, rgb(220, 20, 20));
    }

    // ORANGE zone (33-66%)
    if (fillEndDeg > -135.0f) {
      int zEnd = (int)min(fillEndDeg, -45.0f);
      drawArc(-135, zEnd, rgb(255, 140, 0));
    }

    // GREEN zone (66-100%) — blinks when meter is full/ready
    if (fillEndDeg > -45.0f) {
      bool showGreen = !ready || blink;
      if (showGreen) {
        int zEnd = (int)min(fillEndDeg, 45.0f);
        drawArc(-45, zEnd, rgb(0, 220, 60));
      }
    }
  }

  // ── Center text ───────────────────────────────────────────
  spr.setTextSize(1);
  if (active) {
    uint16_t c = blink ? TFT_YELLOW : rgb(200, 160, 0);
    spr.setTextColor(c);
    spr.setCursor(cx - 9, cy - 4);
    spr.print("GO!");
  } else if (ready) {
    spr.setTextColor(blink ? TFT_GREEN : rgb(0, 150, 30));
    spr.setCursor(cx - 12, cy - 4);
    spr.print("RDY");
  }

  // ── "TURBO" label below dial ──────────────────────────────
  spr.setTextSize(1);
  uint16_t lblCol = active ? TFT_YELLOW
                 : ready   ? TFT_GREEN
                 : TFT_WHITE;
  spr.setTextColor(lblCol);
  spr.setCursor(cx - 15, cy + r + 4);
  spr.print("TURBO");
}

// ── Tachometer ────────────────────────────────────────────────
void drawTachometer(float rpmVal, int gear) {
  const int cx = 44;
  const int cy = SCR_H - 44;
  const int r  = 32;

  float rpmClamped  = max(0.0f, min(rpmVal, MAX_RPM));
  bool  overRedline = (rpmVal >= REDLINE);

  spr.fillCircle(cx, cy, r + 3, rgb(15, 15, 15));
  spr.drawCircle(cx, cy, r + 3, TFT_WHITE);
  spr.drawCircle(cx, cy, r + 2, rgb(50, 50, 50));

  // Red zone arc (8500-10000 RPM)
  for (int i = 85; i <= 100; i++) {
    float a  = (-225.0f + (i / 100.0f) * 270.0f) * PI / 180.0f;
    int   x1 = cx + (int)(cosf(a) * (r - 5));
    int   y1 = cy + (int)(sinf(a) * (r - 5));
    int   x2 = cx + (int)(cosf(a) * r);
    int   y2 = cy + (int)(sinf(a) * r);
    spr.drawLine(x1, y1, x2, y2, TFT_RED);
  }

  // Major ticks 0-10 every 27°
  for (int i = 0; i <= 10; i++) {
    float a   = (-225.0f + i * 27.0f) * PI / 180.0f;
    int   x1  = cx + (int)(cosf(a) * (r - 7));
    int   y1  = cy + (int)(sinf(a) * (r - 7));
    int   x2  = cx + (int)(cosf(a) * (r - 1));
    int   y2  = cy + (int)(sinf(a) * (r - 1));
    spr.drawLine(x1, y1, x2, y2, (i >= 9) ? TFT_RED : TFT_WHITE);
  }

  // Minor ticks every 500 RPM
  for (int i = 1; i <= 19; i += 2) {
    float a  = (-225.0f + i * 13.5f) * PI / 180.0f;
    int   x1 = cx + (int)(cosf(a) * (r - 4));
    int   y1 = cy + (int)(sinf(a) * (r - 4));
    int   x2 = cx + (int)(cosf(a) * (r - 1));
    int   y2 = cy + (int)(sinf(a) * (r - 1));
    spr.drawLine(x1, y1, x2, y2, rgb(100, 100, 100));
  }

  // Needle
  float needleAngle = (-225.0f + (rpmClamped / MAX_RPM) * 270.0f) * PI / 180.0f;
  int   nx = cx + (int)(cosf(needleAngle) * (r - 8));
  int   ny = cy + (int)(sinf(needleAngle) * (r - 8));

  uint16_t needleCol = overRedline ? TFT_RED : rgb(255, 220, 0);
  spr.drawLine(cx+1, cy+1, nx+1, ny+1, rgb(10, 10, 10));
  spr.drawLine(cx,   cy,   nx,   ny,   needleCol);
  spr.drawLine(cx-1, cy,   nx-1, ny,   needleCol);
  spr.drawLine(cx,   cy-1, nx,   ny-1, needleCol);

  spr.fillCircle(cx, cy, 3, needleCol);
  spr.drawCircle(cx, cy, 4, TFT_DARKGREY);

  // Gear number in center
  spr.setTextSize(2);
  spr.setTextColor(overRedline ? TFT_RED : TFT_WHITE, rgb(15, 15, 15));
  spr.setCursor(cx - 5, cy + 8);
  spr.print(gear + 1);

  // Labels 0-10 drawn last (transparent bg)
  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE);
  for (int i = 0; i <= 10; i++) {
    float a   = (-225.0f + i * 27.0f) * PI / 180.0f;
    int   tx  = cx + (int)(cosf(a) * (r - 13));
    int   ty  = cy + (int)(sinf(a) * (r - 13));
    spr.setCursor(tx + ((i >= 10) ? -6 : -3), ty - 3);
    spr.print(i);
  }
}

// ── Speedometer ───────────────────────────────────────────────
// Scale: 0-350 km/h (turbo takes car from 300→350, fills dial)
// 8 evenly spaced ticks (every 50 km/h)
// Labels at: 200 (near top), 300 (right center), 350 (far right)
void drawSpeedometer(float speed, float maxSpeed) {
  const int cx = SCR_W - 44;
  const int cy = SCR_H - 44;
  const int r  = 32;

  // ── FIX: scale changed from 300 to 350 ───────────────────────
  int kmh = (int)(speed * 350.0f / maxSpeed);
  kmh = max(0, min(350, kmh));

  // Background
  spr.fillCircle(cx, cy, r + 3, rgb(20, 20, 20));
  spr.drawCircle(cx, cy, r + 3, TFT_DARKGREY);
  spr.drawCircle(cx, cy, r + 2, rgb(60, 60, 60));

  // 8 tick marks at 0, 50, 100, 150, 200, 250, 300, 350 km/h
  // Angle step = 270° / 7 gaps = 38.57° per 50 km/h
  for (int i = 0; i <= 7; i++) {
    float angle = (-225.0f + i * (270.0f / 7.0f)) * PI / 180.0f;
    int   x1    = cx + (int)(cosf(angle) * (r - 7));
    int   y1    = cy + (int)(sinf(angle) * (r - 7));
    int   x2    = cx + (int)(cosf(angle) * (r - 2));
    int   y2    = cy + (int)(sinf(angle) * (r - 2));
    // Red ticks at 300 and 350 (turbo zone)
    spr.drawLine(x1, y1, x2, y2, (i >= 6) ? TFT_RED : TFT_ORANGE);
  }

  // Needle
  // FIX: was /300, now /350 to match new scale
  float needleAngle = (-225.0f + (kmh / 350.0f) * 270.0f) * PI / 180.0f;
  int   nx = cx + (int)(cosf(needleAngle) * (r - 9));
  int   ny = cy + (int)(sinf(needleAngle) * (r - 9));

  spr.drawLine(cx+1, cy+1, nx+1, ny+1, rgb(10,10,10));

  // FIX: red threshold raised to 300 (was 250) to match new scale
  uint16_t needleCol = (kmh > 300) ? TFT_RED
                     : (kmh > 200) ? TFT_YELLOW
                     : TFT_WHITE;
  spr.drawLine(cx,   cy,   nx,   ny,   needleCol);
  spr.drawLine(cx-1, cy,   nx-1, ny,   needleCol);
  spr.drawLine(cx,   cy-1, nx,   ny-1, needleCol);

  // Hub
  spr.fillCircle(cx, cy, 3, needleCol);
  spr.drawCircle(cx, cy, 4, TFT_DARKGREY);

  // Digital readout — drawn before labels
  spr.setTextSize(2);
  spr.setTextColor(needleCol, rgb(20, 20, 20));
  spr.setCursor(cx - 17, cy + 10);
  if (kmh < 100) spr.print(" ");
  if (kmh < 10)  spr.print(" ");
  spr.print(kmh);

  // ── 3 labels: 200 (top), 300 (right center), 350 (far right) ─
  // Drawn LAST with transparent background.
  // Positions match i=4 (top area), i=6 (right center), i=7 (end).
  // 300 marks the OLD max — useful reference.
  // 350 marks the TURBO max — the needle pegs here at full boost.
  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE);

const int labelVals[3] = { 100, 200, 300 };
const int labelIdx[3]  = {   2,   4,   6 };

  for (int l = 0; l < 3; l++) {
    float angle = (-225.0f + labelIdx[l] * (270.0f / 7.0f)) * PI / 180.0f;
    int   tx    = cx + (int)(cosf(angle) * (r - 15));
    int   ty    = cy + (int)(sinf(angle) * (r - 15));
    // 3-digit labels need -9 offset to center; "200" needs slight left nudge
    spr.setCursor(tx - 9, ty - 4);
    spr.print(labelVals[l]);
  }
}

// ── Mini-map ──────────────────────────────────────────────────
// Positioned on the left between the LAP counter (y≤36)
// and the H-gear pattern / tachometer (y≥170).
// Box: x=2, y=40, w=80, h=122.
void drawMiniMap() {
  const int mx = 2, my = 40, mw = 80, mh = 122;

  float rangeX = (float)(mapMaxX - mapMinX);
  float rangeY = (float)(mapMaxY - mapMinY);
  if (rangeX < 1.0f || rangeY < 1.0f) return;

  const int pad = 5;
  float sx = (float)(mw - pad * 2) / rangeX;
  float sy = (float)(mh - pad * 2) / rangeY;
  float s  = (sx < sy) ? sx : sy;

  int drawW = (int)(rangeX * s);
  int drawH = (int)(rangeY * s);
  int offX  = mx + pad + (mw - pad * 2 - drawW) / 2;
  int offY  = my + pad + (mh - pad * 2 - drawH) / 2;

  // Track outline — tunnel sections drawn lighter
  for (int i = 0; i < TOTAL_SEGS; i++) {
    int j  = (i + 1) % TOTAL_SEGS;
    int x1 = offX + (int)((mapPtsX[i] - mapMinX) * s);
    int y1 = offY + (int)((mapPtsY[i] - mapMinY) * s);
    int x2 = offX + (int)((mapPtsX[j] - mapMinX) * s);
    int y2 = offY + (int)((mapPtsY[j] - mapMinY) * s);
    uint16_t lc = segments[i].tunnel ? rgb(180, 180, 255)
                                     : TFT_WHITE;
    spr.drawLine(x1, y1, x2, y2, lc);
  }

  // Start/finish tick at segment 0
  {
    int sx0 = offX + (int)((mapPtsX[0] - mapMinX) * s);
    int sy0 = offY + (int)((mapPtsY[0] - mapMinY) * s);
    spr.drawFastHLine(sx0 - 3, sy0, 7, TFT_YELLOW);
  }

  // Traffic dots
  for (int i = 0; i < MAX_CARS; i++) {
    int cs   = findSegIdx(trafficCars[i].z);
    int cx2  = offX + (int)((mapPtsX[cs] - mapMinX) * s);
    int cy2  = offY + (int)((mapPtsY[cs] - mapMinY) * s);
    uint16_t dc = (trafficCars[i].type == CAR_ONCOMING)
                  ? rgb(255, 140, 0) : rgb(180, 180, 180);
    spr.fillCircle(cx2, cy2, 1, dc);
  }

  // Player dot (drawn last, on top)
  int ps = findSegIdx(position);
  int px = offX + (int)((mapPtsX[ps] - mapMinX) * s);
  int py = offY + (int)((mapPtsY[ps] - mapMinY) * s);
  spr.fillCircle(px, py, 3, TFT_RED);
  spr.drawCircle(px, py, 3, rgb(255, 120, 120));
}

// ── Full HUD ──────────────────────────────────────────────────
void drawHUD(float speed, float maxSpeed,
             float currentLapTime, float bestLapTime) {

  // Lap counter (top left)
  spr.fillRect(0, 0, 90, 36, TFT_BLACK);
  spr.drawRect(0, 0, 90, 36, TFT_RED);
  spr.drawRect(1, 1, 88, 34, TFT_DARKGREY);
  spr.setTextSize(2);
  spr.setTextColor(TFT_RED, TFT_BLACK);
  spr.setCursor(5, 4);
  spr.print("LAP ");
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.print(currentLap);
  spr.print("/");
  spr.print(totalLaps);

  // Current lap time
  spr.setTextSize(1);
  spr.setTextColor(TFT_YELLOW, TFT_BLACK);
  spr.setCursor(5, 22);
  spr.print("TIME ");
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  int mins     = (int)currentLapTime / 60;
  int secs     = (int)currentLapTime % 60;
  int decimals = (int)((currentLapTime - (int)currentLapTime) * 100);
  if (mins > 0) { spr.print(mins); spr.print(":"); if (secs < 10) spr.print("0"); }
  spr.print(secs); spr.print(".");
  if (decimals < 10) spr.print("0");
  spr.print(decimals);

/*
  // Best lap (top right)
  if (bestLapTime > 0.0f && bestLapTime < 999.0f) {
    spr.setCursor(SCR_W - 82, 2);
    spr.setTextColor(TFT_GREEN, TFT_BLACK);
    spr.print("BEST ");
    spr.print((int)bestLapTime);
    spr.print(".");
    spr.print((int)((bestLapTime - (int)bestLapTime) * 10));
    spr.print(" ");
  }
*/
  drawMiniMap();

  // Turbo meter (top center)
  drawTurboMeter(turboCharge, turboActive);

  // H-pattern (above tachometer)
  drawHGearPattern(currentGear);

  // Instruments
  drawTachometer(rpm, currentGear);
  drawSpeedometer(speed, maxSpeed);
}
