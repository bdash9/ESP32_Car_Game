/*
  ═══════════════════════════════════════════════════════════════
  PHYSICS AND LOGIC IMPLEMENTATION
  ═══════════════════════════════════════════════════════════════
*/

#include "physics.h"
#include "track.h"
#include "utils.h"
#include "config.h"
#include <Arduino.h>

// ── Global variables ──────────────────────────────────────────
float cameraDepth;
float playerZdist;
float position    = 0;
float playerX     = 0;
float speed       = 0;
float maxSpeed;
float centrifugal = CENTRIFUGAL;

bool          crashed         = false;
unsigned long crashTimer      = 0;
unsigned long lastFrameMs     = 0;
unsigned long invincibleUntil = 0;

float currentLapTime = 0;
float lastLapTime    = 0;
float bestLapTime    = 0;
float prevPosition   = 0;
int   currentLap     = 1;
int   totalLaps      = 3;

float velocityX    = 0;
float acceleration = 0;
float driftAngle   = 0;

// ── Gear system ───────────────────────────────────────────────
int   currentGear = 0;
float rpm         = 800.0f;

static const float GEAR_SPEED_CAP[NUM_GEARS] = {
  0.25f, 0.50f, 0.75f, 1.00f
};
static const float GEAR_RPM_BASE[NUM_GEARS] = {
  0.000f, 0.125f, 0.300f, 0.550f
};

// ── Turbo system ──────────────────────────────────────────────
float turboCharge   = 0.0f;
float turboTimeLeft = 0.0f;
bool  turboActive   = false;

// ── Init ──────────────────────────────────────────────────────
void initPhysics() {
  float fovRad = FOV_DEG * PI / 180.0f;
  cameraDepth  = 1.0f / tanf(fovRad / 2.0f);
  playerZdist  = CAM_HEIGHT * cameraDepth;
  maxSpeed     = SEG_LEN * SPEED_MULTIPLIER;

  position        = 0;
  playerX         = 0;
  speed           = 0;
  crashed         = false;
  currentLapTime  = 0;
  lastLapTime     = 0;
  bestLapTime     = 0;
  prevPosition    = 0;
  velocityX       = 0;
  acceleration    = 0;
  driftAngle      = 0;
  invincibleUntil = 0;
  currentGear     = 0;
  rpm             = 800.0f;
  turboCharge     = 0.0f;
  turboTimeLeft   = 0.0f;
  turboActive     = false;
}

// ── Crash recovery ────────────────────────────────────────────
void recoverFromCrash() {
  crashed      = false;
  speed        = maxSpeed * POST_CRASH_SPEED;
  acceleration = maxSpeed * POST_CRASH_SPEED;
  playerX      = 0.0f;
  velocityX    = 0.0f;

  // Drop to 1st gear and lose all turbo progress on crash
  currentGear   = 0;
  rpm           = 1000.0f;
  turboCharge   = 0.0f;
  turboActive   = false;
  turboTimeLeft = 0.0f;

  invincibleUntil = millis() + 2000;

  int playerSeg = findSegIdx(position);
  for (int i = 0; i < MAX_CARS; i++) {
    TrafficCar& car = trafficCars[i];
    int carSeg = findSegIdx(car.z);
    int dist   = (carSeg - playerSeg + TOTAL_SEGS) % TOTAL_SEGS;
    bool tooClose = (dist < 15) || (dist > TOTAL_SEGS - 4);
    if (tooClose) {
      if (car.type == CAR_ONCOMING) {
        int s = (playerSeg + RESPAWN_ONCOMING_MIN + random(0, 15)) % TOTAL_SEGS;
        car.z = s * (float)SEG_LEN + random(0, SEG_LEN);
      } else {
        int s = (playerSeg + RESPAWN_AHEAD_MIN + random(0, 10)) % TOTAL_SEGS;
        car.z     = s * (float)SEG_LEN + random(0, SEG_LEN);
        car.speed = car.baseSpeed;
      }
    }
  }
}

// ── H-pattern gear shift ──────────────────────────────────────
//
//   X<LOW  │  X>HIGH
//  ─────────┼─────────
//  Y<LOW  [1]│[3]   top row
//  Y>HIGH [2]│[4]   bottom row
//
void readGearShift() {
  int jx = analogRead(JOYSTICK_X);
  int jy = analogRead(JOYSTICK_Y);

  bool xLeft  = (jx < JOY_LOW);
  bool xRight = (jx > JOY_HIGH);
  bool yTop   = (jy < JOY_LOW);
  bool yBot   = (jy > JOY_HIGH);

  static bool needsNeutral = false;
  if (!xLeft && !xRight) { needsNeutral = false; return; }
  if (needsNeutral) return;
  needsNeutral = true;

  if      (xLeft  && yTop) currentGear = 0;  // 1st — upper left
  else if (xLeft  && yBot) currentGear = 1;  // 2nd — lower left
  else if (xRight && yTop) currentGear = 2;  // 3rd — upper right
  else if (xRight && yBot) currentGear = 3;  // 4th — lower right
}

// ── Input ─────────────────────────────────────────────────────
void handleInput(float dt) {

  // Engine acceleration ramp
  float targetAccel = maxSpeed * ACCEL_TARGET;
  if (speed < maxSpeed * ACCEL_NEAR_MAX) {
    acceleration += ACCEL_RAMP * dt;
    if (acceleration > targetAccel) acceleration = targetAccel;
  } else {
    acceleration *= ACCEL_DAMPING;
  }
  if (acceleration < 0.0f) acceleration = 0.0f;

  // Steering
  bool leftPressed  = (digitalRead(BTN_LEFT)  == LOW);
  bool rightPressed = (digitalRead(BTN_RIGHT) == LOW);
  const float STEER_SPEED = 2.5f;
  if (leftPressed)  playerX -= STEER_SPEED * dt;
  if (rightPressed) playerX += STEER_SPEED * dt;
  playerX = clampF(playerX, -1.0f, 1.0f);

  // H-pattern gear shift
  readGearShift();

  // ── Turbo button (KEY1) — falling edge only ───────────────────
  // INPUT_PULLUP: LOW = pressed, HIGH = not pressed.
  // static bool stores previous state as true=not pressed, false=pressed.
  // Falling edge = turboPrev was HIGH (not pressed), now LOW (pressed).
  static bool turboPrev = true;   // true = not pressed
  bool turboCurr = (digitalRead(BTN_TURBO) == LOW);  // true = pressed

  if (turboPrev && turboCurr) {
    // Falling edge: was not pressed, now pressed → activate turbo
    if (turboCharge >= 1.0f && !turboActive) {
      turboActive   = true;
      turboTimeLeft = TURBO_DURATION;
      turboCharge   = 0.0f;
    }
  }
  turboPrev = !turboCurr;   // Save inverted: true = not pressed
}

// ── Physics update ────────────────────────────────────────────
void updatePhysics(float dt) {
  int pSeg       = findSegIdx(position + playerZdist);
  int prevSegIdx = (pSeg - 1 + TOTAL_SEGS) % TOTAL_SEGS;
  float spPct    = speed / maxSpeed;

  // Gravity (bounded direct speed delta)
  float slope     = (segments[pSeg].y - segments[prevSegIdx].y) / SEG_LEN;
  float gravityDv = -slope * (GRAVITY_FACTOR / 3.0f) * dt;
  gravityDv = clampF(gravityDv, -maxSpeed * 0.10f * dt, maxSpeed * 0.10f * dt);

  // Forward speed with friction
  speed *= FRICTION;
  speed += acceleration * dt;
  speed += gravityDv;

  // ── FIX: Turbo = flat +50 km/h added to current gear limit ───
  // Convert TURBO_SPEED_KMH (50) to game units.
  // maxSpeed = 300 km/h equivalent, so +50 km/h = +1/6 of maxSpeed.
  float turboSpeedAdd = maxSpeed * (TURBO_SPEED_KMH / 300.0f);

  // Gear speed limiter — bypassed during turbo so boost can exceed gear cap
  float gearLimit = GEAR_SPEED_CAP[currentGear] * maxSpeed;
  if (!turboActive && speed > gearLimit) {
    speed        = gearLimit;
    acceleration *= 0.7f;
  }

  // ── Turbo: tick down and push speed up by flat 50 km/h ───────
  if (turboActive) {
    turboTimeLeft -= dt;
    if (turboTimeLeft <= 0.0f) {
      turboActive   = false;
      turboTimeLeft = 0.0f;
    } else {
      // Target = current gear cap + flat 50 km/h bonus
      float turboTarget = gearLimit + turboSpeedAdd;
      // Ramp toward target quickly so boost feels instant
      speed += (turboTarget - speed) * 6.0f * dt;
      if (speed > turboTarget) speed = turboTarget;
    }
  }

  // ── Turbo charge build ────────────────────────────────────────
  // Only builds while driving clean — no crash, no active boost
  if (!crashed && !turboActive) {
    turboCharge += dt / TURBO_BUILD_TIME;
    if (turboCharge > 1.0f) turboCharge = 1.0f;
  }

  // Absolute speed cap
  float hardCap = turboActive ? (gearLimit + turboSpeedAdd) : maxSpeed;
  if (speed > hardCap) speed = hardCap;
  if (speed < 0.0f)    speed = 0.0f;

  // ── RPM calculation ───────────────────────────────────────────
  float gBase     = GEAR_RPM_BASE[currentGear];
  float gTop      = GEAR_SPEED_CAP[currentGear];
  float range     = gTop - gBase;
  float posInGear = (range > 0.001f) ? (spPct - gBase) / range : 0.0f;
  posInGear       = clampF(posInGear, 0.0f, 1.05f);
  rpm = 800.0f + posInGear * (REDLINE - 800.0f);

  // FIX: flat +500 RPM spike during turbo (was *1.15 multiplier
  // which barely moved the needle at low RPM)
  if (turboActive) rpm = min(rpm + 500.0f, MAX_RPM);
  rpm = clampF(rpm, 0.0f, MAX_RPM);

  // Centrifugal drift
  float curveForce = segments[pSeg].curve * centrifugal * spPct;
  velocityX += curveForce * dt;
  velocityX *= LATERAL_FRICTION;
  playerX   -= velocityX * dt * 0.3f;
  playerX    = clampF(playerX, -1.0f, 1.0f);

  // Visual drift angle
  if (speed > 0.1f) {
    driftAngle = atan2f(velocityX * 10.0f, speed / maxSpeed) * 0.5f;
    driftAngle = clampF(driftAngle, -0.5f, 0.5f);
  } else {
    driftAngle *= 0.9f;
  }

  // Advance position
  prevPosition = position;
  position     = loopIncrease(position, dt * speed, trackLength);

  // Lap detection
  if (position < prevPosition && prevPosition > trackLength * 0.9f) {
    if (currentLapTime > 5.0f) {
      lastLapTime = currentLapTime;
      if (bestLapTime <= 0.0f || currentLapTime < bestLapTime)
        bestLapTime = currentLapTime;
      if (currentLap < totalLaps) currentLap++;
      else currentLap = 1;
    }
    currentLapTime = 0;
  }
  currentLapTime += dt;

  // Traffic movement and behaviour
  int playerSeg = findSegIdx(position);
  for (int i = 0; i < MAX_CARS; i++) {
    TrafficCar& car = trafficCars[i];

    if (car.type == CAR_ONCOMING) {
      car.z -= dt * car.baseSpeed;
      if (car.z < 0.0f) car.z += trackLength;
    } else {
      car.z = loopIncrease(car.z, dt * car.speed, trackLength);
    }

    car.behaviorTimer--;
    switch (car.type) {
      case CAR_BRAKER:
        if (car.behaviorTimer <= 0) {
          if (car.speed > car.baseSpeed * 0.25f) {
            car.speed         = car.baseSpeed * 0.15f;
            car.behaviorTimer = random(30, 80);
          } else {
            car.speed         = car.baseSpeed;
            car.behaviorTimer = random(150, 400);
          }
        }
        break;
      case CAR_FAST:
        if (car.behaviorTimer <= 0) {
          car.offset       += (random(0, 2) == 0) ? -0.25f : 0.25f;
          car.offset        = clampF(car.offset, -0.65f, 0.65f);
          car.behaviorTimer = random(60, 180);
        }
        break;
      case CAR_MEDIUM:
        if (car.behaviorTimer <= 0) {
          car.offset       += random(-1, 2) * 0.15f;
          car.offset        = clampF(car.offset, -0.70f, 0.70f);
          car.behaviorTimer = random(150, 350);
        }
        break;
      default:
        if (car.behaviorTimer <= 0) car.behaviorTimer = random(300, 600);
        break;
    }

    int carSeg = findSegIdx(car.z);
    int behind = (playerSeg - carSeg + TOTAL_SEGS) % TOTAL_SEGS;
    if (behind > RESPAWN_BEHIND_SEGS && behind < TOTAL_SEGS / 2) {
      if (car.type == CAR_ONCOMING) {
        int s = (playerSeg + RESPAWN_ONCOMING_MIN
                 + random(0, RESPAWN_ONCOMING_MAX - RESPAWN_ONCOMING_MIN))
                % TOTAL_SEGS;
        car.z = s * (float)SEG_LEN + random(0, SEG_LEN / 2);
      } else {
        int s = (playerSeg + RESPAWN_AHEAD_MIN
                 + random(0, RESPAWN_AHEAD_MAX - RESPAWN_AHEAD_MIN))
                % TOTAL_SEGS;
        car.z     = s * (float)SEG_LEN + random(0, SEG_LEN / 2);
        car.speed = car.baseSpeed;
      }
    }
  }
}

// ── Collision detection ───────────────────────────────────────
void checkCollisions() {
  if (millis() < invincibleUntil) return;

  const float playerW = 0.15f;
  int pSeg   = findSegIdx(position);
  Segment& s = segments[pSeg];

  for (int i = 0; i < MAX_CARS; i++) {
    TrafficCar& car = trafficCars[i];

    float relZ = car.z - position;
    if (relZ >  trackLength / 2.0f) relZ -= trackLength;
    if (relZ < -trackLength / 2.0f) relZ += trackLength;
    if (relZ < 0.0f)              continue;
    if (relZ > SEG_LEN * 6.0f)    continue;
    if (speed < maxSpeed * 0.05f)  continue;
    if (!overlapChk(playerX, playerW, car.offset, 0.18f)) continue;

    if (car.type == CAR_ONCOMING) {
      speed      = 0.0f;
      velocityX  = 0.0f;
      crashed    = true;
      crashTimer = millis();
      int s2 = (pSeg + RESPAWN_ONCOMING_MIN + random(0, 15)) % TOTAL_SEGS;
      car.z = s2 * (float)SEG_LEN + random(0, SEG_LEN);

    } else if (speed > car.speed + maxSpeed * 0.05f) {
      float speedDiff = speed - car.speed;
      speed    = car.speed * 0.75f;
      position = loopIncrease(position, -(speed * 0.05f), trackLength);
      velocityX += (playerX < car.offset) ? -0.3f : 0.3f;
      int s2 = (pSeg + RESPAWN_AHEAD_MIN + random(2, 8)) % TOTAL_SEGS;
      car.z     = s2 * (float)SEG_LEN + random(0, SEG_LEN / 2);
      car.speed = car.baseSpeed;
      if (speedDiff > maxSpeed * 0.5f) { crashed = true; crashTimer = millis(); }
    }
  }

  // Tunnel walls
  if (s.tunnel) {
    const float wallX = 0.95f;
    if (playerX < -wallX || playerX > wallX) {
      playerX   = clampF(playerX, -wallX, wallX);
      velocityX = 0.0f;
      speed    *= 0.3f;
      if (speed > maxSpeed * 0.35f) { crashed = true; crashTimer = millis(); }
    }
  }

  // Off-road sprites
  if (playerX < -1.0f || playerX > 1.0f) {
    if (s.spriteType >= 0 &&
        overlapChk(playerX, playerW, s.spriteOffset, 0.4f)) {
      speed *= 0.2f;
      if (speed > maxSpeed * 0.25f) { crashed = true; crashTimer = millis(); }
    }
  }

  // Completely off road
  if (playerX <= -2.4f || playerX >= 2.4f) {
    speed *= 0.5f;
    if (speed > maxSpeed * 0.4f) { crashed = true; crashTimer = millis(); }
  }

  // Stop turbo immediately on any crash
  if (crashed) {
    turboActive   = false;
    turboTimeLeft = 0.0f;
    turboCharge   = 0.0f;
  }
}
