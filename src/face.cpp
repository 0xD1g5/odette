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

// Fraction 0..1 through a repeating cycle.
static float phase(uint32_t now, uint32_t period, uint32_t offset = 0) {
  return (float)((now + period - offset % period) % period) / period;
}

void moodMotion(Mood m, uint32_t now, Pose& p) {
  p = Pose();
  switch (m) {
    case M_IDLE:
      p.eyeOpen = blinkOpen(now);
      glance(now, p);
      p.dy = (int16_t)lroundf(wave(now, 2600) * 4);  // bob 0..4 px
      break;
    case M_THINKING:
    case M_ERROR:
      p.eyeOpen = blinkOpen(now);
      p.dy = (int16_t)lroundf(wave(now, 2600) * 4);
      break;
    case M_HAPPY: {
      // Bounce: up 9 px and hold through the middle of each 0.9 s hop.
      float t = phase(now, 900);
      float up = t < 0.4f ? t / 0.4f : (t > 0.6f ? (1 - t) / 0.4f : 1.0f);
      p.dy = (int16_t)lroundf(-9 * (0.5f - 0.5f * cosf(up * (float)M_PI)));
      break;
    }
    case M_ALERT:
      p.eyeOpen = blinkOpen(now);
      p.dx = ((now / 90) % 2) ? 1 : -1;  // fast jitter, kept small: bigger steps tear visibly on this panel
      break;
    case M_SLEEPY:
      p.squash = 1 + 0.04f * wave(now, 4000);  // slow breathing
      break;
    default:
      break;
  }
}

// ---- faces ----

// Mockup glance: eyes and brows travel about 12 px sideways and 3 px up or down.
static const float GAZE_X_PX = 12, GAZE_Y_PX = 3;

// Eye highlight at (hx, hy), pulled towards the eye's centre line `cy` as the lid closes.
static void highlight(TFT_eSprite& s, const Xf& f, float hx, float hy, float cy, int r, float open) {
  if (open <= 0.5f) return;
  s.fillCircle(lroundf(f.X(hx)), lroundf(f.Y(cy + (hy - cy) * open)), r, P_WHITE);
}

static void drawIdle(TFT_eSprite& s, const Xf& f, const Pose& p) {
  Xf eyes = f.moved(p.gazeX * GAZE_X_PX, p.gazeY * GAZE_Y_PX);
  bar(s, eyes, 72, 36 - p.browLift, 54, 11, 0, P_INK);
  bar(s, eyes, 194, 36 - p.browLift, 54, 11, 0, P_INK);
  pill(s, eyes, 72, 60, 54, 86, p.eyeOpen, P_INK);
  pill(s, eyes, 194, 60, 54, 86, p.eyeOpen, P_INK);
  highlight(s, eyes, 106, 82, 103, 9, p.eyeOpen);
  highlight(s, eyes, 228, 82, 103, 9, p.eyeOpen);
  s.fillEllipse(lroundf(f.X(62)), lroundf(f.Y(168)), 16, lroundf(f.H(9)), P_BLUSH);
  s.fillEllipse(lroundf(f.X(258)), lroundf(f.Y(168)), 16, lroundf(f.H(9)), P_BLUSH);
  quad(s, f.X(140), f.Y(178), f.X(160), f.Y(194), f.X(180), f.Y(178), 9, P_INK);
}

// Looks up and to the right, one brow raised, flat "hmm" mouth, "..." bubble in the top-right corner.
static void drawThinking(TFT_eSprite& s, const Xf& f, const Pose& p, uint32_t now) {
  bar(s, f, 76, 40 - p.browLift, 54, 11, 0, P_INK);
  bar(s, f, 198, 28 - p.browLift, 54, 11, -12, P_INK);
  pill(s, f, 84, 58, 54, 80, p.eyeOpen, P_INK);
  pill(s, f, 206, 58, 54, 80, p.eyeOpen, P_INK);
  highlight(s, f, 124, 72, 98, 9, p.eyeOpen);
  highlight(s, f, 246, 72, 98, 9, p.eyeOpen);
  stroke(s, f.X(144), f.Y(184), f.X(184), f.Y(176), 9, P_INK);
  // The bubble stays put while the face bobs.
  s.fillRoundRect(256, 4, 62, 36, 18, P_BUBBLE);
  int active = (now / 350) % 3;
  for (int i = 0; i < 3; i++) s.fillCircle(273 + i * 14, 22, 6, i == active ? P_INK : P_DIM);
}

// ^ ^ eyes, raised brows, big open smile with a tongue, strong cheeks.
static void drawHappy(TFT_eSprite& s, const Xf& f, const Pose& p) {
  bar(s, f, 70, 34 - p.browLift, 56, 11, -10, P_INK);
  bar(s, f, 194, 34 - p.browLift, 56, 11, 10, P_INK);
  quad(s, f.X(72), f.Y(118), f.X(99), f.Y(66), f.X(126), f.Y(118), 20, P_INK);
  quad(s, f.X(194), f.Y(118), f.X(221), f.Y(66), f.X(248), f.Y(118), 20, P_INK);
  s.fillEllipse(lroundf(f.X(58)), lroundf(f.Y(150)), 17, lroundf(f.H(11)), P_CHEEK);
  s.fillEllipse(lroundf(f.X(262)), lroundf(f.Y(150)), 17, lroundf(f.H(11)), P_CHEEK);
  Path(f).move(122, 148).line(198, 148).quad(194, 204, 160, 204).quad(126, 204, 122, 148).fill(s, P_INK);
  Path(f).move(140, 188).quad(160, 174, 180, 188).quad(172, 202, 160, 202).quad(148, 202, 140, 188).fill(s, P_TONGUE);
}

// Tall wide eyes, brows high, small "o" mouth, white speech bubble with an orange "!".
static void drawAlert(TFT_eSprite& s, const Xf& f, const Pose& p) {
  bar(s, f, 68, 22 - p.browLift, 58, 12, 0, P_INK);
  bar(s, f, 194, 22 - p.browLift, 58, 12, 0, P_INK);
  pill(s, f, 68, 46, 58, 104, p.eyeOpen, P_INK);
  pill(s, f, 194, 46, 58, 104, p.eyeOpen, P_INK);
  highlight(s, f, 104, 70, 98, 10, p.eyeOpen);
  highlight(s, f, 230, 70, 98, 10, p.eyeOpen);
  s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(190)), 15, lroundf(f.H(18)), P_INK);
  // The bubble stays still while he jitters, so it reads instantly.
  s.fillRoundRect(268, 8, 48, 52, 8, P_WHITE);
  s.fillTriangle(284, 58, 296, 58, 286, 70, P_WHITE);
  stroke(s, 292, 20, 292, 37, 8, P_ACCENT);
  s.fillCircle(292, 48, 5, P_ACCENT);
}

// Worried brows (inner ends higher), lids drooping at the outer corners, puppy-eye highlights,
// wobbly pout and a falling tear.
static void drawSad(TFT_eSprite& s, const Xf& f, const Pose& p, uint32_t now) {
  bar(s, f, 66, 50 - p.browLift, 58, 11, -18, P_INK);  // left brow: inner (right) end up
  bar(s, f, 196, 50 - p.browLift, 58, 11, 18, P_INK);  // right brow: inner (left) end up
  pill(s, f, 72, 70, 54, 80, p.eyeOpen, P_INK);
  pill(s, f, 194, 70, 54, 80, p.eyeOpen, P_INK);
  highlight(s, f, 104, 104, 110, 11, p.eyeOpen);
  highlight(s, f, 92, 128, 110, 4, p.eyeOpen);
  highlight(s, f, 226, 104, 110, 11, p.eyeOpen);
  highlight(s, f, 214, 128, 110, 4, p.eyeOpen);
  // Lids in the background colour, cut across the top of each eye: low at the outer corner.
  float lx[] = {f.X(62), f.X(136), f.X(136), f.X(62)};
  float ly[] = {f.Y(94), f.Y(66), f.Y(60), f.Y(60)};
  fillConvex(s, lx, ly, 4, P_BG);
  float rx[] = {f.X(184), f.X(258), f.X(258), f.X(184)};
  float ry[] = {f.Y(66), f.Y(94), f.Y(60), f.Y(60)};
  fillConvex(s, rx, ry, 4, P_BG);
  // Pout wobbles: narrows to 85% and back every 1.6 s.
  float w = 1 - 0.15f * wave(now, 1600);
  quad(s, f.X(160 - 26 * w), f.Y(198), f.X(160), f.Y(174), f.X(160 + 26 * w), f.Y(198), 10, P_INK);
  // Tear: falls 40 px from under the left eye every 2.4 s, visible for the middle 70% of the fall.
  float t = phase(now, 2400);
  if (t > 0.15f && t < 0.85f) {
    int ty = 156 + (int)(40 * t * t);  // ease-in
    s.fillTriangle(80, ty, 75, ty + 10, 85, ty + 10, P_WHITE);
    s.fillCircle(80, ty + 12, 5, P_WHITE);
  }
}

// Closed eyes, relaxed brows, small "o" mouth, slow breathing, "Z z" drifting up.
static void drawSleepy(TFT_eSprite& s, const Xf& f, uint32_t now) {
  bar(s, f, 72, 62, 54, 10, 0, P_INK);
  bar(s, f, 194, 62, 54, 10, 0, P_INK);
  quad(s, f.X(74), f.Y(112), f.X(99), f.Y(134), f.X(124), f.Y(112), 10, P_INK);
  quad(s, f.X(196), f.Y(112), f.X(221), f.Y(134), f.X(246), f.Y(112), 10, P_INK);
  s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(180)), 12, lroundf(f.H(14)), P_INK);
  s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(180)), 6, lroundf(f.H(8)), P_BG);
  // Each Z drifts 18 px right and 34 px up over 3 s; the small one runs half a cycle behind.
  s.setTextDatum(BL_DATUM);
  s.setTextColor(P_ACCENT);
  const struct {
    int x, y, font;
    uint32_t offset;
  } zs[] = {{250, 70, 4, 0}, {270, 56, 2, 1500}};
  for (const auto& z : zs) {
    float t = phase(now, 3000, z.offset);
    if (t < 0.12f || t > 0.9f) continue;  // fade in and out by not drawing at the ends
    s.drawString(z.font == 4 ? "Z" : "z", z.x + (int)(18 * t), z.y - (int)(34 * t), z.font);
  }
  s.setTextDatum(TL_DATUM);
}

void drawFace(TFT_eSprite& s, Mood m, const Pose& p, uint32_t now) {
  Xf f;
  f.dx = p.dx;
  f.dy = p.dy;
  f.squash = p.squash;
  switch (m) {
    case M_IDLE:
      drawIdle(s, f, p);
      break;
    case M_THINKING:
      drawThinking(s, f, p, now);
      break;
    case M_HAPPY:
      drawHappy(s, f, p);
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
