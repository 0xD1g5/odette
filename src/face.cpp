#include "face.h"

#include <math.h>

#include "draw.h"
#include "palette.h"

static const float TAU = 6.2831853f;

// ---- motion ----

// Blink: 180 ms close-and-open every 2.2-6 s.
static float blinkOpen(uint32_t now) {
  static uint32_t nextBlink = 3000, blinkStart = 0;
  const uint32_t BLINK_MS = 180;
  if (blinkStart == 0) {
    if (!reached(now, nextBlink)) return 1.0f;
    blinkStart = now ? now : 1;
  }
  uint32_t t = now - blinkStart;
  if (t >= BLINK_MS) {
    blinkStart = 0;
    nextBlink = now + random(2200, 6000);
    return 1.0f;
  }
  float p = (float)t / BLINK_MS;
  return p < 0.45f ? 1.0f - (p / 0.45f) * 0.92f : 0.08f + ((p - 0.45f) / 0.55f) * 0.92f;
}

// Glance: a new target every 1.2-3.8 s, eased towards each frame.
static void glance(uint32_t now, Pose& p) {
  static uint32_t nextGlance = 1500;
  static float gx = 0, gy = 0, tgx = 0, tgy = 0;
  if (reached(now, nextGlance)) {
    tgx = random(-100, 101) / 100.0f;
    tgy = random(-100, 101) / 100.0f;
    if (random(4) == 0) tgx = tgy = 0;
    nextGlance = now + random(1200, 3800);
  }
  gx += (tgx - gx) * 0.25f;
  gy += (tgy - gy) * 0.25f;
  p.gazeX = gx;
  p.gazeY = gy;
}

// 0..1..0 over `period` ms, eased (the mockups' ease-in-out loops).
static float wave(uint32_t now, uint32_t period) { return 0.5f - 0.5f * cosf(TAU * (now % period) / period); }

void moodMotion(Mood m, uint32_t now, Pose& p) {
  p = Pose();
  switch (m) {
    case M_IDLE:
    default:
      p.eyeOpen = blinkOpen(now);
      glance(now, p);
      p.dy = (int16_t)lroundf(wave(now, 2600) * 4);  // bob 0..4 px
      break;
  }
}

// ---- faces ----

// Mockup glance: eyes and brows travel about 12 px sideways and 3 px up or down.
static const float GAZE_X_PX = 12, GAZE_Y_PX = 3;

static void drawIdle(TFT_eSprite& s, const Xf& f, const Pose& p) {
  Xf eyes = f.moved(p.gazeX * GAZE_X_PX, p.gazeY * GAZE_Y_PX);
  bar(s, eyes, 72, 36 - p.browLift, 54, 11, 0, P_INK);
  bar(s, eyes, 194, 36 - p.browLift, 54, 11, 0, P_INK);
  pill(s, eyes, 72, 60, 54, 86, p.eyeOpen, P_INK);
  pill(s, eyes, 194, 60, 54, 86, p.eyeOpen, P_INK);
  if (p.eyeOpen > 0.5f) {
    float cy = 103;  // eye centre; the highlight scales towards it as the lid closes
    s.fillCircle(eyes.X(106), eyes.Y(cy + (82 - cy) * p.eyeOpen), 9, P_WHITE);
    s.fillCircle(eyes.X(228), eyes.Y(cy + (82 - cy) * p.eyeOpen), 9, P_WHITE);
  }
  s.fillEllipse(f.X(62), f.Y(168), 16, (int)f.H(9), P_BLUSH);
  s.fillEllipse(f.X(258), f.Y(168), 16, (int)f.H(9), P_BLUSH);
  quad(s, f.X(140), f.Y(178), f.X(160), f.Y(194), f.X(180), f.Y(178), 9, P_INK);
}

void drawFace(TFT_eSprite& s, Mood m, const Pose& p, uint32_t now) {
  Xf f;
  f.dx = p.dx;
  f.dy = p.dy;
  f.squash = p.squash;
  switch (m) {
    case M_IDLE:
    default:
      drawIdle(s, f, p);
      break;
  }
}

Rect faceBounds(Mood m, const Pose& p) {
  Rect r = {34, 18, 252, 192};  // idle: cheeks 46-274, brows from y 36, mouth to ~190, plus glance
  r.x += p.dx;
  r.y += p.dy;
  if (p.squash != 1) {  // stretch grows upwards from the pivot near the chin
    int top = Xf::PIVOT_Y + (int)((r.y - Xf::PIVOT_Y) * p.squash);
    r.h += r.y - top;
    r.y = top;
  }
  return r;
}
