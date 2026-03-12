/*
  ═══════════════════════════════════════════════════════════════
  TRAFFIC CAR RENDERING IMPLEMENTATION
  ═══════════════════════════════════════════════════════════════
*/

#include "render_traffic.h"
#include "rendering.h"
#include "config.h"
#include "colors.h"

// Forward declaration — drawQuad defined in render_road.cpp
void drawQuad(int x1, int y1, int x2, int y2,
              int x3, int y3, int x4, int y4, uint16_t c);

void drawTrafficCar(int cx, int cy, float scale, uint16_t col, int16_t clipY) {

  // ── FIX 1: Scale threshold was 0.002 ─────────────────────────────────────
  // scale = cameraDepth / camZ = 0.84 / (n * SEG_LEN)
  // Old threshold rejected ALL cars beyond segment 2:
  //   n=8  → scale=0.000525  REJECTED (was never drawn)
  //   n=40 → scale=0.000105  REJECTED (was never drawn)
  // New threshold 0.00005 allows drawing at full DRAW_DIST=40.
  if (scale < 0.00005f) return;
  if (cy >= SCR_H || cy < 0) return;

  // angle = 0 always (traffic faces away from player, player sees back)
  // cosA=1, sinA=0 → simplified out, only X and Y values matter for projection

  // ── FIX 2: Vertices scaled 25× to match road coordinate system ───────────
  // Road half-width = ROAD_W = 2000 units.
  // Original vertices ranged ±20 units:
  //   n=10: ±20 * 0.00042 * 160 = ±1.3px → total width 2.7px (invisible!)
  // New vertices range ±500 units:
  //   n=10: ±500 * 0.00042 * 160 = ±33.6px → total width 67px (clearly visible)
  //   n=40: ±500 * 0.000105 * 160 = ±8.4px → total width 17px (still visible)
  float verts[16][3] = {
    // Lower chassis (0-7) — ground level to door sill
    {-450,  75, -1000}, { 450,  75, -1000},  // 0,1 front bottom
    { 450,   0,  1000}, {-450,   0,  1000},  // 2,3 rear bottom
    {-450, 275, -1000}, { 450, 275, -1000},  // 4,5 front upper body
    { 500, 250,  1000}, {-500, 250,  1000},  // 6,7 rear upper body (wider)

    // Cabin (8-15) — door tops to roof
    {-375, 275, -325}, { 375, 275, -325},    // 8,9  front cabin base
    { 425, 275,  525}, {-425, 275,  525},    // 10,11 rear cabin base
    {-300, 525, -100}, { 300, 525, -100},    // 12,13 front roof
    { 300, 500,  325}, {-300, 500,  325}     // 14,15 rear roof
  };

  float sx[16], sy[16];
  for (int i = 0; i < 16; i++) {
    // X: lateral world-space → screen pixels (same formula as road half-width)
    sx[i] = cx + (verts[i][0] * scale * SCR_CX);
    // Y: 0=road surface (cy), positive Y rises above road
    sy[i] = cy - (verts[i][1] * scale * SCR_CY);
  }

  // ── FIX 3: Backface culling was inverted ──────────────────────────────────
  // Player is always BEHIND traffic → only REAR-facing faces should be visible.
  // Cross product math (verified for key faces viewed from behind):
  //   Rear bumper  (6,7,3,2):  cross = NEGATIVE → must use cross < 0 to draw ✓
  //   Front grille (4,5,1,0):  cross = POSITIVE → correctly culled with cross < 0 ✓
  //   Roof         (15,14,13,12): cross = NEGATIVE → drawn ✓
  //   Windshield   (12,13,9,8): cross = POSITIVE → culled ✓
  // Old code `cross > 0` did the exact opposite: drew front, culled rear.
  auto drawFace = [&](int v0, int v1, int v2, int v3, uint16_t faceCol) {
    float cross = (sx[v1]-sx[v0]) * (sy[v2]-sy[v0])
                - (sy[v1]-sy[v0]) * (sx[v2]-sx[v0]);
    if (cross >= 0) return;  // FIX: was > 0, flipped to show rear-facing faces

    float maxY = max(max(sy[v0], sy[v1]), max(sy[v2], sy[v3]));
    float minY = min(min(sy[v0], sy[v1]), min(sy[v2], sy[v3]));
    if (maxY < 0 || minY > SCR_H) return;  // Completely off screen
    if (minY > (float)clipY) return;        // FIX: was ignored — clips against buildings

    drawQuad((int)sx[v0], (int)sy[v0],
             (int)sx[v1], (int)sy[v1],
             (int)sx[v2], (int)sy[v2],
             (int)sx[v3], (int)sy[v3], faceCol);
  };

  uint16_t hoodCol  = col;
  uint16_t bodyCol  = darkenCol(col, 0.85f);
  uint16_t darkCol  = darkenCol(col, 0.65f);
  uint16_t glassCol = rgb(80, 180, 255);
  uint16_t grillCol = rgb(30, 30, 30);

  // Back-to-front draw order (painter's algorithm).
  // With culling fixed: rear bumper, roof, and rear window draw.
  //                     front grille and windshield are culled. ✓
  drawFace(6,  7,  3,  2, darkCol);    // ← Rear bumper: biggest face, faces player
  drawFace(14, 15, 11, 10, grillCol);  // Rear window
  drawFace(7,  6,  5,  4, hoodCol);    // Top of lower body
  drawFace(7,  4,  0,  3, bodyCol);    // Left side
  drawFace(5,  6,  2,  1, bodyCol);    // Right side
  drawFace(15, 14, 13, 12, hoodCol);   // Cabin roof
  drawFace(13, 14, 10,  9, bodyCol);   // Right door
  drawFace(15, 12,  8, 11, bodyCol);   // Left door
  drawFace(12, 13,  9,  8, glassCol);  // Windshield (cross>0 → culled from behind ✓)
  drawFace(0,  1,  2,  3, darkCol);    // Chassis base
  drawFace(4,  5,  1,  0, grillCol);   // Front grille (cross>0 → culled from behind ✓)
}
