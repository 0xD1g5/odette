#include "ring.h"

#include <math.h>

#include "palette.h"

// Mockup: circle at (288, 208) for Classic, r 22, stroke 7, so the band runs from about r 18.5 to 25.5.
static const int CY = 208, R_OUT = 25, R_IN = 18;
static const float R_MID = (R_OUT + R_IN) / 2.0f;

// TFT_eSPI arc angles run clockwise from 6 o'clock, so 12 o'clock is 180.
static void cap(TFT_eSprite& s, int cx, uint32_t angle) {
  float a = angle * (float)M_PI / 180.0f;
  s.fillCircle(lroundf(cx - R_MID * sinf(a)), lroundf(CY + R_MID * cosf(a)), (R_OUT - R_IN) / 2, P_INK);
}

void drawRing(TFT_eSprite& s, int pct, int cx) {
  if (pct < 0) return;
  if (pct > 100) pct = 100;
  s.drawArc(cx, CY, R_OUT, R_IN, 0, 360, P_TRACK, P_BG, false);
  if (pct >= 100) {
    s.drawArc(cx, CY, R_OUT, R_IN, 0, 360, P_INK, P_BG, false);
  } else if (pct > 0) {
    uint32_t end = (180 + pct * 360 / 100) % 360;
    s.drawArc(cx, CY, R_OUT, R_IN, 180, end, P_INK, P_BG, false);
    cap(s, cx, 180);  // round line ends, like stroke-linecap="round"
    cap(s, cx, end);
  }
  char text[4];
  snprintf(text, sizeof(text), "%d", pct);
  s.setTextDatum(MC_DATUM);
  s.setTextColor(P_INK);
  s.drawString(text, cx, CY + 1, 2);
  s.setTextDatum(TL_DATUM);
}
