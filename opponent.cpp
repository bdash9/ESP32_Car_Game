/*
  ═══════════════════════════════════════════════════════════════
  OPPONENT AI IMPLEMENTATION
  ═══════════════════════════════════════════════════════════════
*/

#include "opponent.h"
#include "track.h"
#include "utils.h"
#include "physics.h"
#include "config.h"
#include <Arduino.h>

OpponentState opp;
LapResult     raceResults[NUM_TRACKS][MAX_LAPS_PER_TRACK];
bool          gameOverPending = false;

static const float OPP_GEAR_SPEED_CAP[NUM_GEARS] = {
  0.25f, 0.50f, 0.75f, 1.00f
};

static void tryResolveWinner(int ti, int li) {
  LapResult& r = raceResults[ti][li];
  if (r.playerTime > 0.1f && r.opponentTime > 0.1f) {
    if      (r.playerTime < r.opponentTime) r.winner = 0;
    else if (r.opponentTime < r.playerTime) r.winner = 1;
    else                                    r.winner = 2;
  }
}

void recordPlayerLapTime(int ti, int li, float t) {
  if (ti < 0 || ti >= NUM_TRACKS || li < 0 || li >= MAX_LAPS_PER_TRACK) return;
  raceResults[ti][li].playerTime = t;
  tryResolveWinner(ti, li);
}

void recordOpponentLapTime(int ti, int li, float t) {
  if (ti < 0 || ti >= NUM_TRACKS || li < 0 || li >= MAX_LAPS_PER_TRACK) return;
  raceResults[ti][li].opponentTime = t;
  tryResolveWinner(ti, li);
}

void initOpponent() {
  opp.position        = 0.0f;
  opp.prevPosition    = 0.0f;
  opp.speed           = 0.0f;
  opp.x               = 0.35f;   // Start right-of-centre lane
  opp.velocityX       = 0.0f;
  opp.acceleration    = 0.0f;
  opp.currentGear     = 0;
  opp.crashed         = false;
  opp.crashTimer      = 0;
  opp.currentLapTime  = 0.0f;
  opp.bestLapTime     = 0.0f;
  opp.currentLap      = 1;
  opp.invincibleUntil = millis() + 3000;
  opp.mistakeTimer    = 3.0f + random(0, 50) / 10.0f;
  opp.mistakeForce    = 0.0f;
  opp.mistakeDur      = 0.0f;

  for (int t = 0; t < NUM_TRACKS; t++)
    for (int l = 0; l < MAX_LAPS_PER_TRACK; l++) {
      raceResults[t][l].playerTime   = 0.0f;
      raceResults[t][l].opponentTime = 0.0f;
      raceResults[t][l].winner       = -1;
    }

  gameOverPending = false;
}

void resetOpponentForNewTrack() {
  opp.position        = 0.0f;
  opp.prevPosition    = 0.0f;
  opp.speed           = 0.0f;
  opp.x               = 0.35f;
  opp.velocityX       = 0.0f;
  opp.acceleration    = 0.0f;
  opp.currentGear     = 0;
  opp.crashed         = false;
  opp.crashTimer      = 0;
  opp.currentLapTime  = 0.0f;
  opp.bestLapTime     = 0.0f;
  opp.currentLap      = 1;
  opp.invincibleUntil = millis() + 3000;
  opp.mistakeTimer    = 3.0f + random(0, 50) / 10.0f;
  opp.mistakeForce    = 0.0f;
  opp.mistakeDur      = 0.0f;
}

void updateOpponent(float dt) {
  if (opp.crashed) {
    opp.currentLapTime += dt;
    if (millis() - opp.crashTimer > 2000) {
      opp.crashed         = false;
      opp.speed           = maxSpeed * 0.30f;
      opp.x               = clampF(opp.x, -0.6f, 0.6f);
      opp.velocityX       = 0.0f;
      opp.mistakeForce    = 0.0f;
      opp.mistakeDur      = 0.0f;
      opp.mistakeTimer    = 4.0f + random(0, 40) / 10.0f;
      opp.invincibleUntil = millis() + 2500;
    }
    return;
  }

// ── Acceleration (105% of player max — slight edge) ──────────
  float oppMaxSpeed = maxSpeed * 1.05f;
  static const float GEAR_MULT[NUM_GEARS] = { 3.5f, 1.8f, 1.3f, 1.0f };
  float tgtAccel    = oppMaxSpeed * ACCEL_TARGET;

  if (opp.speed < oppMaxSpeed * ACCEL_NEAR_MAX) {
    opp.acceleration += ACCEL_RAMP * GEAR_MULT[opp.currentGear] * dt;
    if (opp.acceleration > tgtAccel) opp.acceleration = tgtAccel;
  } else {
    opp.acceleration *= ACCEL_DAMPING;
  }
  if (opp.acceleration < 0.0f) opp.acceleration = 0.0f;

  opp.speed *= FRICTION;
  opp.speed += opp.acceleration * dt;

  float gearCap = OPP_GEAR_SPEED_CAP[opp.currentGear] * oppMaxSpeed;
  if (opp.speed > gearCap) { opp.speed = gearCap; opp.acceleration *= 0.7f; }
  if (opp.speed < 0.0f) opp.speed = 0.0f;

// Auto gear shift — thresholds must be BELOW each gear's speed cap
  // Cap:   1st=25%  2nd=50%  3rd=75%  4th=100%
  // Shift: at 20%   at 44%   at 68%   (stay in 4th)
  float spPct = opp.speed / oppMaxSpeed;
  if      (spPct > 0.68f) opp.currentGear = 3;
  else if (spPct > 0.44f) opp.currentGear = 2;
  else if (spPct > 0.20f) opp.currentGear = 1;
  else                    opp.currentGear = 0;

  // ── Curve centrifugal ────────────────────────────────────────
  int pSeg    = findSegIdx(opp.position);
  int prevSeg = (pSeg - 1 + TOTAL_SEGS) % TOTAL_SEGS;
  opp.velocityX += segments[pSeg].curve * CENTRIFUGAL * (opp.speed / maxSpeed) * dt;
  opp.velocityX *= LATERAL_FRICTION;
  opp.x         -= opp.velocityX * dt * 0.3f;

  // ── Mistake system ────────────────────────────────────────────
  if (opp.mistakeDur > 0.0f) {
    opp.x         += opp.mistakeForce * dt;
    opp.mistakeDur -= dt;
    if (opp.mistakeDur <= 0.0f) opp.mistakeForce = 0.0f;
  } else {
    opp.mistakeTimer -= dt;
    if (opp.mistakeTimer <= 0.0f) {
      if (random(0, 100) < 55) {
        opp.mistakeForce = (random(0, 2) == 0) ? -1.6f : 1.6f;
        opp.mistakeDur   = 0.2f + random(0, 30) / 100.0f;
      }
      opp.mistakeTimer = 5.0f + random(0, 80) / 10.0f;
    }
  }

  // ── Hill gravity ─────────────────────────────────────────────
  float slope     = (segments[pSeg].y - segments[prevSeg].y) / SEG_LEN;
  float gravityDv = -slope * (GRAVITY_FACTOR / 3.0f) * dt;
  gravityDv       = clampF(gravityDv, -maxSpeed * 0.10f * dt, maxSpeed * 0.10f * dt);
  opp.speed      += gravityDv;
  if (opp.speed < 0.0f) opp.speed = 0.0f;

  // ── Crash detection ───────────────────────────────────────────
  if (millis() >= opp.invincibleUntil) {
    bool wallHit = segments[pSeg].tunnel && (opp.x < -0.95f || opp.x > 0.95f);
    bool offRoad = (opp.x <= -2.2f || opp.x >= 2.2f);
    if (wallHit || offRoad) {
      opp.crashed    = true;
      opp.crashTimer = millis();
      opp.speed      = 0.0f;
      opp.velocityX  = 0.0f;
    }
  }
  opp.x = clampF(opp.x, -2.2f, 2.2f);

  // ── Advance position ─────────────────────────────────────────
  opp.prevPosition = opp.position;
  opp.position     = loopIncrease(opp.position, dt * opp.speed, trackLength);

  // ── Lap tracking ─────────────────────────────────────────────
  opp.currentLapTime += dt;
  if (opp.position < opp.prevPosition && opp.currentLapTime > 2.0f) {
    float lapT = opp.currentLapTime;
    if (opp.bestLapTime <= 0.0f || lapT < opp.bestLapTime)
      opp.bestLapTime = lapT;

    int lapIdx = opp.currentLap - 1;
    recordOpponentLapTime(currentTrack, lapIdx, lapT);

    opp.currentLapTime = 0.0f;
    opp.currentLap     = (opp.currentLap < totalLaps)
                         ? opp.currentLap + 1 : 1;
  }
}