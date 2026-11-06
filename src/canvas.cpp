#include "canvas.h"

#include "diff.h"
#include "palette.h"

#ifndef ROTATION
#define ROTATION 1
#endif

static const int W = 320, H = 240, BAND_ROWS = 16, MAX_SPANS = H / BAND_ROWS;

static TFT_eSPI tft = TFT_eSPI();
static TFT_eSprite spr = TFT_eSprite(&tft);
static uint8_t* shown = nullptr;  // the frame currently on the display

bool canvasBegin() {
  tft.init();
  tft.setRotation(ROTATION);
  spr.setColorDepth(4);
  if (!spr.createSprite(W, H)) return false;
  spr.createPalette(paletteFor(M_IDLE), PAL_COUNT);
  shown = (uint8_t*)malloc(W / 2 * H);
  return shown != nullptr;
}

TFT_eSPI& display() { return tft; }
TFT_eSprite& canvas() { return spr; }

void canvasSetMood(Mood m) {
  const uint16_t* p = paletteFor(m);
  for (uint8_t i = 0; i < PAL_COUNT; i++) spr.setPaletteColor(i, p[i]);
}

void canvasPush(bool full) {
  const uint8_t* frame = (const uint8_t*)spr.getPointer();
  if (full) {
    spr.pushSprite(0, 0);
    memcpy(shown, frame, W / 2 * H);
    return;
  }
  Span spans[MAX_SPANS];
  int n = diffFrames(shown, frame, W, H, BAND_ROWS, spans, MAX_SPANS);
  for (int i = 0; i < n; i++) spr.pushSprite(spans[i].x, spans[i].y, spans[i].x, spans[i].y, spans[i].w, spans[i].h);
}
