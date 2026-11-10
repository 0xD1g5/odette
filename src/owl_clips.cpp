#include "owl_clips.h"

#include <math.h>

#include "anim.h"
#include "draw.h"
#include "owl_face.h"
#include "palette.h"

static const int OWL_SNORE_X = 160, OWL_SNORE_Y = 214;  // the snore bubble grows here, just above the beak

// ---- shared pieces ----

static void blob(TFT_eSprite& s, const Xf& f, float x, float y, int rx, int ry, uint8_t col) {
  s.fillEllipse(lroundf(f.X(x)), lroundf(f.Y(y)), rx, lroundf(f.H(ry)), col);
}

static void cheeks(TFT_eSprite& s, const Xf& f, float y, int rx, int ry, uint8_t col) {
  blob(s, f, 100, y, rx, ry, col);
  blob(s, f, 220, y, rx, ry, col);
}

static void arcs(TFT_eSprite& s, const Xf& f, float y0, float top, float width) {
  quad(s, f.X(56), f.Y(y0), f.X(96), f.Y(top), f.X(136), f.Y(y0), width, P_INK);
  quad(s, f.X(184), f.Y(y0), f.X(224), f.Y(top), f.X(264), f.Y(y0), width, P_INK);
}

// > <: eyes squeezed shut.
static void shut(TFT_eSprite& s, const Xf& f) {
  stroke(s, f.X(66), f.Y(94), f.X(126), f.Y(118), 14, P_INK);
  stroke(s, f.X(126), f.Y(118), f.X(66), f.Y(142), 14, P_INK);
  stroke(s, f.X(254), f.Y(94), f.X(194), f.Y(118), 14, P_INK);
  stroke(s, f.X(194), f.Y(118), f.X(254), f.Y(142), 14, P_INK);
}

// Happy ^ ^ eyes with strong cheeks, as the Happy mood draws them.
static void happyFace(TFT_eSprite& s, const Xf& f) {
  bar(s, f, 58, 30, 70, 11, -6, P_INK);
  bar(s, f, 192, 30, 70, 11, 6, P_INK);
  quad(s, f.X(52), f.Y(140), f.X(96), f.Y(62), f.X(140), f.Y(140), 22, P_INK);
  quad(s, f.X(180), f.Y(140), f.X(224), f.Y(62), f.X(268), f.Y(140), 22, P_INK);
  cheeks(s, f, 180, 22, 12, P_CHEEK);
}

// A plain ink "!" in the bottom-right corner, where the alert box lives, clear of the hidden top rows.
static void bang(TFT_eSprite& s, int x, int y) {
  stroke(s, x, y, x, y + 20, 7, P_INK);
  s.fillCircle(x, y + 31, 4, P_INK);
}

static void label(TFT_eSprite& s, const char* text, int x, int y, int font) {
  s.setTextDatum(MC_DATUM);
  s.setTextColor(P_INK);
  s.drawString(text, x, y, font);
  s.setTextDatum(TL_DATUM);
}

static Pose still() { return Pose(); }

// ---- idle quirks ----

static void note(TFT_eSprite& s, int x, int y, bool flag) {
  s.fillEllipse(x, y, 7, 5, P_INK);
  s.fillRect(x + 4, y - 24, 3, 24, P_INK);
  if (flag) stroke(s, x + 7, y - 23, x + 14, y - 13, 3, P_INK);
}

// Hum (whistle): eyes shut in a content ∩, a sway, and notes floating up from the beak between his eyes.
static bool hum(TFT_eSprite& s, uint32_t t) {
  if (t >= 6000) return false;
  Xf f;
  f.dx = 8 * sinf(2 * PI_F * t / 2400);
  bar(s, f, 58, 28, 70, 11, -4, P_INK);
  bar(s, f, 192, 28, 70, 11, 4, P_INK);
  quad(s, f.X(54), f.Y(130), f.X(96), f.Y(94), f.X(138), f.Y(130), 14, P_INK);
  quad(s, f.X(182), f.Y(130), f.X(224), f.Y(94), f.X(266), f.Y(130), 14, P_INK);
  cheeks(s, f, 184, 20, 10, P_BLUSH);
  for (int i = 0; i < 3; i++) {  // a note a second, each rising 150 px over 3 s with a little drift
    if (t < (uint32_t)i * 1000) continue;
    float u = cycle(t, 3000, i * 1000);
    if (u < 0.05f || u > 0.9f) continue;
    note(s, lroundf(156 + 10 * sinf(2 * PI_F * u * 1.5f) + 22 * u), lroundf(228 - 150 * u), i != 1);
  }
  return true;
}

// Peck (snack): a cookie rises onto the beak and he pecks it from below three times, cheeks puffing.
static bool peck(TFT_eSprite& s, uint32_t t) {
  if (t >= 6000) return false;
  static const uint32_t PECKS[] = {1200, 2600, 4000};
  static const Key RISE[] = {{0, 70}, {600, 0}, {5300, 0}, {6000, 70}};
  float pk = 0;
  int bites = 0;
  for (int i = 0; i < 3; i++) {
    if (t >= PECKS[i] + 180) bites = i + 1;
    if (t >= PECKS[i] && t < PECKS[i] + 360) pk = sinf(PI_F * (t - PECKS[i]) / 360);
  }
  Xf f;
  f.dy = 10 * pk + 3 * wave(t, 2600);
  bar(s, f, 58, 32, 70, 11, -8, P_INK);
  bar(s, f, 192, 32, 70, 11, 8, P_INK);
  arcs(s, f, 134, 74 + 22 * pk, 20);
  cheeks(s, f, 182, lroundf(20 + 4 * pk), lroundf(10 + 3 * pk), bites ? P_CHEEK : P_BLUSH);
  int cy = lroundf(214 + track(t, RISE));
  s.fillCircle(160, cy, 30, P_COOKIE);
  s.fillCircle(148, cy - 12, 4, P_CHIP);
  s.fillCircle(172, cy - 4, 4, P_CHIP);
  s.fillCircle(156, cy + 10, 3, P_CHIP);
  s.fillCircle(176, cy - 18, 3, P_CHIP);
  s.fillCircle(140, cy + 4, 3, P_CHIP);
  if (bites >= 1) s.fillCircle(146, cy + 30, 14, P_BG);  // bitten from below, where the beak is
  if (bites >= 2) s.fillCircle(176, cy + 28, 14, P_BG);
  if (bites >= 3) s.fillCircle(160, cy + 18, 15, P_BG);
  for (int i = 0; i < 3; i++) {  // crumbs drop off the bottom after each peck
    float u = ((int32_t)t - (int32_t)PECKS[i] - 180) / 700.0f;
    if (u < 0 || u > 1) continue;
    s.fillRect(150 + i * 8, cy + 20 + lroundf(40 * u * u), 5, 5, P_COOKIE);
    s.fillRect(166 + i * 6, cy + 24 + lroundf(46 * u * u), 4, 4, P_COOKIE);
  }
  return true;
}

// Perk-up (coffee): drowsy eyes, a steaming mug, a dip down to the beak ("glug"), then his eyes pop wide.
static bool perkUp(TFT_eSprite& s, uint32_t t) {
  if (t >= 7000) return false;
  static const Key DRINK[] = {{0, 0}, {2000, 0}, {2500, 1}, {3300, 1}, {3800, 0}, {7000, 0}};
  static const Key RISE[] = {{0, 80}, {600, 80}, {1400, 0}, {5800, 0}, {6500, 80}, {7000, 80}};
  float drowsy = 1 - segment(t, 3700, 3850);
  float pop = segment(t, 3800, 3950) * (1 - segment(t, 5000, 5800));
  int jit = t >= 3800 && t < 4400 ? lroundf(2.5f * sinf(2 * PI_F * t / 60)) : 0;
  Pose p = still();
  p.dx = jit;
  p.eyeOpen = 0.42f + 0.58f * (1 - drowsy) + 0.08f * pop;
  p.browLift = 14 * pop - 22 * drowsy;
  drawOwlIdle(s, p, 52 + 6 * pop);
  Xf f;
  if (t >= 5200) cheeks(s, f, 194, 20, 10, P_CHEEK);  // a warm "ahh"
  if (pop > 0.6f && t < 4700) {  // sparkle rays over each eye
    static const float ANGLES[] = {-55, -90, -125};
    for (int cx : {96, 224}) {
      for (float deg : ANGLES) {
        float a = deg * PI_F / 180, x = cx + jit + cosf(a) * 68, y = 118 + sinf(a) * 68;
        stroke(s, x, y, x + cosf(a) * 10, y + sinf(a) * 10, 4, P_WHITE);
      }
    }
  }
  float drink = track(t, DRINK);
  int y = lroundf(212 + track(t, RISE) + 44 * drink);
  if (t < 2000) {
    for (int k = 0; k < 2; k++) {  // steam, until the mug goes down to the beak
      float u = cycle(t, 2000, k * 1000);
      if (u < 0.12f || u > 0.9f) continue;
      float x = 151 + 18 * k, sy = y - 32 - 36 * u;
      quad(s, x, sy, x - 6, sy - 8, x, sy - 16, 4, P_WHITE);
      quad(s, x, sy - 16, x + 6, sy - 24, x, sy - 32, 4, P_WHITE);
    }
  }
  s.fillCircle(191, y, 15, P_WHITE);  // handle first, so the body covers its inner edge
  s.fillCircle(191, y, 9, P_BG);
  s.fillRoundRect(137, y - 26, 46, 52, 8, P_WHITE);
  s.fillEllipse(160, y - 22, 19, 5, t < 3300 ? P_COFFEE : P_EMPTY);
  s.fillRect(137, y, 46, 8, P_CHEEK);
  if (t >= 2500 && t < 3400) label(s, t < 2950 ? "glug" : "glug glug", 262, 214, 2);
  return true;
}

// Stretch (yawn): > < eyes as he stretches tall, a small tear, a feather-ruffle shake and a heavy blink.
static bool stretch(TFT_eSprite& s, uint32_t t) {
  if (t >= 6000) return false;
  static const Key STRETCH[] = {{0, 1}, {900, 1}, {2100, 1.1f}, {3300, 1.1f}, {3900, 0.93f}, {4500, 1}, {6000, 1}};
  float shake = t >= 4300 && t < 5100 ? 7 * sinf(2 * PI_F * t / 110) * (1 - segment(t, 4300, 5100)) : 0;
  Xf f;
  f.dx = shake;
  f.squash = track(t, STRETCH);
  if (t < 1140 || t >= 4140) {
    Pose p = still();
    p.dx = lroundf(shake);
    p.squash = f.squash;
    p.eyeOpen = dip(t, 5200, 5800, 0.35f);
    drawOwlIdle(s, p);
  } else {
    Xf b = f;  // the brows ride up less than he stretches, so they stay out of the hidden top rows
    b.squash = fminf(f.squash, 1.03f);
    bar(s, b, 58, 28, 70, 11, -6, P_INK);
    bar(s, b, 192, 28, 70, 11, 6, P_INK);
    shut(s, f);
    cheeks(s, f, 194, 18, 9, P_BLUSH);
  }
  if (t >= 3300 && t < 5400) {  // a small tear from the outer corner
    int y = lroundf(f.Y(132 + 26 * segment(t, 3000, 4800)));
    s.fillTriangle(54, y, 49, y + 9, 59, y + 9, P_WHITE);
    s.fillCircle(54, y + 11, 5, P_WHITE);
  }
  if (t >= 4300 && t < 5100) {  // feather ticks at the sides while he ruffles
    float u = segment(t, 4300, 5100);
    static const int TICKS[][2] = {{10, 150}, {310, 150}, {14, 120}, {306, 120}};
    for (int i = 0; i < 4; i++) {
      int x = TICKS[i][0], y = TICKS[i][1];
      stroke(s, x, y - 10 * u, x + (i % 2 ? 6 : -6), y - 18 * u, 3, P_INK);
    }
  }
  return true;
}

// Sneeze: brows scrunch and eyes narrow ("ah.. ah.."), then "achoo!", a jolt and spray from the beak.
static bool sneeze(TFT_eSprite& s, uint32_t t) {
  if (t >= 5000) return false;
  static const Key DY[] = {{0, 0}, {500, 0}, {1250, -8}, {2400, -8}, {2600, 10}, {3000, 0}, {5000, 0}};
  static const Key SCALE[] = {{0, 1}, {500, 1}, {1250, 0.97f}, {2400, 0.97f}, {2600, 1.12f}, {3000, 1}, {5000, 1}};
  Xf f;
  f.dy = track(t, DY);
  f.squash = track(t, SCALE);
  if (t < 800 || t >= 2950) {
    Pose p = still();
    p.dy = lroundf(f.dy);
    p.squash = f.squash;
    drawOwlIdle(s, p);
  } else {
    bar(s, f, 58, 30, 70, 11, 14, P_INK);
    bar(s, f, 192, 30, 70, 11, -14, P_INK);
    if (t < 2450) {
      float open = (1 - 0.5f * segment(t, 800, 2300)) * dip(t, 1500, 1650, 0.6f);
      disc(s, f, 96, 118, 52, open, P_INK);
      disc(s, f, 224, 118, 52, open, P_INK);
      if (open > 0.5f) {
        s.fillCircle(lroundf(f.X(116)), lroundf(f.Y(118 - 22 * open)), 14, P_WHITE);
        s.fillCircle(lroundf(f.X(244)), lroundf(f.Y(118 - 22 * open)), 14, P_WHITE);
      }
      cheeks(s, f, 194, 18, 9, P_BLUSH);
    } else {
      shut(s, f);
    }
  }
  // Bottom right: the top right is a brow now.
  if (t >= 900 && t < 2400) label(s, t < 1650 ? "ah.." : "ah.. ah..", 258, 222, 4);
  if (t >= 2450 && t < 4000) label(s, "achoo!", 258, 222, 4);
  if (t >= 2500 && t < 3500) {  // spray fans out from the beak
    float u = segment(t, 2500, 3500);
    static const int SPRAY[][2] = {{118, 214}, {138, 204}, {160, 200}, {182, 204}, {202, 214}};
    for (int i = 0; i < 5; i++) {
      s.fillCircle(lroundf(160 + (SPRAY[i][0] - 160) * u), lroundf(238 + (SPRAY[i][1] - 238) * u), i % 2 ? 2 : 3, P_WHITE);
    }
  }
  return true;
}

// Peek: "!", ducks off the right edge, peeks back with one eye, ducks out, a second lower peek, then zips
// back in from the other side looking pleased with himself.
static bool peek(TFT_eSprite& s, uint32_t t) {
  if (t >= 9000) return false;
  static const Key DX[] = {{0, 0},      {720, 0},    {1440, 340}, {2340, 340}, {2880, 206}, {3780, 206},
                           {4050, 340}, {4680, 340}, {5220, 214}, {6120, 214}, {6480, 340}, {6929, 340},
                           {6930, -340}, {7920, 0},  {9000, 0}};
  static const Key DY[] = {{0, 0}, {4680, 0}, {5220, 50}, {6929, 50}, {6930, 0}, {9000, 0}};
  static const Key LOOK[] = {{0, 0},    {2700, 0}, {2970, -14}, {3600, -14}, {3780, 0},
                             {5130, 0}, {5400, -14}, {6030, -14}, {6300, 0}, {9000, 0}};
  float dx = track(t, DX), dy = track(t, DY);
  if (t >= 7290 && t < 8640) {
    Xf f;
    f.dx = dx;
    f.dy = dy;
    happyFace(s, f);
  } else {
    Pose p = still();
    p.dx = lroundf(dx);
    p.dy = lroundf(dy);
    p.gazeX = track(t, LOOK) / 12;  // the owl's full glance is 12 px
    drawOwlIdle(s, p);
  }
  if (t >= 180 && t < 900) bang(s, 290, lroundf(186 + 6 * (1 - segment(t, 180, 450))));
  return true;
}

// Snore (sleepy only): a bubble slowly grows above the beak... and pops.
static bool snore(TFT_eSprite& s, uint32_t t, uint32_t now, const Pose& p) {
  if (t >= 4000) return false;
  drawOwlFace(s, M_SLEEPY, p, now);
  if (t < 3480) {
    float u = clamp01(t / 3400.0f);
    int r = lroundf(22 * (0.1f + 0.9f * u * u));
    s.fillCircle(OWL_SNORE_X, OWL_SNORE_Y, r, P_SNORE_EDGE);
    if (r > 3) s.fillCircle(OWL_SNORE_X, OWL_SNORE_Y, r - 2, P_SNORE_FILL);
  } else if (t < 3700) {
    for (int i = 0; i < 8; i++) {
      float a = i * PI_F / 4, r0 = 18 + (t - 3480) / 20.0f;
      stroke(s, OWL_SNORE_X + r0 * cosf(a), OWL_SNORE_Y + r0 * sinf(a), OWL_SNORE_X + (r0 + 5) * cosf(a),
             OWL_SNORE_Y + (r0 + 5) * sinf(a), 2, P_SNORE_EDGE);
    }
  }
  return true;
}

// ---- poke reactions ----

// Woken from sleepy: eyes pop to alert size with a "!" bottom right, a small jump, groggy blinks, a look
// around. The caller has already switched to idle and swapped in the startle's pink palette.
static bool startle(TFT_eSprite& s, uint32_t t, Pose p) {
  if (t >= 2500) return false;
  float r = 52;
  if (t < 800) {  // the jolt: 12 px, so the brows stay on screen
    p.dy = t < 450 ? lroundf(-12 * sinf(PI_F * t / 450)) : 0;
    r = 58;
    p.eyeOpen = 1.05f;
    p.browLift = 10;
    p.gazeX = p.gazeY = 0;
  } else if (t < 1700) {
    r = 58 - 6 * segment(t, 800, 1200);
    p.browLift = 10 * (1 - segment(t, 800, 1200));
    p.eyeOpen = dip(t, 1000, 1300, 0.15f) * dip(t, 1400, 1700, 0.2f);
    p.gazeX = p.gazeY = 0;
  } else {
    float u = (t - 1700) / 800.0f;  // left, right, back to the middle
    p.gazeX = u < 0.3f ? -segment(t, 1700, 1940) : (u < 0.7f ? -1 + 2 * segment(t, 1940, 2260) : 1 - segment(t, 2260, 2500));
  }
  drawOwlIdle(s, p, r);
  if (t < 1500) bang(s, 290, 186);
  if (clipSnoreBurst() && t < 250) {  // the snore bubble bursts, above the beak
    for (int i = 0; i < 8; i++) {
      float a = i * PI_F / 4, r0 = 12 + t / 25.0f;
      stroke(s, OWL_SNORE_X + r0 * cosf(a), OWL_SNORE_Y + r0 * sinf(a), OWL_SNORE_X + (r0 + 6) * cosf(a),
             OWL_SNORE_Y + (r0 + 6) * sinf(a), 3, P_WHITE);
    }
  }
  return true;
}

// Squeezed ^ ^ eyes, brows up, a big blush and a wiggle, in whatever mood's colours are showing. No grin.
static bool giggle(TFT_eSprite& s, uint32_t t, uint32_t duration) {
  if (t >= duration) return false;
  Xf f;
  f.dx = 5 * sinf(2 * PI_F * t / 220.0f);
  f.dy = -3 * fabsf(sinf(2 * PI_F * t / 440.0f));
  bar(s, f, 58, 24, 70, 11, -8, P_INK);
  bar(s, f, 192, 24, 70, 11, 8, P_INK);
  arcs(s, f, 134, 84, 20);
  blob(s, f, 96, 178, 26, 14, P_BLUSH);
  blob(s, f, 224, 178, 26, 14, P_BLUSH);
  return true;
}

// A quick peek-a-boo: slides off the right edge, the left eye peeks back in, then slides home.
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
  drawOwlFace(s, m, p, now);
  return true;
}

bool drawOwlClip(TFT_eSprite& s, ClipId id, Mood m, uint32_t t, uint32_t now, Pose p) {
  switch (id) {
    case CLIP_WHISTLE:
      return hum(s, t);
    case CLIP_SNACK:
      return peck(s, t);
    case CLIP_COFFEE:
      return perkUp(s, t);
    case CLIP_YAWN:
      return stretch(s, t);
    case CLIP_SNEEZE:
      return sneeze(s, t);
    case CLIP_PEEK:
      return peek(s, t);
    case CLIP_SNORE:
      return snore(s, t, now, p);
    case CLIP_STARTLE:
      return startle(s, t, p);
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
