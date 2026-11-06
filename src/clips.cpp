#include "clips.h"

#include <math.h>

#include "draw.h"
#include "palette.h"

static const float PI_F = 3.14159265f;
static bool snorePopped = false;

void clipSnorePopped(bool popped) { snorePopped = popped; }

// ---- timing helpers ----

static float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static float easeInOut(float x) { return 0.5f - 0.5f * cosf(PI_F * clamp01(x)); }
// Progress through [start, end) of the clip, eased.
static float segment(uint32_t t, uint32_t start, uint32_t end) {
  return easeInOut((float)((int32_t)t - (int32_t)start) / (end - start));
}
// A dip from 1 down to `low` and back over [start, end): a slow blink.
static float dip(uint32_t t, uint32_t start, uint32_t end, float low) {
  if (t < start || t >= end) return 1;
  return 1 - (1 - low) * sinf(PI_F * (t - start) / (end - start));
}

// ---- reactions ----

// Squeezed ^ ^ eyes, raised brows, big blush and an open grin, in whatever mood's colours are showing.
static void giggleFace(TFT_eSprite& s, const Xf& f) {
  bar(s, f, 72, 30, 54, 11, -6, P_INK);
  bar(s, f, 194, 30, 54, 11, 6, P_INK);
  quad(s, f.X(76), f.Y(112), f.X(99), f.Y(80), f.X(122), f.Y(112), 14, P_INK);
  quad(s, f.X(198), f.Y(112), f.X(221), f.Y(80), f.X(244), f.Y(112), 14, P_INK);
  s.fillEllipse(lroundf(f.X(62)), lroundf(f.Y(158)), 20, lroundf(f.H(12)), P_BLUSH);
  s.fillEllipse(lroundf(f.X(258)), lroundf(f.Y(158)), 20, lroundf(f.H(12)), P_BLUSH);
  Path(f).move(132, 160).line(188, 160).quad(186, 198, 160, 198).quad(134, 198, 132, 160).fill(s, P_INK);
}

static bool giggle(TFT_eSprite& s, uint32_t t, uint32_t duration) {
  if (t >= duration) return false;
  Xf f;
  f.dx = 5 * sinf(2 * PI_F * t / 220.0f);  // wiggle
  f.dy = -3 * fabsf(sinf(2 * PI_F * t / 440.0f));
  giggleFace(s, f);
  return true;
}

// A quick peek-a-boo: slides off the right edge, peeks back with one eye, then slides home.
static bool hide(TFT_eSprite& s, Mood m, uint32_t t, uint32_t now, Pose p) {
  const float OUT = 340, PEEK = 200;  // dx with the face fully off screen, and with only the left eye showing
  if (t >= 3000) return false;
  if (t < 400) {
    p.dx = (int16_t)(OUT * segment(t, 0, 400));
  } else if (t < 1300) {
    p.dx = (int16_t)OUT;
  } else if (t < 2300) {
    p.dx = (int16_t)(OUT - (OUT - PEEK) * segment(t, 1300, 1700));
    p.gazeX = -1;  // looking for you
    p.eyeOpen = dip(t, 1950, 2150, 0.1f);
  } else if (t < 2500) {
    p.dx = (int16_t)(PEEK + (OUT - PEEK) * segment(t, 2300, 2500));
  } else {
    p.dx = (int16_t)(OUT * (1 - segment(t, 2500, 3000)));
  }
  p.dy = 0;
  drawFace(s, m, p, now);
  return true;
}

// Woken from sleepy: eyes pop open with a "!", a little jump, groggy blinks, a look around. The caller has
// already switched the board to idle, so this draws in idle's colours.
static bool startle(TFT_eSprite& s, uint32_t t, uint32_t now, Pose p) {
  if (t >= 2500) return false;
  if (t < 450) {
    p.dy = (int16_t)(-14 * sinf(PI_F * t / 450));
    p.eyeOpen = 1.12f;
    p.browLift = 10;
    p.gazeX = p.gazeY = 0;
  } else if (t < 1500) {
    p.browLift = 10 * (1 - segment(t, 450, 900));
    p.eyeOpen = dip(t, 700, 1000, 0.15f) * dip(t, 1150, 1450, 0.2f);
    p.gazeX = p.gazeY = 0;
  } else {
    float u = (t - 1500) / 1000.0f;  // left, right, back to the middle
    p.gazeX = u < 0.3f ? -segment(t, 1500, 1800) : (u < 0.7f ? -1 + 2 * segment(t, 1800, 2200) : 1 - segment(t, 2200, 2500));
  }
  drawFace(s, M_IDLE, p, now);
  if (t < 1200) {  // the "!"
    stroke(s, 286, 14, 286, 34, 7, P_INK);
    s.fillCircle(286, 44, 4, P_INK);
  }
  if (snorePopped && t < 250) {  // the snore bubble bursts
    for (int i = 0; i < 8; i++) {
      float a = i * PI_F / 4;
      float r0 = 12 + t / 25.0f;
      stroke(s, 262 + r0 * cosf(a), 60 + r0 * sinf(a), 262 + (r0 + 6) * cosf(a), 60 + (r0 + 6) * sinf(a), 3, P_WHITE);
    }
  }
  return true;
}

bool drawClip(TFT_eSprite& s, ClipId id, Mood m, uint32_t t, uint32_t now, Pose p) {
  switch (id) {
    case CLIP_STARTLE:
      return startle(s, t, now, p);
    case CLIP_GIGGLE:
      return giggle(s, t, 2500);
    case CLIP_QUICK_GIGGLE:
      return giggle(s, t, 1000);
    case CLIP_HIDE:
      return hide(s, m, t, now, p);
    default:
      return false;
  }
}
