/*
  ═══════════════════════════════════════════════════════════════
  PLAYER CAR RENDERING
  ═══════════════════════════════════════════════════════════════
*/

#ifndef RENDER_PLAYER_H
#define RENDER_PLAYER_H

#include <Arduino.h>

// Draws the player car in 3D
void drawPlayerCar();

// Draws the start screen with the rotating car
void drawStartScreen(float time);

// Draws the crash message
void drawCrashMessage();

void drawTrackTransition(int newTrack, float t);

void drawResultsScreen();

#endif // RENDER_PLAYER_H
