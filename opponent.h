#pragma once
#include <Arduino.h>
#include "config.h"

#define MAX_LAPS_PER_TRACK 4

struct LapResult {
  float playerTime;
  float opponentTime;
  int   winner;   // 0=player, 1=opponent, 2=tie, -1=pending
};

extern LapResult raceResults[NUM_TRACKS][MAX_LAPS_PER_TRACK];
extern bool      gameOverPending;

struct OpponentState {
  float        position;
  float        prevPosition;
  float        speed;
  float        x;               // Lateral ±1 = road edges
  float        velocityX;
  float        acceleration;
  int          currentGear;
  bool         crashed;
  unsigned long crashTimer;
  float        currentLapTime;
  float        bestLapTime;
  int          currentLap;
  unsigned long invincibleUntil;
  float        mistakeTimer;    // Countdown to next mistake check
  float        mistakeForce;    // Active lateral error force
  float        mistakeDur;      // Remaining mistake duration
};

extern OpponentState opp;

void initOpponent();
void resetOpponentForNewTrack();
void updateOpponent(float dt);
void recordPlayerLapTime(int trackIdx, int lapIdx, float t);
void recordOpponentLapTime(int trackIdx, int lapIdx, float t);