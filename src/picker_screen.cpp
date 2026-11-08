#include "picker_screen.h"

#include "draw.h"
#include "palette.h"

static const int TILE_Y = 52, TILE_W = 136, TILE_H = 140, TILE_R = 16, BORDER = 5;
static const int TILE_X[STYLE_COUNT] = {16, 168};

static void tile(TFT_eSprite& s, int x, bool highlighted) {
  if (highlighted) {
    s.fillRoundRect(x, TILE_Y, TILE_W, TILE_H, TILE_R, P_INK);
    s.fillRoundRect(x + BORDER, TILE_Y + BORDER, TILE_W - 2 * BORDER, TILE_H - 2 * BORDER, TILE_R - BORDER, P_WHITE);
  } else {
    s.fillRoundRect(x, TILE_Y, TILE_W, TILE_H, TILE_R, P_BUBBLE);
  }
}

static void owlPreview(TFT_eSprite& s, float open) {
  Xf f;
  bar(s, f, 38, 70, 34, 6, 8, P_INK);
  bar(s, f, 94, 70, 34, 6, -8, P_INK);
  s.fillEllipse(58, 104, 24, max(2, (int)lroundf(24 * open)), P_INK);
  s.fillEllipse(110, 104, 24, max(2, (int)lroundf(24 * open)), P_INK);
  if (open > 0.5f) {
    s.fillCircle(67, 94, 6, P_WHITE);
    s.fillCircle(119, 94, 6, P_WHITE);
  }
  s.fillEllipse(60, 142, 9, 5, P_BLUSH);
  s.fillEllipse(108, 142, 9, 5, P_BLUSH);
}

static void classicPreview(TFT_eSprite& s, float open) {
  Xf f;
  bar(s, f, 200, 70, 22, 6, 0, P_INK);
  bar(s, f, 250, 70, 22, 6, 0, P_INK);
  pill(s, f, 200, 82, 22, 36, open, P_INK);
  pill(s, f, 250, 82, 22, 36, open, P_INK);
  if (open > 0.5f) {
    s.fillCircle(215, 91, 4, P_WHITE);
    s.fillCircle(265, 91, 4, P_WHITE);
  }
  s.fillEllipse(196, 134, 7, 4, P_BLUSH);
  s.fillEllipse(276, 134, 7, 4, P_BLUSH);
  quad(s, 228, 136, 236, 144, 244, 136, 4, P_INK);
}

void drawPicker(TFT_eSprite& s, const StylePicker& p, uint32_t now) {
  s.setTextColor(P_INK);
  s.setTextDatum(MC_DATUM);
  s.drawString("Which face?", 160, 30, 4);

  float open = now % 3200 < 150 ? 0.1f : 1;  // both previews blink together
  for (int i = 0; i < STYLE_COUNT; i++) tile(s, TILE_X[i], p.highlighted() == i);
  owlPreview(s, open);
  classicPreview(s, open);
  s.drawString("Owl", 84, 178, 2);
  s.drawString("Classic", 236, 178, 2);

  uint32_t left = p.msLeft(now);
  s.fillRoundRect(16, 206, 288, 8, 4, P_TRACK);
  int w = (int)(288 * (uint64_t)left / p.windowMs());
  if (w >= 8) s.fillRoundRect(16, 206, w, 8, 4, P_INK);
  char text[32];
  snprintf(text, sizeof(text), "%s starts in %u s", p.highlighted() == S_OWL ? "Owl" : "Classic",
           (unsigned)((left + 999) / 1000));
  s.drawString(text, 160, 226, 2);
  s.setTextDatum(TL_DATUM);
}
