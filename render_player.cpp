/*
  ═══════════════════════════════════════════════════════════════
  PLAYER CAR RENDERING IMPLEMENTATION
  3D rendering with OBJ mesh + RGB565 texture
  ═══════════════════════════════════════════════════════════════
*/

#include "render_player.h"
#include "rendering.h"
#include "config.h"
#include "colors.h"
#include "physics.h"
#include "track.h"
#include "utils.h"
#include "car2_mesh.h"
#include "car2_texture.h"
#include "opponent.h"

// ---------------------------------------------------------------------------
// Textured triangle rasterizer (affine mapping, scanline)
// ---------------------------------------------------------------------------
static void drawTexturedTri(
    float ax, float ay, float au, float av,
    float bx, float by, float bu, float bv,
    float cx, float cy, float cu, float cv,
    float light)
{
  // Sort vertices by ascending Y (a <= b <= c)
  if (ay > by) { float t; t=ax;ax=bx;bx=t; t=ay;ay=by;by=t; t=au;au=bu;bu=t; t=av;av=bv;bv=t; }
  if (ay > cy) { float t; t=ax;ax=cx;cx=t; t=ay;ay=cy;cy=t; t=au;au=cu;cu=t; t=av;av=cv;cv=t; }
  if (by > cy) { float t; t=bx;bx=cx;cx=t; t=by;by=cy;cy=t; t=bu;bu=cu;cu=t; t=bv;bv=cv;cv=t; }

  if (cy - ay < 0.5f) return;

  int yA = (int)ay, yB = (int)by, yC = (int)cy;
  yA = max(yA, 0); yC = min(yC, SCR_H - 1);
  if (yA > yC) return;

  for (int y = yA; y <= yC; y++) {
    float t_AC = (cy - ay > 0.001f) ? (float)(y - ay) / (cy - ay) : 0.0f;
    float xAC = ax + t_AC * (cx - ax);
    float uAC = au + t_AC * (cu - au);
    float vAC = av + t_AC * (cv - av);

    float xL, xR, uL, uR, vL, vR;
    if (y < yB) {
      float t_AB = (yB != (int)ay) ? (float)(y - ay) / (by - ay) : 0.0f;
      float xAB = ax + t_AB * (bx - ax);
      float uAB = au + t_AB * (bu - au);
      float vAB = av + t_AB * (bv - av);
      if (xAC < xAB) { xL=xAC; xR=xAB; uL=uAC; uR=uAB; vL=vAC; vR=vAB; }
      else            { xL=xAB; xR=xAC; uL=uAB; uR=uAC; vL=vAB; vR=vAC; }
    } else {
      float t_BC = (yC != yB) ? (float)(y - by) / (cy - by) : 0.0f;
      float xBC = bx + t_BC * (cx - bx);
      float uBC = bu + t_BC * (cu - bu);
      float vBC = bv + t_BC * (cv - bv);
      if (xAC < xBC) { xL=xAC; xR=xBC; uL=uAC; uR=uBC; vL=vAC; vR=vBC; }
      else            { xL=xBC; xR=xAC; uL=uBC; uR=uAC; vL=vBC; vR=vAC; }
    }

    if (xR - xL < 0.5f) continue;
    if (xR - xL > 160.0f) continue;
    int x0 = max((int)xL, 0);
    int x1 = min((int)xR, SCR_W - 1);
    if (x0 > x1) continue;
    float dx = 1.0f / (xR - xL);

    for (int x = x0; x <= x1; x++) {
      float t = (x - xL) * dx;
      float u = uL + t * (uR - uL);
      float v = vL + t * (vR - vL);

      int tx = (int)(u * (CAR2_TEX_W - 1));
      int ty = (int)((1.0f - v) * (CAR2_TEX_H - 1));
      tx = max(0, min(tx, CAR2_TEX_W - 1));
      ty = max(0, min(ty, CAR2_TEX_H - 1));

#ifdef ARDUINO
      uint16_t texel = pgm_read_word(&car2_texture[ty * CAR2_TEX_W + tx]);
#else
      uint16_t texel = car2_texture[ty * CAR2_TEX_W + tx];
#endif

      if (light < 0.99f) {
        uint8_t r = ((texel >> 11) & 0x1F);
        uint8_t g = ((texel >>  5) & 0x3F);
        uint8_t b = ( texel        & 0x1F);
        r = (uint8_t)(r * light);
        g = (uint8_t)(g * light);
        b = (uint8_t)(b * light);
        texel = (r << 11) | (g << 5) | b;
      }

      spr.drawPixel(x, y, texel);
    }
  }
}

// ---------------------------------------------------------------------------
// Project and render the full Car2 mesh (unchanged)
// ---------------------------------------------------------------------------
static void renderCar2Mesh(int centerX, int centerY,
                            float rotY, float pitch, float camDist, float fov)
{
  float cosY = cosf(rotY), sinY = sinf(rotY);
  float cosP = cosf(pitch), sinP = sinf(pitch);

  static float px[428], py[428], pz[428];

  for (int i = 0; i < car2_vert_count; i++) {
    float x = car2_verts[i].x;
    float y = car2_verts[i].y;
    float z = car2_verts[i].z;

    y -= 0.5f;

    float rx = x * cosY - z * sinY;
    float ry = -y;
    float rz = x * sinY + z * cosY;

    float fy = ry * cosP - rz * sinP;
    float fz = ry * sinP + rz * cosP;

    fz += camDist;
    pz[i] = fz;

    if (fz > 0.01f) {
      px[i] = centerX + (rx * fov) / fz;
      py[i] = centerY + (fy * fov) / fz;
    } else {
      px[i] = -9999;
      py[i] = -9999;
    }
  }

  static int order[312];
  static float zdepth[312];
  int ntri = car2_tri_count;

  for (int t = 0; t < ntri; t++) {
    int i0 = car2_indices[t*3+0];
    int i1 = car2_indices[t*3+1];
    int i2 = car2_indices[t*3+2];
    zdepth[t] = (pz[i0] + pz[i1] + pz[i2]) / 3.0f;
    order[t] = t;
  }

  for (int i = 1; i < ntri; i++) {
    float kd = zdepth[order[i]];
    int   ki = order[i];
    int j = i - 1;
    while (j >= 0 && zdepth[order[j]] < kd) {
      order[j+1] = order[j];
      j--;
    }
    order[j+1] = ki;
  }

  for (int ti = 0; ti < ntri; ti++) {
    int t  = order[ti];
    int i0 = car2_indices[t*3+0];
    int i1 = car2_indices[t*3+1];
    int i2 = car2_indices[t*3+2];

    if (pz[i0] < 0.01f || pz[i1] < 0.01f || pz[i2] < 0.01f) continue;

    float ax = px[i0], ay = py[i0];
    float bx = px[i1], by = py[i1];
    float cx = px[i2], cy = py[i2];

    const float MARGIN = 20.0f;
    if (ax < -MARGIN || ax > SCR_W+MARGIN || bx < -MARGIN || bx > SCR_W+MARGIN ||
        cx < -MARGIN || cx > SCR_W+MARGIN) continue;
    if (ay < -MARGIN || ay > SCR_H+MARGIN || by < -MARGIN || by > SCR_H+MARGIN ||
        cy < -MARGIN || cy > SCR_H+MARGIN) continue;

    float cross = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax);
    if (cross <= 0) continue;

    float nx = (by-ay)*(0) - (by-ay)*(cy-ay);
    float ex1 = car2_verts[i1].x - car2_verts[i0].x;
    float ey1 = car2_verts[i1].y - car2_verts[i0].y;
    float ez1 = car2_verts[i1].z - car2_verts[i0].z;
    float ex2 = car2_verts[i2].x - car2_verts[i0].x;
    float ey2 = car2_verts[i2].y - car2_verts[i0].y;
    float ez2 = car2_verts[i2].z - car2_verts[i0].z;
    float fnx = ey1*ez2 - ez1*ey2;
    float fny = ez1*ex2 - ex1*ez2;
    float fnz = ex1*ey2 - ey1*ex2;
    float fnlen = sqrtf(fnx*fnx + fny*fny + fnz*fnz);
    if (fnlen > 0.0001f) { fnx/=fnlen; fny/=fnlen; fnz/=fnlen; }
    float lx=0.0f, ly=0.7f, lz=-0.7f;
    float dot = fnx*lx + fny*ly + fnz*lz;
    float light = 0.35f + 0.65f * max(0.0f, dot);

    drawTexturedTri(
      ax, ay, car2_verts[i0].u, car2_verts[i0].v,
      bx, by, car2_verts[i1].u, car2_verts[i1].v,
      cx, cy, car2_verts[i2].u, car2_verts[i2].v,
      light);
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void drawPlayerCar() {

  // ── FIX 1: Car stays at screen center (OutRun style) ─────────────────────
  // In pseudo-3D racing the road moves UNDER the car — the car does not
  // chase playerX across the screen. drawRoad() already uses playerX to
  // shift the road. Moving centerX here too would double-count the offset.
  int centerX = SCR_CX;
  int centerY = SCR_H - 40;

  // Road pitch (unchanged)
  int segIdx = findSegIdx(position + playerZdist);
  const int SLOPE_SAMPLES = 6;
  float yStart = segments[segIdx].y;
  int   farIdx = (segIdx + SLOPE_SAMPLES) % TOTAL_SEGS;
  float yEnd   = segments[farIdx].y;
  float slope  = (yEnd - yStart) / (SEG_LEN * SLOPE_SAMPLES);
  float roadPitch = atanf(slope) * 0.20f;
  roadPitch = clampF(roadPitch, -0.25f, 0.25f);
  static float smoothPitch = 0.0f;
  smoothPitch += (roadPitch - smoothPitch) * 0.15f;

  // ── FIX 2: Rotation based on steering INPUT, not absolute road position ───
  //
  // OLD CODE:  rotY = playerX * 0.5f
  //   Problem: playerX is absolute position (-0.8 to +0.8).
  //            While holding a button playerX keeps growing, so rotY keeps
  //            growing too — the car spins continuously rather than leaning.
  //            Also: the pivot is the mesh center so only the tail visually
  //            swings while the front (near the pivot) barely moved.
  //
  // NEW CODE:  Read buttons directly. Smooth the lean angle in/out.
  //            Car leans while button is held, returns upright on release.
  //            Max lean ≈ ±14 degrees — enough to feel responsive.
  static float smoothSteer = 0.0f;
  bool leftPressed  = (digitalRead(BTN_LEFT)  == LOW);
  bool rightPressed = (digitalRead(BTN_RIGHT) == LOW);

  float steerTarget = 0.0f;
  if (leftPressed)  steerTarget = -0.25f;   // lean left  (~14 deg)
  if (rightPressed) steerTarget =  0.25f;   // lean right (~14 deg)

  // 0.12 = how quickly lean follows input (higher = snappier)
  smoothSteer += (steerTarget - smoothSteer) * 0.12f;
  float rotY = smoothSteer;

  float pitch = 0.28f + smoothPitch;

  // Shadow (centered under car, no playerX offset needed)
  int shadowX  = centerX;
  int shadowY  = SCR_H - 18;
  int shadowRx = 42;
  int shadowRy = 5;
  uint16_t shadowCol = rgb(10, 10, 10);
  for (int dy = -shadowRy; dy <= shadowRy; dy++) {
    float t  = (float)dy / shadowRy;
    int   hw = (int)(shadowRx * sqrtf(1.0f - t * t));
    int   sy = shadowY + dy;
    if (sy < 0 || sy >= SCR_H) continue;
    int x0 = max(0, shadowX - hw);
    int x1 = min(SCR_W - 1, shadowX + hw);
    if (x1 > x0) spr.drawFastHLine(x0, sy, x1 - x0, shadowCol);
  }

  renderCar2Mesh(centerX, centerY, rotY, pitch, 6.5f, 130.0f);
}

void drawTrackTransition(int newTrack, float t) {
uint16_t bgTop = (newTrack == 1) ? rgb(  0,  50, 130)
                 : (newTrack == 2) ? rgb(  5,  60,   5)
                 : (newTrack == 3) ? rgb( 80,  90, 120)
                 :                   rgb( 15,  10,  40);
  uint16_t bgBot = (newTrack == 1) ? rgb(  0, 120, 220)
                 : (newTrack == 2) ? rgb( 20, 140,  20)
                 : (newTrack == 3) ? rgb(160, 175, 210)
                 :                   rgb( 50,  30,  90);

  for (int y = 0; y < SCR_H; y++) {
    float ft = (float)y / SCR_H;
    spr.drawFastHLine(0, y, SCR_W, lerpCol(bgTop, bgBot, ft));
  }
  for (int y = 0; y < SCR_H; y += 6)
    spr.drawFastHLine(0, y, SCR_W, darkenCol(bgTop, 0.65f));

  spr.setTextSize(2);
  spr.setTextColor(TFT_YELLOW);
  spr.setCursor(SCR_CX - 90, 22);
  spr.print("TRACK COMPLETE!");

  spr.setTextSize(2);
if (newTrack == 1) {
    spr.setTextColor(rgb(80, 210, 255));
    spr.setCursor(SCR_CX - 84, 50);
    spr.print("OCEAN CIRCUIT");
  } else if (newTrack == 2) {
    spr.setTextColor(rgb(80, 220, 80));
    spr.setCursor(SCR_CX - 74, 50);
    spr.print("GRASS CIRCUIT");
  } else if (newTrack == 3) {
    spr.setTextColor(rgb(200, 220, 255));
    spr.setCursor(SCR_CX - 84, 50);
    spr.print("WINTER CIRCUIT");
  } else {
    spr.setTextColor(rgb(255, 200, 50));
    spr.setCursor(SCR_CX - 74, 50);
    spr.print("CITY CIRCUIT");
  }

  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE);
  spr.setCursor(SCR_CX - 46, 80);
  spr.print("BEST LAP:  ");
  if (bestLapTime > 0.0f) {
    int mins = (int)bestLapTime / 60;
    int secs = (int)bestLapTime % 60;
    int decs = (int)((bestLapTime - (int)bestLapTime) * 100);
    if (mins > 0) { spr.print(mins); spr.print(":"); }
    spr.print(secs); spr.print(".");
    if (decs < 10) spr.print("0");
    spr.print(decs);
  } else {
    spr.print("--");
  }

  // Minimap preview
  {
    const int mx = SCR_CX - 44, my = 96, mw = 88, mh = 88;
    float rangeX = (float)(mapMaxX - mapMinX);
    float rangeY = (float)(mapMaxY - mapMinY);
    if (rangeX > 1.0f && rangeY > 1.0f) {
      const int pad = 5;
      float sx2 = (float)(mw - pad * 2) / rangeX;
      float sy2 = (float)(mh - pad * 2) / rangeY;
      float s   = (sx2 < sy2) ? sx2 : sy2;
      int drawW = (int)(rangeX * s);
      int drawH = (int)(rangeY * s);
      int offX  = mx + pad + (mw - pad * 2 - drawW) / 2;
      int offY  = my + pad + (mh - pad * 2 - drawH) / 2;

      for (int i = 0; i < TOTAL_SEGS; i++) {
        int j  = (i + 1) % TOTAL_SEGS;
        int x1 = offX + (int)((mapPtsX[i] - mapMinX) * s);
        int y1 = offY + (int)((mapPtsY[i] - mapMinY) * s);
        int x2 = offX + (int)((mapPtsX[j] - mapMinX) * s);
        int y2 = offY + (int)((mapPtsY[j] - mapMinY) * s);
        uint16_t lc = segments[i].tunnel ? rgb(160, 160, 255) : TFT_WHITE;
        spr.drawLine(x1, y1, x2, y2, lc);
      }

      int sx0 = offX + (int)((mapPtsX[0] - mapMinX) * s);
      int sy0 = offY + (int)((mapPtsY[0] - mapMinY) * s);
      spr.fillCircle(sx0, sy0, 4, TFT_GREEN);
      spr.drawCircle(sx0, sy0, 4, TFT_WHITE);
    }
  }

  if (t > 0.4f) {
    bool blink = ((int)(t * 5.0f)) % 2 == 0;
    if (blink) {
      spr.setTextSize(2);
      spr.setTextColor(TFT_WHITE);
      spr.setCursor(SCR_CX - 62, 207);
      spr.print("GET READY!");
    }
  }

  spr.pushSprite(0, 0);
}

void drawResultsScreen() {
  spr.fillSprite(TFT_BLACK);

  // Header
  spr.setTextSize(2);
  spr.setTextColor(TFT_YELLOW);
  spr.setCursor(SCR_CX - 80, 5);
  spr.print("RACE RESULTS");

  // Column headers
  spr.setTextSize(1);
  spr.setTextColor(rgb(140, 140, 140));
  spr.setCursor(2,   28); spr.print("TRACK");
  spr.setCursor(74,  28); spr.print("YOU");
  spr.setCursor(120, 28); spr.print("RIVAL");
  spr.setCursor(174, 28); spr.print("BEST-U");
  spr.setCursor(248, 28); spr.print("BEST-R");
  spr.drawFastHLine(0, 37, SCR_W, rgb(60, 60, 60));

  const char* trackNames[4] = { "CITY", "OCEAN", "GRASS", "WINTER" };
  int pTotal = 0, oTotal = 0;
  int y = 42;

  for (int t = 0; t < NUM_TRACKS; t++) {
    int   pWins = 0, oWins = 0;
    float pBest = 9999.0f, oBest = 9999.0f;

    for (int l = 0; l < MAX_LAPS_PER_TRACK; l++) {
      if (raceResults[t][l].winner == 0) pWins++;
      else if (raceResults[t][l].winner == 1) oWins++;
      if (raceResults[t][l].playerTime   > 0.1f && raceResults[t][l].playerTime   < pBest) pBest = raceResults[t][l].playerTime;
      if (raceResults[t][l].opponentTime > 0.1f && raceResults[t][l].opponentTime < oBest) oBest = raceResults[t][l].opponentTime;
    }
    pTotal += pWins;
    oTotal += oWins;

    // Track name
    spr.setTextColor(rgb(200, 200, 200));
    spr.setCursor(2, y); spr.print(trackNames[t]);

    // Player laps won
    spr.setCursor(74, y);
    spr.setTextColor(pWins > oWins ? TFT_GREEN : (pWins < oWins ? TFT_RED : TFT_YELLOW));
    spr.print(pWins); spr.print("/4");

    // Rival laps won
    spr.setCursor(120, y);
    spr.setTextColor(oWins > pWins ? rgb(255,80,80) : (oWins < pWins ? rgb(100,220,100) : TFT_YELLOW));
    spr.print(oWins); spr.print("/4");

    // Player best lap
    spr.setCursor(174, y);
    spr.setTextColor(TFT_CYAN);
    if (pBest < 9999.0f) {
      int ps = (int)pBest; int pd = (int)((pBest - ps) * 100);
      spr.print(ps); spr.print("."); if (pd < 10) spr.print("0"); spr.print(pd);
    } else { spr.print("--.-"); }

    // Rival best lap
    spr.setCursor(248, y);
    spr.setTextColor(rgb(255, 100, 100));
    if (oBest < 9999.0f) {
      int os2 = (int)oBest; int od = (int)((oBest - os2) * 100);
      spr.print(os2); spr.print("."); if (od < 10) spr.print("0"); spr.print(od);
    } else { spr.print("--.-"); }

    y += 14;
  }

  spr.drawFastHLine(0, y + 3, SCR_W, rgb(60, 60, 60));
  y += 10;

  // Totals
  spr.setTextSize(1);
  spr.setCursor(2, y);  spr.setTextColor(TFT_WHITE);   spr.print("LAPS WON:");
  spr.setCursor(74, y); spr.setTextColor(pTotal > oTotal ? TFT_GREEN : TFT_WHITE);
  spr.print(pTotal); spr.print("/"); spr.print(NUM_TRACKS * MAX_LAPS_PER_TRACK);
  spr.setCursor(120, y); spr.setTextColor(oTotal > pTotal ? rgb(255,80,80) : TFT_WHITE);
  spr.print(oTotal); spr.print("/"); spr.print(NUM_TRACKS * MAX_LAPS_PER_TRACK);
  y += 18;

  // Winner
  spr.setTextSize(2);
  if (pTotal > oTotal) {
    spr.setTextColor(TFT_GREEN);
    spr.setCursor(SCR_CX - 64, y); spr.print("YOU WIN!");
  } else if (oTotal > pTotal) {
    spr.setTextColor(TFT_RED);
    spr.setCursor(SCR_CX - 84, y); spr.print("RIVAL WINS!");
  } else {
    spr.setTextColor(TFT_YELLOW);
    spr.setCursor(SCR_CX - 76, y); spr.print("IT'S A TIE!");
  }

  // Blinking prompt
  if (((millis() / 500) % 2) == 0) {
    spr.setTextSize(1);
    spr.setTextColor(TFT_WHITE);
    spr.setCursor(SCR_CX - 72, SCR_H - 14);
    spr.print("Press button to play again");
  }

  spr.pushSprite(0, 0);
}

// ---------------------------------------------------------------------------
// Start screen and crash message 
// ---------------------------------------------------------------------------
void drawStartScreen(float time) {
  spr.fillSprite(TFT_BLACK);

  spr.fillRect(0, 100, SCR_W, 3, TFT_RED);
  spr.fillRect(0, 105, SCR_W, 3, TFT_WHITE);

  spr.setTextColor(TFT_RED);
  spr.setTextSize(3);
  spr.setCursor(22, 115);
  spr.print("OUTRUN ESP32");

  spr.setTextSize(2);
  spr.setTextColor(TFT_YELLOW);
  spr.setCursor(60, 145);
  spr.print("3D RACING");

  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE);
  spr.setCursor(5, 163);
  spr.print("Use left and right buttons to steer");
  spr.setCursor(5, 176);
  spr.print("Shift with joystick in an H pattern.");
  spr.setCursor(5, 189);
  spr.print("Hit up button for TURBO.");

  // ── H-pattern gear diagram (right side, compact) ───────────
  {
    const int16_t bx = 242, by = 158, bw = 75, bh = 48;
    const int16_t lx = bx + 14;
    const int16_t rx = bx + bw - 14;
    const int16_t ty = by + 12;
    const int16_t gy = by + bh - 12;
    const int16_t my = (ty + gy) / 2;

    spr.fillRect(bx, by, bw, bh, rgb(20, 20, 20));
    spr.drawRect(bx, by, bw, bh, rgb(80, 80, 80));

    uint16_t rc = rgb(190, 190, 190);
    spr.drawFastVLine(lx, ty, gy - ty, rc);
    spr.drawFastVLine(rx, ty, gy - ty, rc);
    spr.drawFastHLine(lx, my, rx - lx + 1, rc);

    spr.fillCircle(lx, ty, 3, TFT_WHITE);
    spr.fillCircle(lx, gy, 3, TFT_WHITE);
    spr.fillCircle(rx, ty, 3, TFT_WHITE);
    spr.fillCircle(rx, gy, 3, TFT_WHITE);

    spr.setTextSize(1);
    spr.setTextColor(TFT_YELLOW);
    spr.setCursor(lx - 7, by + 3);       spr.print("1");
    spr.setCursor(lx - 7, by + bh - 10); spr.print("2");
    spr.setCursor(rx + 3,  by + 3);       spr.print("3");
    spr.setCursor(rx + 3,  by + bh - 10); spr.print("4");
  }

  spr.fillEllipse(SCR_CX, 95, 55, 18, rgb(15, 15, 15));
  renderCar2Mesh(SCR_CX, 75, time * 1.5f, 0.5f, 5.0f, 130.0f);

  spr.pushSprite(0, 0);
}

void drawCrashMessage() {
  spr.fillRect(SCR_CX - 70, SCR_CY - 15, 140, 30, TFT_BLACK);
  spr.drawRect(SCR_CX - 71, SCR_CY - 16, 142, 32, TFT_RED);
  spr.setTextSize(3);
  spr.setTextColor(TFT_RED, TFT_BLACK);
  spr.setCursor(SCR_CX - 55, SCR_CY - 8);
  spr.print("CRASH!");
}
