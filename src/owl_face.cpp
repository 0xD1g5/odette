#include "owl_face.h"

#include <math.h>

#include "draw.h"
#include "palette.h"

// Glance: eyes and brows travel together, as in the Classic faces.
static const float GAZE_X_PX = 12, GAZE_Y_PX = 3;

// Highlight at (hx, hy) in an eye centred on cy, pulled towards cy as the lid closes; hidden when mostly shut.
static void highlight(TFT_eSprite& s, const Xf& f, float hx, float hy, float cy, int r, float open) {
  if (open <= 0.5f) return;
  s.fillCircle(lroundf(f.X(hx)), lroundf(f.Y(cy + (hy - cy) * open)), r, P_WHITE);
}

static void blob(TFT_eSprite& s, const Xf& f, float x, float y, int rx, int ry, uint8_t col) {
  s.fillEllipse(lroundf(f.X(x)), lroundf(f.Y(y)), rx, lroundf(f.H(ry)), col);
}

// The owl stare: big discs, brows tilted in a little, blush under each eye.
static void drawIdle(TFT_eSprite& s, const Xf& f, const Pose& p) {
  Xf e = f.moved(p.gazeX * GAZE_X_PX, p.gazeY * GAZE_Y_PX);
  bar(s, e, 58, 34 - p.browLift, 70, 11, 8, P_INK);
  bar(s, e, 192, 34 - p.browLift, 70, 11, -8, P_INK);
  disc(s, e, 96, 118, 52, p.eyeOpen, P_INK);
  disc(s, e, 224, 118, 52, p.eyeOpen, P_INK);
  highlight(s, e, 116, 96, 118, 14, p.eyeOpen);
  highlight(s, e, 244, 96, 118, 14, p.eyeOpen);
  highlight(s, e, 96, 136, 118, 5, p.eyeOpen);
  highlight(s, e, 224, 136, 118, 5, p.eyeOpen);
  blob(s, f, 100, 194, 18, 9, P_BLUSH);
  blob(s, f, 220, 194, 18, 9, P_BLUSH);
}

// Big ^ ^ eyes and strong cheeks right under them.
static void drawHappy(TFT_eSprite& s, const Xf& f, const Pose& p) {
  bar(s, f, 58, 30 - p.browLift, 70, 11, -6, P_INK);
  bar(s, f, 192, 30 - p.browLift, 70, 11, 6, P_INK);
  quad(s, f.X(52), f.Y(140), f.X(96), f.Y(62), f.X(140), f.Y(140), 22, P_INK);
  quad(s, f.X(180), f.Y(140), f.X(224), f.Y(62), f.X(268), f.Y(140), 22, P_INK);
  blob(s, f, 100, 180, 22, 12, P_CHEEK);
  blob(s, f, 220, 180, 22, 12, P_CHEEK);
}

// Smaller eyes looking down and right at the "..." oval in the bottom-right corner, left brow raised.
static void drawThinking(TFT_eSprite& s, const Xf& f, const Pose& p, uint32_t now) {
  bar(s, f, 62, 40 - p.browLift, 66, 11, 10, P_INK);
  bar(s, f, 186, 46 - p.browLift, 66, 11, 0, P_INK);
  disc(s, f, 98, 112, 46, p.eyeOpen, P_INK);
  disc(s, f, 218, 112, 46, p.eyeOpen, P_INK);
  highlight(s, f, 120, 134, 112, 13, p.eyeOpen);
  highlight(s, f, 240, 134, 112, 13, p.eyeOpen);
  // The oval stays put while the face bobs.
  s.fillRoundRect(250, 194, 62, 36, 18, P_BUBBLE);
  int active = (now / 350) % 3;
  for (int i = 0; i < 3; i++) s.fillCircle(267 + i * 14, 212, 6, i == active ? P_INK : P_DIM);
}

// The biggest eyes, brows shot up, a white box with an orange "!" in the bottom-right corner.
static void drawAlert(TFT_eSprite& s, const Xf& f, const Pose& p) {
  bar(s, f, 61, 20 - p.browLift, 74, 12, 0, P_INK);
  bar(s, f, 185, 20 - p.browLift, 74, 12, 0, P_INK);
  disc(s, f, 98, 110, 58, p.eyeOpen, P_INK);
  disc(s, f, 222, 110, 58, p.eyeOpen, P_INK);
  highlight(s, f, 120, 84, 110, 16, p.eyeOpen);
  highlight(s, f, 244, 84, 110, 16, p.eyeOpen);
  // The box stays still while he jitters, so it reads instantly.
  s.fillRoundRect(266, 178, 46, 50, 8, P_WHITE);
  stroke(s, 289, 190, 289, 206, 8, P_ACCENT);
  s.fillCircle(289, 217, 5, P_ACCENT);
}

// Lids drooping at the outer corners, worried brows (inner ends up), puppy highlights, a falling tear.
static void drawSad(TFT_eSprite& s, const Xf& f, const Pose& p, uint32_t now) {
  disc(s, f, 96, 128, 50, p.eyeOpen, P_INK);
  disc(s, f, 224, 128, 50, p.eyeOpen, P_INK);
  // Lids in the background colour, cut across the top of each eye: low at the outer corner.
  float lx[] = {f.X(40), f.X(150), f.X(150), f.X(40)};
  float ly[] = {f.Y(118), f.Y(78), f.Y(60), f.Y(60)};
  fillConvex(s, lx, ly, 4, P_BG);
  float rx[] = {f.X(170), f.X(280), f.X(280), f.X(170)};
  float ry[] = {f.Y(78), f.Y(118), f.Y(60), f.Y(60)};
  fillConvex(s, rx, ry, 4, P_BG);
  bar(s, f, 56, 62 - p.browLift, 74, 11, -18, P_INK);
  bar(s, f, 190, 62 - p.browLift, 74, 11, 18, P_INK);
  highlight(s, f, 118, 134, 128, 14, p.eyeOpen);
  highlight(s, f, 104, 156, 128, 5, p.eyeOpen);
  highlight(s, f, 246, 134, 128, 14, p.eyeOpen);
  highlight(s, f, 232, 156, 128, 5, p.eyeOpen);
  // Tear: falls 40 px from under the left eye every 2.4 s, visible for the middle 70% of the fall.
  float t = (now % 2400) / 2400.0f;
  if (t > 0.15f && t < 0.85f) {
    int ty = 164 + (int)(40 * t * t);  // ease-in
    s.fillTriangle(58, ty, 53, ty + 10, 63, ty + 10, P_WHITE);
    s.fillCircle(58, ty + 12, 5, P_WHITE);
  }
}

// Wide closed lids, relaxed brows, Zs drifting up on the right, started low enough to stay off the top rows.
static void drawSleepy(TFT_eSprite& s, const Xf& f, uint32_t now) {
  bar(s, f, 58, 80, 70, 10, 0, P_INK);
  bar(s, f, 192, 80, 70, 10, 0, P_INK);
  quad(s, f.X(54), f.Y(126), f.X(96), f.Y(160), f.X(138), f.Y(126), 12, P_INK);
  quad(s, f.X(182), f.Y(126), f.X(224), f.Y(160), f.X(266), f.Y(126), 12, P_INK);
  drawSleepyZs(s, now, 20);
}

void drawOwlFace(TFT_eSprite& s, Mood m, const Pose& p, uint32_t now) {
  Xf f;
  f.dx = p.dx;
  f.dy = p.dy;
  f.squash = p.squash;
  switch (m) {
    case M_IDLE:
      drawIdle(s, f, p);
      break;
    case M_HAPPY:
      drawHappy(s, f, p);
      break;
    case M_THINKING:
      drawThinking(s, f, p, now);
      break;
    case M_ALERT:
      drawAlert(s, f, p);
      break;
    case M_ERROR:
      drawSad(s, f, p, now);
      break;
    case M_SLEEPY:
      drawSleepy(s, f, now);
      break;
    default:
      break;
  }
}
