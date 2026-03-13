/*
  ═══════════════════════════════════════════════════════════════
  ESP32 Pseudo-3D Racing Game — TFT_eSPI (Double Buffered)
  ═══════════════════════════════════════════════════════════════
*/

#include <SPI.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "structs.h"
#include "colors.h"
#include "utils.h"
#include "track.h"
#include "rendering.h"
#include "physics.h"

int  timeOfDay          = 0;
long distSinceTimeChange = 0;

void fatalHalt(const char* msg) {
  Serial.println("══════════════════════════════");
  Serial.print("FATAL ERROR: ");
  Serial.println(msg);
  Serial.println("System halted. Fix error and reflash.");
  Serial.println("══════════════════════════════");
  while (true) {
    delay(500);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(JOYSTICK_Z, INPUT_PULLUP);
  analogReadResolution(12);
  pinMode(BTN_TURBO, INPUT_PULLUP);

  Serial.println("══════════════════════════════");
  Serial.println("  ESP32 Racing Game Boot");
  Serial.println("══════════════════════════════");

  if (!psramFound()) {
    fatalHalt("No PSRAM detected! Check board has PSRAM and 'PSRAM: OPI PSRAM' is set in Tools menu.");
  }

  Serial.print("Total PSRAM: "); Serial.println(ESP.getPsramSize());
  Serial.print("Free PSRAM:  "); Serial.println(ESP.getFreePsram());

  if (ESP.getPsramSize() == 0) {
    fatalHalt("PSRAM size is 0! Hardware or board config issue.");
  }

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  randomSeed(analogRead(0));

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  Serial.println("[1/7] Initializing display...");
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("      Display OK");

  Serial.println("[2/7] Creating sprite (double buffer)...");
  Serial.print("      Sprite will need ~");
  Serial.print((SCR_W * SCR_H * 2) / 1024);
  Serial.println(" KB of PSRAM");

  spr.setColorDepth(16);
  spr.setAttribute(PSRAM_ENABLE, true);

  void* sprPtr = spr.createSprite(SCR_W, SCR_H);
  if (sprPtr == nullptr) {
    Serial.print("      Free PSRAM at time of failure: ");
    Serial.println(ESP.getFreePsram());
    fatalHalt("Sprite creation failed! Not enough PSRAM.");
  }
  Serial.println("      Sprite OK");
  Serial.print("      Free PSRAM after sprite: ");
  Serial.println(ESP.getFreePsram());

  Serial.println("[3/7] Initializing physics...");
  initPhysics();
  Serial.println("      Physics OK");

  Serial.println("[4/7] Initializing colors...");
  initColors(timeOfDay);
  Serial.println("      Colors OK");

  Serial.println("[5/7] Initializing background (parallax)...");
  size_t psramBefore = ESP.getFreePsram();
  initBackground();
  size_t psramAfter = ESP.getFreePsram();
  Serial.print("      Background used ");
  Serial.print((psramBefore - psramAfter) / 1024);
  Serial.println(" KB of PSRAM");
  Serial.println("      Background OK");

  Serial.println("[6/7] Building track...");
  buildTrack();
  Serial.println("      Track OK");

  Serial.println("[7/7] Initializing traffic...");
  initTraffic(maxSpeed);
  Serial.println("      Traffic OK");

  Serial.println("══════════════════════════════");
  Serial.println("  All systems initialized OK");
  Serial.print(  "  Final free PSRAM: ");
  Serial.println(ESP.getFreePsram());
  Serial.println("══════════════════════════════");

  unsigned long startTime = millis();
  while (millis() - startTime < 6000) {
    float animTime = (millis() - startTime) * 0.001f;
    drawStartScreen(animTime);
    delay(16);
  }

  lastFrameMs          = millis();
  distSinceTimeChange  = 0;
}

void loop() {

  // ── Handle pending track switch ──────────────────────────────
// ── Handle pending track switch ──────────────────────────────
  if (trackSwitchPending) {
    trackSwitchPending  = false;

    // Switch track and rebuild everything FIRST
    // so the transition screen can show the new minimap
    switchToNextTrack(maxSpeed);
    position            = 0.0f;
    prevPosition        = 0.0f;
    currentLapTime      = 0.0f;
    timeOfDay           = 0;
    distSinceTimeChange = 0;
    speed               = 0.0f;
    velocityX           = 0.0f;

if (currentTrack == 1) {
      initColors(0, 1);
      rebuildBackground(1);
    } else if (currentTrack == 2) {
      initColors(0, 2);
      rebuildBackground(2);
    } else {
      initColors(0, 0);
      rebuildBackground(0);
    }

    // Transition splash — 4 seconds
    unsigned long transStart = millis();
    while (millis() - transStart < 4000) {
      float t = (float)(millis() - transStart) / 4000.0f;
      drawTrackTransition(currentTrack, t);
      delay(16);
    }

    lastFrameMs = millis();   // Reset dt so first frame isn't huge
  }

  unsigned long now = millis();
  float dt = (now - lastFrameMs) / 1000.0f;
  lastFrameMs = now;

  if (dt <= 0.0f) dt = 0.001f;
  if (dt >  0.05f) dt = 0.05f;

  // ── Update game if not crashed ────────────────────────────────
  if (!crashed) {
    handleInput(dt);
    updatePhysics(dt);
    checkCollisions();

    int pSeg = findSegIdx(position + playerZdist);
    if (pSeg >= 0 && pSeg < TOTAL_SEGS) {
      float curveForce = segments[pSeg].curve;
      skyOffset += curveForce * (speed / maxSpeed) * 150.0f * dt;
    }
  }

  // ── Render frame ─────────────────────────────────────────────
  drawSky(position, playerZdist, timeOfDay, skyOffset);
  drawRoad(position, playerX, playerZdist, cameraDepth, timeOfDay);
  drawPlayerCar();
  drawHUD(speed, maxSpeed, currentLapTime, bestLapTime);

  // ── Crash overlay ─────────────────────────────────────────────
  if (crashed) {
    drawCrashMessage();
    if (millis() - crashTimer > 2000) {
      recoverFromCrash();
    }
  }

  // ── Push frame to display ─────────────────────────────────────
  if (spr.created()) {
    spr.pushSprite(0, 0);
  }

  // ── Time of day cycle (normal theme only) ────────────────────
  if (trackTheme == 0) {
    distSinceTimeChange += (long)(speed * dt);
    if (distSinceTimeChange > 180000) {
      distSinceTimeChange = 0;
      timeOfDay = (timeOfDay + 1) % 3;
      initColors(timeOfDay, 0);
    }
  }
}