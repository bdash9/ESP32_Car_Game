/*
  ═══════════════════════════════════════════════════════════════
  PHYSICS AND LOGIC
  ═══════════════════════════════════════════════════════════════
*/

#ifndef PHYSICS_H
#define PHYSICS_H

#include <Arduino.h>
#include "structs.h"
#include "config.h"

// ── Camera and player ─────────────────────────────────────────
extern float         cameraDepth;
extern float         playerZdist;
extern float         position;
extern float         playerX;
extern float         speed;
extern float         maxSpeed;
extern float         centrifugal;

// ── Crash state ───────────────────────────────────────────────
extern bool          crashed;
extern unsigned long crashTimer;
extern unsigned long lastFrameMs;
extern unsigned long invincibleUntil;

// ── Lap tracking ──────────────────────────────────────────────
extern float currentLapTime;
extern float lastLapTime;
extern float bestLapTime;
extern float prevPosition;
extern int   currentLap;
extern int   totalLaps;

// ── Physics ───────────────────────────────────────────────────
extern float velocityX;
extern float acceleration;
extern float driftAngle;

// ── Gear system ───────────────────────────────────────────────
extern int   currentGear;   // 0-5  (displayed as 1-6)
extern float rpm;           // Current engine RPM

// ── Turbo system ──────────────────────────────────────────────
extern float turboCharge;    // 0.0 (empty) to 1.0 (full)
extern float turboTimeLeft;  // seconds of boost remaining
extern bool  turboActive;    // true while boost is running

extern int  carsPassed;   // Cars overtaken since last turbo charge
extern bool isSteering;   // True while left or right button is held

extern bool trackSwitchPending;

// ── Functions ─────────────────────────────────────────────────
void initPhysics();
void recoverFromCrash();
void readGearShift();       // Reads joystick and updates currentGear
void handleInput(float dt);
void updatePhysics(float dt);
void checkCollisions();
bool overlapChk(float aX, float aW, float bX, float bW);

#endif // PHYSICS_H
