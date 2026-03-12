/*
  ═══════════════════════════════════════════════════════════════
  GAME CONFIGURATION AND CONSTANTS
  ═══════════════════════════════════════════════════════════════
*/

#ifndef CONFIG_H
#define CONFIG_H

// ── Hardware pins ─────────────────────────────────────────────
#define TFT_BL    39
#define BTN_LEFT  17
#define BTN_RIGHT 16

// ── Joystick pins — CHANGE THESE TO MATCH YOUR WIRING ─────────
#define JOYSTICK_X    4    // Analog — left/right axis
#define JOYSTICK_Y    5    // Analog — forward/back axis
#define JOYSTICK_Z    7    // Digital — press button (unused, reserved)

// ADC thresholds (ESP32 12-bit: 0-4095, center ≈ 2048)
// Adjust if your joystick center is different
#define JOY_LOW     1400   // Below = pushed left or up
#define JOY_HIGH    2700   // Above = pushed right or down

// ── Buttons ───────────────────────────────────────────────────
#define BTN_TURBO    0    // KEY1 — GPIO 0 (BOOT button on most ESP32-S3 boards)
                          // VERIFY this matches your board before uploading

// ── Turbo system ──────────────────────────────────────────────
#define TURBO_SPEED_KMH    50.0f    // Flat +50 km/h boost 
#define TURBO_DURATION     5.0f     // seconds of boost
#define TURBO_BUILD_TIME   10.0f    // seconds of clean driving to reach full charge


// ── Screen ────────────────────────────────────────────────────
#define SCR_W       320
#define SCR_H       240
#define SCR_CX      (SCR_W / 2)
#define SCR_CY      (SCR_H / 2)

// ── Game constants ────────────────────────────────────────────
#define SEG_LEN     200
#define RUMBLE_LEN  3
#define DRAW_DIST   40
#define TOTAL_SEGS  200
#define ROAD_W      2000
#define LANES       3
#define FOV_DEG     100
#define CAM_HEIGHT  1000
#define FOG_DENSITY 5

#define RANDOM_TRACK 1

// ── Car physics ───────────────────────────────────────────────
#define SPEED_MULTIPLIER   65.0f
#define ACCEL_TARGET       0.9f
#define ACCEL_RAMP         180.0f
#define ACCEL_NEAR_MAX     0.90f
#define ACCEL_DAMPING      0.97f
#define FRICTION           0.996f
#define GRAVITY_FACTOR     400.0f
#define CENTRIFUGAL        0.18f
#define CURVE_FORCE        3.0f
#define LATERAL_FRICTION   0.90f
#define STEER_AUTO         1.8f
#define CENTRIFUGAL_DX     1.5f
#define POST_CRASH_SPEED   0.35f

// ── Gear system ───────────────────────────────────────────────
#define NUM_GEARS     4
#define MAX_RPM    10000.0f  // Full scale of tachometer (0-10 x1000)
#define REDLINE     8500.0f  // RPM where power cuts and needle enters red zone

// ── Buildings ─────────────────────────────────────────────────
#define BUILDING_H_MIN     120000
#define BUILDING_H_MAX     350000
#define BUILDING_W         400000
#define BUILDING_OFFSET    1.5f
#define BUILDING_SEG_MIN   6
#define BUILDING_SEG_MAX   16
#define BUILDING_GAP_MIN   10
#define BUILDING_GAP_MAX   20

// ── Traffic ───────────────────────────────────────────────────
#define MAX_CARS            2   // 1 slow car + 1 oncoming — sparse and challenging

#define RESPAWN_BEHIND_SEGS    8
#define RESPAWN_AHEAD_MIN     60
#define RESPAWN_AHEAD_MAX     80
#define RESPAWN_ONCOMING_MIN  60
#define RESPAWN_ONCOMING_MAX  90

// ── Car behaviour types (stored in TrafficCar.type) ───────────
#define CAR_SLOW           0    // Truck/bus:      15-25% maxSpeed, blocks lane
#define CAR_MEDIUM         1    // Normal car:     40-55% maxSpeed
#define CAR_FAST           2    // Sports car:     70-80% maxSpeed, weaves
#define CAR_ONCOMING       3    // Coming at you:  35-50%, instant crash on hit
#define CAR_BRAKER         4    // Brake-checker:  45-60%, random sudden stops

#endif // CONFIG_H
