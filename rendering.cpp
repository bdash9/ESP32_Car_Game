/*
  ═══════════════════════════════════════════════════════════════
  RENDERING IMPLEMENTATION - MAIN MODULE
  Coordinator of rendering submodules
  ═══════════════════════════════════════════════════════════════
*/

#include "rendering.h"
#include "config.h"
#include "colors.h"
#include "utils.h"

#include "render_player.h"
#include "render_traffic.h"
#include "render_road.h"
#include "render_hud.h"

// ═══════════════════════════════════════════════════════════════
//  GLOBAL VARIABLES
// ═══════════════════════════════════════════════════════════════
TFT_eSPI    tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

TFT_eSprite bgSpr = TFT_eSprite(&tft);
float skyOffset = 0.0f;
bool  bgCreated = false;

RenderPt rCache[DRAW_DIST];
int16_t  rClip[DRAW_DIST];

// ═══════════════════════════════════════════════════════════════
//  BACKGROUND REBUILD
// ═══════════════════════════════════════════════════════════════

void rebuildBackground(int theme) {
  if (!bgCreated) return;

  if (theme == 1) {
    // ── Ocean track: bright azure sky ──────────────────────────
    for (int y = 0; y < SCR_CY; y++) {
      float    t      = (float)y / SCR_CY;
      uint16_t skyCol = lerpCol(rgb(20, 150, 255), rgb(135, 205, 255), t);
      if (y > SCR_CY * 0.82f) {
        float t2 = (float)(y - SCR_CY * 0.82f) / (SCR_CY * 0.18f);
        skyCol = lerpCol(rgb(135, 205, 255), rgb(195, 230, 255), t2);
      }
      bgSpr.drawFastHLine(0, y, SCR_W * 2, skyCol);
    }

    // Bright high sun
    bgSpr.fillCircle(SCR_W + 60, 14, 16, rgb(255, 248, 180));
    bgSpr.fillCircle(SCR_W + 60, 14, 10, rgb(255, 255, 230));

    // White resort building silhouette on horizon
    int x = 0;
    while (x < SCR_W * 2) {
      int w = random(8, 24);
      int h = random(8, 32);
      bgSpr.fillRect(x, SCR_CY - h, w, h, rgb(225, 228, 235));
      x += w + random(1, 6);
    }

  } else if (theme == 2) {
    // ── Grass track: bright blue sky, green rolling horizon ────
    for (int y = 0; y < SCR_CY; y++) {
      float    t      = (float)y / SCR_CY;
      uint16_t skyCol = lerpCol(rgb(60, 160, 255), rgb(140, 210, 255), t);
      bgSpr.drawFastHLine(0, y, SCR_W * 2, skyCol);
    }

    // Bright sun
    bgSpr.fillCircle(SCR_W / 2, 18, 18, rgb(255, 255, 180));
    bgSpr.fillCircle(SCR_W / 2, 18, 12, rgb(255, 255, 220));

    // Rolling green hill silhouette
    for (int x = 0; x < SCR_W * 2; x++) {
      float wave = sinf(x * 0.018f) * 12.0f + sinf(x * 0.007f) * 8.0f;
      int   h    = (int)(20 + wave);
      bgSpr.drawFastVLine(x, SCR_CY - h, h, rgb(40, 140, 30));
    }

    // Scattered distant trees on horizon
    int x = 0;
    while (x < SCR_W * 2) {
      int h = random(18, 38);
      int w = random(6, 14);
      bgSpr.fillRect(x, SCR_CY - h, w, h, rgb(20, 90, 20));
      bgSpr.fillTriangle(x - 4,    SCR_CY - h,
                         x + w / 2, SCR_CY - h - 14,
                         x + w + 4, SCR_CY - h, rgb(30, 110, 30));
      x += w + random(8, 30);
    }

  } else {
    // ── Default track: city sunset sky ─────────────────────────
    for (int y = 0; y < SCR_CY; y++) {
      float    t      = (float)y / SCR_CY;
      uint16_t skyCol = lerpCol(rgb(40, 40, 80), rgb(150, 100, 150), t);
      if (y > SCR_CY * 0.7f) {
        float t2 = (float)(y - SCR_CY * 0.7f) / (SCR_CY * 0.3f);
        skyCol = lerpCol(rgb(150, 100, 150), rgb(255, 180, 100), t2);
      }
      bgSpr.drawFastHLine(0, y, SCR_W * 2, skyCol);
    }

    // Setting sun
    bgSpr.fillCircle(SCR_W, SCR_CY - 15, 20, rgb(255, 100,  50));
    bgSpr.fillCircle(SCR_W, SCR_CY - 15, 15, rgb(255, 150,  50));

    // Back layer: dark silhouette buildings
    int x = 0;
    while (x < SCR_W * 2) {
      int w = random(10, 30);
      int h = random(20, 50);
      bgSpr.fillRect(x, SCR_CY - h, w, h, rgb(30, 30, 50));
      x += w;
    }

    // Front layer: taller buildings with lit windows
    x = 0;
    while (x < SCR_W * 2) {
      int w = random(15, 40);
      int h = random(30, 80);
      bgSpr.fillRect(x, SCR_CY - h, w, h, rgb(20, 20, 40));
      if (w > 10 && h > 10) {
        for (int wy = SCR_CY - h + 5; wy < SCR_CY - 2; wy += 8)
          for (int wx = x + 3; wx < x + w - 3; wx += 6)
            if (random(0, 10) > 3)
              bgSpr.drawPixel(wx, wy, rgb(80, 80, 100));
      }
      x += w;
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  INIT BACKGROUND (called once at startup)
// ═══════════════════════════════════════════════════════════════

void initBackground() {
  bgSpr.setColorDepth(16);
  bgSpr.setAttribute(PSRAM_ENABLE, true);

  if (bgSpr.createSprite(SCR_W * 2, SCR_CY) == nullptr) {
    Serial.println("ERROR: Failed to create bgSpr in PSRAM!");
    bgCreated = false;
  } else {
    Serial.println("bgSpr created in PSRAM successfully.");
    bgCreated = true;
  }

  rebuildBackground(0);
}