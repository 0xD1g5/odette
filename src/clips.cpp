#include "clips.h"

#include <math.h>

#include "anim.h"
#include "draw.h"
#include "palette.h"

static const int SNORE_X = 176, SNORE_Y = 166;  // where the snore bubble grows, at the mouth
static bool snorePopped = false;

void clipSnorePopped(bool popped) { snorePopped = popped; }

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
// already switched the board to idle and swapped in the startle's pink palette for the clip's length.
static bool startle(TFT_eSprite& s, uint32_t t, uint32_t now, Pose p) {
  if (t >= 2500) return false;
  if (t < 800) {  // the jolt: a jump, and wide eyes held long enough to register
    p.dy = t < 450 ? (int16_t)(-20 * sinf(PI_F * t / 450)) : 0;
    p.eyeOpen = 1.2f;
    p.browLift = 14;
    p.gazeX = p.gazeY = 0;
  } else if (t < 1700) {
    p.browLift = 14 * (1 - segment(t, 800, 1200));
    p.eyeOpen = dip(t, 1000, 1300, 0.15f) * dip(t, 1400, 1700, 0.2f);
    p.gazeX = p.gazeY = 0;
  } else {
    float u = (t - 1700) / 800.0f;  // left, right, back to the middle
    p.gazeX = u < 0.3f ? -segment(t, 1700, 1940) : (u < 0.7f ? -1 + 2 * segment(t, 1940, 2260) : 1 - segment(t, 2260, 2500));
  }
  drawFace(s, M_IDLE, p, now);
  if (t < 1500) {  // the "!"
    stroke(s, 286, 10, 286, 38, 9, P_INK);
    s.fillCircle(286, 50, 5, P_INK);
  }
  if (snorePopped && t < 250) {  // the snore bubble bursts
    for (int i = 0; i < 8; i++) {
      float a = i * PI_F / 4;
      float r0 = 12 + t / 25.0f;
      stroke(s, SNORE_X + r0 * cosf(a), SNORE_Y + r0 * sinf(a), SNORE_X + (r0 + 6) * cosf(a), SNORE_Y + (r0 + 6) * sinf(a), 3,
             P_WHITE);
    }
  }
  return true;
}

// ---- idle quirks (idle-vignettes.html, idle-vignettes-3.html) ----

static void idleEyes(TFT_eSprite& s, const Xf& f, float lookPx = 0) {
  Xf e = f.moved(lookPx, 0);
  pill(s, e, 72, 60, 54, 86, 1, P_INK);
  pill(s, e, 194, 60, 54, 86, 1, P_INK);
  s.fillCircle(lroundf(e.X(106)), lroundf(e.Y(82)), 9, P_WHITE);
  s.fillCircle(lroundf(e.X(228)), lroundf(e.Y(82)), 9, P_WHITE);
}

static void cheeks(TFT_eSprite& s, const Xf& f, float y, int rx, int ry, uint8_t col) {
  s.fillEllipse(lroundf(f.X(62)), lroundf(f.Y(y)), rx, lroundf(f.H(ry)), col);
  s.fillEllipse(lroundf(f.X(258)), lroundf(f.Y(y)), rx, lroundf(f.H(ry)), col);
}

static void note(TFT_eSprite& s, int x, int y, bool flag) {
  s.fillEllipse(x, y, 7, 5, P_INK);
  s.fillRect(x + 5, y - 24, 3, 24, P_INK);
  if (flag) stroke(s, x + 8, y - 23, x + 15, y - 13, 3, P_INK);
}

// Eyes shut in a content look, head sways, puckered mouth, notes drift up and away.
static bool whistle(TFT_eSprite& s, uint32_t t) {
  if (t >= 6000) return false;
  Xf f;
  f.dx = 8 * sinf(2 * PI_F * t / 2400);  // sway (the mockup rotates 4 degrees about the chin)
  bar(s, f, 72, 40, 54, 11, -6, P_INK);
  bar(s, f, 194, 40, 54, 11, 6, P_INK);
  quad(s, f.X(76), f.Y(110), f.X(99), f.Y(92), f.X(122), f.Y(110), 11, P_INK);
  quad(s, f.X(198), f.Y(110), f.X(221), f.Y(92), f.X(244), f.Y(110), 11, P_INK);
  cheeks(s, f, 160, 16, 9, P_BLUSH);
  s.fillCircle(lroundf(f.X(176)), lroundf(f.Y(180)), 12, P_INK);
  s.fillCircle(lroundf(f.X(176)), lroundf(f.Y(180)), 6, P_BG);
  for (int i = 0; i < 3; i++) {  // a note every second, each drifting 46 px right and 70 px up over 3 s
    if (t < (uint32_t)i * 1000) continue;
    float u = cycle(t, 3000, i * 1000);
    if (u < 0.05f || u > 0.9f) continue;
    note(s, 200 + (int)(46 * u), 170 - (int)(70 * u), i != 1);
  }
  return true;
}

// A cookie shows up, bites vanish from it one by one, the mouth chomps, crumbs fall.
static bool snack(TFT_eSprite& s, uint32_t t) {
  if (t >= 6000) return false;
  Xf f;
  f.dy = 4 * wave(t, 2600);
  bar(s, f, 72, 32, 54, 11, -10, P_INK);
  bar(s, f, 194, 32, 54, 11, 10, P_INK);
  quad(s, f.X(74), f.Y(112), f.X(99), f.Y(70), f.X(124), f.Y(112), 14, P_INK);
  quad(s, f.X(196), f.Y(112), f.X(221), f.Y(70), f.X(246), f.Y(112), 14, P_INK);
  cheeks(s, f, 150, 17, 10, P_CHEEK);
  s.fillEllipse(lroundf(f.X(138)), lroundf(f.Y(168)), 16, lroundf(f.H(10 * (1 - 0.65f * wave(t, 500)))), P_INK);
  s.fillCircle(186, 184, 30, P_COOKIE);
  s.fillCircle(176, 176, 4, P_CHIP);
  s.fillCircle(195, 190, 4, P_CHIP);
  s.fillCircle(182, 198, 3, P_CHIP);
  s.fillCircle(200, 172, 3, P_CHIP);
  if (t >= 1000) s.fillCircle(158, 170, 13, P_BG);  // bites, at 17%, 41% and 67% of the loop
  if (t >= 2460) s.fillCircle(156, 190, 13, P_BG);
  if (t >= 4000) s.fillCircle(164, 208, 12, P_BG);
  if (t >= 1000) {
    for (int k = 0; k < 2; k++) {
      float u = cycle(t, 1000, k * 500);
      s.fillRect(152 + k * 12, 186 + k * 4 + (int)(30 * u), 5 - k, 5 - k, P_COOKIE);
    }
  }
  return true;
}

// The mug, in its own frame: (lx, ly) from the body's centre, turned by the tilt, placed at (cx, cy).
struct Mug {
  float cx, cy, c, sn;
  float x(float lx, float ly) const { return cx + lx * c - ly * sn; }
  float y(float lx, float ly) const { return cy + lx * sn + ly * c; }
  void rect(TFT_eSprite& s, float x0, float y0, float x1, float y1, uint8_t col) const {
    float xs[4] = {x(x0, y0), x(x1, y0), x(x1, y1), x(x0, y1)};
    float ys[4] = {y(x0, y0), y(x1, y0), y(x1, y1), y(x0, y1)};
    fillConvex(s, xs, ys, 4, col);
  }
  void circle(TFT_eSprite& s, float lx, float ly, int r, uint8_t col) const {
    s.fillCircle(lroundf(x(lx, ly)), lroundf(y(lx, ly)), r, col);
  }
  void ellipse(TFT_eSprite& s, float lx, float ly, float rx, float ry, uint8_t col) const {
    float xs[16], ys[16];
    for (int i = 0; i < 16; i++) {
      float a = i * PI_F / 8;
      xs[i] = x(lx + rx * cosf(a), ly + ry * sinf(a));
      ys[i] = y(lx + rx * cosf(a), ly + ry * sinf(a));
    }
    fillConvex(s, xs, ys, 16, col);
  }
};

// A steaming mug rises from the bottom, he tips it to his mouth and blushes, the mug goes away.
static bool coffee(TFT_eSprite& s, uint32_t t) {
  if (t >= 7000) return false;
  static const Key RISE[] = {{0, 90}, {560, 90}, {1400, 0}, {5600, 0}, {6440, 90}, {7000, 90}};
  static const Key SIP[] = {{0, 0}, {2800, 0}, {3500, 1}, {4060, 1}, {4760, 0}, {7000, 0}};
  const float W = 23, H = 26, R = 8;  // half the body's size, and its corner radius
  float sip = track(t, SIP);
  Xf f;
  f.dy = 4 * wave(t, 2600);
  bar(s, f, 72, 40, 54, 11, 0, P_INK);
  bar(s, f, 194, 40, 54, 11, 0, P_INK);
  quad(s, f.X(76), f.Y(112), f.X(99), f.Y(128), f.X(122), f.Y(112), 10, P_INK);
  quad(s, f.X(198), f.Y(112), f.X(221), f.Y(128), f.X(244), f.Y(112), 10, P_INK);
  cheeks(s, f, 156, 18 + (int)(4 * sip), 10, P_CHEEK);  // "ahh": a deeper blush while sipping
  if (sip < 0.5f) {
    quad(s, f.X(124), f.Y(176), f.X(138), f.Y(186), f.X(152), f.Y(176), 8, P_INK);
  } else {
    s.fillEllipse(lroundf(f.X(140)), lroundf(f.Y(180)), 8, 7, P_INK);  // lips pursed on the rim
  }
  // Tipped 35 degrees towards him, the rim's near corner lands on the right of his mouth, (146, 181).
  float a = -0.61f * sip;
  Mug m = {193 - 13 * sip, 182 + 7 * sip + track(t, RISE) + f.dy * sip, cosf(a), sinf(a)};
  if (sip < 0.1f) {
    for (int k = 0; k < 2; k++) {  // steam, until he lifts it
      float u = cycle(t, 2000, k * 1000);
      if (u < 0.12f || u > 0.9f) continue;
      int x = lroundf(m.x(-9 + 16 * k, -H)), y = lroundf(m.y(-9 + 16 * k, -H)) - 6 - (int)(36 * u);
      quad(s, x, y, x - 6, y - 8, x, y - 16, 4, P_WHITE);
      quad(s, x, y - 16, x + 6, y - 24, x, y - 32, 4, P_WHITE);
    }
  }
  m.circle(s, W + 8, 0, 15, P_WHITE);  // handle first, so the body covers its inner edge
  m.circle(s, W + 8, 0, 9, P_BG);
  m.rect(s, -W + R, -H, W - R, H, P_WHITE);  // a rounded body: two crossed rectangles and four corners
  m.rect(s, -W, -H + R, W, H - R, P_WHITE);
  for (int i = 0; i < 4; i++) m.circle(s, i & 1 ? W - R : -W + R, i & 2 ? H - R : -H + R, R, P_WHITE);
  m.ellipse(s, 0, -H + 4, 19, 5, P_COFFEE);
  m.rect(s, -W, 0, W, 8, P_CHEEK);
  return true;
}

// Eyes squeeze shut into > <, a big yawn with a small tear, he stretches tall, then squishes back down.
static bool yawn(TFT_eSprite& s, uint32_t t) {
  if (t >= 6000) return false;
  static const Key STRETCH[] = {{0, 1}, {900, 1}, {2100, 1.1f}, {3300, 1.1f}, {3900, 0.93f}, {4500, 1}, {6000, 1}};
  static const Key MOUTH[] = {{0, 0}, {900, 0}, {2100, 1}, {3300, 1}, {4200, 0}, {6000, 0}};
  Xf f;
  f.squash = track(t, STRETCH);
  bar(s, f, 72, 36, 54, 11, 0, P_INK);
  bar(s, f, 194, 36, 54, 11, 0, P_INK);
  if (t < 1140 || t >= 4140) {
    idleEyes(s, f);
  } else {
    stroke(s, f.X(78), f.Y(92), f.X(118), f.Y(104), 10, P_INK);
    stroke(s, f.X(118), f.Y(104), f.X(78), f.Y(116), 10, P_INK);
    stroke(s, f.X(242), f.Y(92), f.X(202), f.Y(104), 10, P_INK);
    stroke(s, f.X(202), f.Y(104), f.X(242), f.Y(116), 10, P_INK);
  }
  if (t >= 3300 && t < 5400) {  // tear
    int y = lroundf(f.Y(122 + 14 * segment(t, 3000, 4800)));
    s.fillTriangle(70, y, 66, y + 8, 74, y + 8, P_WHITE);
    s.fillCircle(70, y + 9, 4, P_WHITE);
  }
  float k = track(t, MOUTH), sx = 0.4f + 0.6f * k, sy = 0.3f + 0.7f * k;  // grows from its top edge
  s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(156 + 30 * sy)), lroundf(22 * sx), lroundf(f.H(30 * sy)), P_INK);
  s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(156 + 48 * sy)), lroundf(12 * sx), lroundf(f.H(8 * sy)), P_TONGUE);
  return true;
}

// Build-up ("ah.. ah..", brows up, eyes squint), then "achoo!", a jolt forward and a little spray.
static bool sneeze(TFT_eSprite& s, uint32_t t) {
  if (t >= 5000) return false;
  static const Key DY[] = {{0, 0}, {500, 0}, {1250, -8}, {2400, -8}, {2600, 10}, {3000, 0}, {5000, 0}};
  static const Key SCALE[] = {{0, 1}, {500, 1}, {1250, 0.97f}, {2400, 0.97f}, {2600, 1.12f}, {3000, 1}, {5000, 1}};
  Xf f;
  f.dy = track(t, DY);
  f.squash = track(t, SCALE);
  if (t < 800 || t >= 2950) {
    bar(s, f, 72, 36, 54, 11, 0, P_INK);
    bar(s, f, 194, 36, 54, 11, 0, P_INK);
    idleEyes(s, f);
    quad(s, f.X(140), f.Y(178), f.X(160), f.Y(194), f.X(180), f.Y(178), 9, P_INK);
  } else {
    bar(s, f, 70, 26, 56, 11, 14, P_INK);
    bar(s, f, 194, 26, 56, 11, -14, P_INK);
    quad(s, f.X(76), f.Y(104), f.X(99), f.Y(86), f.X(122), f.Y(104), 11, P_INK);
    quad(s, f.X(198), f.Y(104), f.X(221), f.Y(86), f.X(244), f.Y(104), 11, P_INK);
    s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(180)), 13, lroundf(f.H(16)), P_INK);
  }
  s.setTextDatum(MC_DATUM);
  s.setTextColor(P_INK);
  if (t >= 900 && t < 2400) s.drawString(t < 1650 ? "ah.." : "ah.. ah..", 252, 40, 4);
  if (t >= 2450 && t < 4000) s.drawString("achoo!", 248, 52, 4);
  s.setTextDatum(TL_DATUM);
  if (t >= 2500 && t < 3500) {
    int dy = (int)(18 * segment(t, 2500, 3500));
    const int xs[] = {138, 162, 184, 150, 174}, ys[] = {206, 214, 206, 220, 222};
    for (int i = 0; i < 5; i++) s.fillCircle(xs[i], ys[i] + dy, i < 3 ? 3 : 2, P_WHITE);
  }
  return true;
}

// "!", then he ducks off the right edge, peeks back with one eye, ducks out, sneaks a second lower peek,
// and zips back in from the other side with a grin.
static bool peek(TFT_eSprite& s, uint32_t t) {
  if (t >= 9000) return false;
  static const Key DX[] = {{0, 0},      {720, 0},    {1440, 340}, {2340, 340}, {2880, 206}, {3780, 206},
                           {4050, 340}, {4680, 340}, {5220, 214}, {6120, 214}, {6480, 340}, {6929, 340},
                           {6930, -340}, {7920, 0},  {9000, 0}};
  static const Key DY[] = {{0, 0}, {4680, 0}, {5220, 50}, {6929, 50}, {6930, 0}, {9000, 0}};
  static const Key LOOK[] = {{0, 0},    {2700, 0}, {2970, -14}, {3600, -14}, {3780, 0},
                             {5130, 0}, {5400, -14}, {6030, -14}, {6300, 0}, {9000, 0}};
  Xf f;
  f.dx = track(t, DX);
  f.dy = track(t, DY);
  bar(s, f, 72, 34, 54, 11, -8, P_INK);
  bar(s, f, 194, 34, 54, 11, 8, P_INK);
  Xf e = f.moved(track(t, LOOK), 0);
  pill(s, e, 72, 60, 54, 86, 1, P_INK);
  pill(s, e, 194, 60, 54, 86, 1, P_INK);
  s.fillCircle(lroundf(e.X(96)), lroundf(e.Y(82)), 9, P_WHITE);
  s.fillCircle(lroundf(e.X(218)), lroundf(e.Y(82)), 9, P_WHITE);
  cheeks(s, f, 166, 16, 9, P_BLUSH);
  if (t >= 7290 && t < 8640) {
    Path(f).move(132, 174).line(188, 174).quad(184, 206, 160, 206).quad(136, 206, 132, 174).fill(s, P_INK);
    Path(f).move(146, 196).quad(160, 186, 174, 196).quad(168, 204, 160, 204).quad(152, 204, 146, 196).fill(s, P_TONGUE);
  } else {
    s.fillEllipse(lroundf(f.X(160)), lroundf(f.Y(184)), 8, 10, P_INK);
  }
  if (t >= 180 && t < 900) {  // the "!" before he hides, rising into place
    int rise = (int)(6 * (1 - segment(t, 180, 450)));
    stroke(s, 158, 12 + rise, 158, 30 + rise, 6, P_INK);
    s.fillCircle(158, 38 + rise, 4, P_INK);
  }
  return true;
}

// Sleepy only: a bubble slowly grows at his mouth... and pops.
static bool snore(TFT_eSprite& s, uint32_t t, uint32_t now, Pose p) {
  if (t >= 4000) return false;
  drawFace(s, M_SLEEPY, p, now);
  if (t < 3480) {
    float u = clamp01(t / 3400.0f);
    int r = lroundf(22 * (0.1f + 0.9f * u * u));
    s.fillCircle(SNORE_X, SNORE_Y, r, P_SNORE_EDGE);
    if (r > 3) s.fillCircle(SNORE_X, SNORE_Y, r - 2, P_SNORE_FILL);
  } else if (t < 3700) {
    for (int i = 0; i < 8; i++) {
      float a = i * PI_F / 4, r0 = 18 + (t - 3480) / 20.0f;
      stroke(s, SNORE_X + r0 * cosf(a), SNORE_Y + r0 * sinf(a), SNORE_X + (r0 + 5) * cosf(a), SNORE_Y + (r0 + 5) * sinf(a),
             2, P_SNORE_EDGE);
    }
  }
  return true;
}

ClipId clipForQuirk(Quirk q) {
  switch (q) {
    case Q_WHISTLE:
      return CLIP_WHISTLE;
    case Q_SNACK:
      return CLIP_SNACK;
    case Q_COFFEE:
      return CLIP_COFFEE;
    case Q_YAWN:
      return CLIP_YAWN;
    case Q_SNEEZE:
      return CLIP_SNEEZE;
    case Q_PEEK:
      return CLIP_PEEK;
    case Q_SNORE:
      return CLIP_SNORE;
    default:
      return CLIP_NONE;
  }
}

bool drawClip(TFT_eSprite& s, ClipId id, Mood m, uint32_t t, uint32_t now, Pose p) {
  switch (id) {
    case CLIP_WHISTLE:
      return whistle(s, t);
    case CLIP_SNACK:
      return snack(s, t);
    case CLIP_COFFEE:
      return coffee(s, t);
    case CLIP_YAWN:
      return yawn(s, t);
    case CLIP_SNEEZE:
      return sneeze(s, t);
    case CLIP_PEEK:
      return peek(s, t);
    case CLIP_SNORE:
      return snore(s, t, now, p);
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
