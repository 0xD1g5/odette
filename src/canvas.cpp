#include "canvas.h"

#include "palette.h"

#ifndef ROTATION
#define ROTATION 1
#endif

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite spr = TFT_eSprite(&tft);
static Rect lastPush = FULL_RECT;

Rect unionRect(const Rect& a, const Rect& b) {
  int16_t x0 = min(a.x, b.x), y0 = min(a.y, b.y);
  int16_t x1 = max(a.x + a.w, b.x + b.w), y1 = max(a.y + a.h, b.y + b.h);
  return {x0, y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};
}

static Rect clipToScreen(Rect r) {
  int16_t x0 = max<int16_t>(r.x, 0), y0 = max<int16_t>(r.y, 0);
  int16_t x1 = min<int16_t>(r.x + r.w, 320), y1 = min<int16_t>(r.y + r.h, 240);
  return {x0, y0, (int16_t)max(0, x1 - x0), (int16_t)max(0, y1 - y0)};
}

bool canvasBegin() {
  tft.init();
  tft.setRotation(ROTATION);
  spr.setColorDepth(4);
  if (!spr.createSprite(320, 240)) return false;
  spr.createPalette(paletteFor(M_IDLE), PAL_COUNT);
  return true;
}

TFT_eSPI& display() { return tft; }
TFT_eSprite& canvas() { return spr; }

void canvasSetMood(Mood m) {
  const uint16_t* p = paletteFor(m);
  for (uint8_t i = 0; i < PAL_COUNT; i++) spr.setPaletteColor(i, p[i]);
}

void canvasPush(const Rect& dirty) {
  Rect r = clipToScreen(unionRect(dirty, lastPush));
  if (r.w > 0 && r.h > 0) spr.pushSprite(r.x, r.y, r.x, r.y, r.w, r.h);
  lastPush = dirty;
}
