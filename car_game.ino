/*
  ═══════════════════════════════════════════════════════════════
  ESP32 Pseudo-3D Racing Game — TFT_eSPI (Double Buffered)

  MODULAR STRUCTURE:
  - config.h       : Hardware constants and definitions
  - structs.h      : Data structures (Segment, RenderPt, TrafficCar)
  - colors.cpp/h   : Color management and palettes
  - utils.cpp/h    : Math utility functions
  - track.cpp/h    : Track generation and traffic
  - rendering.cpp/h: Drawing and rendering functions
  - physics.cpp/h  : Game physics and collisions
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

// ═══════════════════════════════════════════════════════════════
//  VARIABLES DE CONTROL DE TIEMPO Y DÍA/NOCHE
// ═══════════════════════════════════════════════════════════════
int  timeOfDay = 0;            // 0=día, 1=atardecer, 2=noche
long distSinceTimeChange = 0;

// ═══════════════════════════════════════════════════════════════
//  HELPER: Fatal halt with message (stops boot loop)
// ═══════════════════════════════════════════════════════════════
void fatalHalt(const char* msg) {
  Serial.println("══════════════════════════════");
  Serial.print("FATAL ERROR: ");
  Serial.println(msg);
  Serial.println("System halted. Fix error and reflash.");
  Serial.println("══════════════════════════════");
  // Blink onboard LED if available to signal fatal state
  // visually instead of silently boot-looping
  while (true) {
    delay(500);
  }
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500); // Give serial monitor time to connect before any output

// Joystick
  pinMode(JOYSTICK_Z, INPUT_PULLUP);  // Z button (reserved)
  analogReadResolution(12);           // 12-bit ADC: 0-4095 for joystick X/Y

  pinMode(BTN_TURBO, INPUT_PULLUP);   // KEY1 turbo button

  Serial.println("══════════════════════════════");
  Serial.println("  ESP32 Racing Game Boot");
  Serial.println("══════════════════════════════");

  // ── FIX 1: Verify PSRAM is physically present FIRST ──────────
  // If PSRAM is not found, every ps_malloc will return nullptr,
  // causing a StoreProhibited crash when the code writes to it.
  if (!psramFound()) {
    fatalHalt("No PSRAM detected! Check board has PSRAM and 'PSRAM: OPI PSRAM' is set in Tools menu.");
  }

  // ── Print PSRAM stats BEFORE allocating anything ─────────────
  Serial.print("Total PSRAM: "); Serial.println(ESP.getPsramSize());
  Serial.print("Free PSRAM:  "); Serial.println(ESP.getFreePsram());

  if (ESP.getPsramSize() == 0) {
    fatalHalt("PSRAM size is 0! Hardware or board config issue.");
  }

  // ── Configure button pins ─────────────────────────────────────
  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  randomSeed(analogRead(0));

  // ── Configure display backlight ───────────────────────────────
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // ── Initialize TFT display ────────────────────────────────────
  Serial.println("[1/7] Initializing display...");
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("      Display OK");

  // ── FIX 2: Create sprite with full PSRAM check ───────────────
  // Previously: failure was detected but execution continued,
  // leading to writes through a null internal buffer pointer.
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
    fatalHalt("Sprite creation failed! Not enough PSRAM. Reduce SCR_W/SCR_H or free PSRAM.");
  }
  Serial.println("      Sprite OK");
  Serial.print("      Free PSRAM after sprite: ");
  Serial.println(ESP.getFreePsram());

  // ── Initialize physics ────────────────────────────────────────
  Serial.println("[3/7] Initializing physics...");
  initPhysics();
  Serial.println("      Physics OK");

  // ── Initialize colors ─────────────────────────────────────────
  Serial.println("[4/7] Initializing colors...");
  initColors(timeOfDay);
  Serial.println("      Colors OK");

  // ── FIX 3: Wrap initBackground with PSRAM guard ───────────────
  // initBackground() likely calls ps_malloc internally.
  // We check free PSRAM before and after to detect silent failures.
  Serial.println("[5/7] Initializing background (parallax mountains)...");
  size_t psramBefore = ESP.getFreePsram();

  initBackground(); // ← If this crashes, you will see "[5/7]" but NOT "[6/7]"

  size_t psramAfter = ESP.getFreePsram();
  Serial.print("      Background used ");
  Serial.print((psramBefore - psramAfter) / 1024);
  Serial.println(" KB of PSRAM");
  Serial.println("      Background OK");

  // ── Build track ───────────────────────────────────────────────
  Serial.println("[6/7] Building track...");
  buildTrack();
  Serial.println("      Track OK");

  // ── Initialize traffic ────────────────────────────────────────
  Serial.println("[7/7] Initializing traffic...");
  initTraffic(maxSpeed);
  Serial.println("      Traffic OK");

  Serial.println("══════════════════════════════");
  Serial.println("  All systems initialized OK");
  Serial.print(  "  Final free PSRAM: ");
  Serial.println(ESP.getFreePsram());
  Serial.println("══════════════════════════════");

  // ── Show start screen with rotating car (3 seconds) ──────────
  unsigned long startTime = millis();
  while (millis() - startTime < 3000) {
    float animTime = (millis() - startTime) * 0.001f;
    drawStartScreen(animTime);
    delay(16); // ~60 FPS
  }

  lastFrameMs = millis();
  distSinceTimeChange = 0;
}

// ═══════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
/*
// ── THROTTLED DEBUG: prints only once every 500ms ────────────
  static unsigned long lastDebugMs = 0;
  if (millis() - lastDebugMs > 500) {
    lastDebugMs = millis();
    Serial.print("BTN_LEFT: ");
    Serial.print(digitalRead(BTN_LEFT));
    Serial.print("  BTN_RIGHT: ");
    Serial.print(digitalRead(BTN_RIGHT));
    Serial.print("  playerX: ");
    Serial.println(playerX);
  }
*/
  unsigned long now = millis();
  float dt = (now - lastFrameMs) / 1000.0f;
  lastFrameMs = now;

  // ── FIX 4: Tighter dt clamp prevents position explosion ───────
  // A very large dt on first frame (e.g. after slow setup) can
  // send playerX or position out of bounds on the first loop tick.
  if (dt <= 0.0f) dt = 0.001f;   // Guard against zero/negative dt
  if (dt >  0.05f) dt = 0.05f;   // Cap at 50ms (~20 FPS minimum)

  // ── Update game if not crashed ────────────────────────────────
  if (!crashed) {
    handleInput(dt);
    updatePhysics(dt);
    checkCollisions();

    // Parallax sky scroll based on curve and speed
    int pSeg = findSegIdx(position + playerZdist);

    // ── FIX 5: Guard segment index before dereferencing ──────────
    // An out-of-range index into segments[] is another common
    // cause of StoreProhibited / LoadProhibited crashes.
        if (pSeg >= 0 && pSeg < TOTAL_SEGS) {
        float curveForce = segments[pSeg].curve;
      skyOffset += curveForce * (speed / maxSpeed) * 150.0f * dt;
    }
  }

  // ── Render frame into sprite (back buffer) ────────────────────
  drawSky(position, playerZdist, timeOfDay, skyOffset);
  drawRoad(position, playerX, playerZdist, cameraDepth, timeOfDay);
  drawPlayerCar();
  drawHUD(speed, maxSpeed, currentLapTime, bestLapTime);

// Show crash overlay
  if (crashed) {
    drawCrashMessage();
    if (millis() - crashTimer > 2000) {
      recoverFromCrash();   // ← replaces the old 3-line inline block
    }
  }

  // ── FIX 6: Guard pushSprite — only call if sprite is valid ────
  // If for any reason the sprite was not created, this call would
  // dereference a null pointer and cause StoreProhibited.
  if (spr.created()) {
    spr.pushSprite(0, 0);
  }

  // ── Cycle time of day based on distance traveled ──────────────
  distSinceTimeChange += (long)(speed * dt);
  if (distSinceTimeChange > 180000) {
    distSinceTimeChange = 0;
    timeOfDay = (timeOfDay + 1) % 3;
    initColors(timeOfDay);
  }
}