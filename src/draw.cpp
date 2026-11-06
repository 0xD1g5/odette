#include "draw.h"

#include <math.h>

void stroke(TFT_eSprite& s, float x0, float y0, float x1, float y1, float width, uint8_t col) {
  int r = max(1, (int)(width / 2));
  float len = hypotf(x1 - x0, y1 - y0);
  int steps = max(1, (int)(len / 1.5f));
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / steps;
    s.fillCircle((int)lroundf(x0 + (x1 - x0) * t), (int)lroundf(y0 + (y1 - y0) * t), r, col);
  }
}

void quad(TFT_eSprite& s, float x0, float y0, float cx, float cy, float x1, float y1, float width, uint8_t col) {
  const int segments = 16;
  float px = x0, py = y0;
  for (int i = 1; i <= segments; i++) {
    float t = (float)i / segments, u = 1 - t;
    float x = u * u * x0 + 2 * u * t * cx + t * t * x1;
    float y = u * u * y0 + 2 * u * t * cy + t * t * y1;
    stroke(s, px, py, x, y, width, col);
    px = x;
    py = y;
  }
}

void bar(TFT_eSprite& s, const Xf& f, float x, float y, float w, float h, float deg, uint8_t col) {
  float cx = x + w / 2, cy = y + h / 2, half = (w - h) / 2;
  float a = deg * (float)M_PI / 180.0f;
  float ex = cosf(a) * half, ey = sinf(a) * half;
  stroke(s, f.X(cx - ex), f.Y(cy - ey), f.X(cx + ex), f.Y(cy + ey), f.H(h), col);
}

void pill(TFT_eSprite& s, const Xf& f, float x, float y, float w, float h, float open, uint8_t col) {
  float cy = f.Y(y + h / 2);
  float ph = max(4.0f, f.H(h) * open);
  int r = (int)(min(w, ph) / 2);
  s.fillRoundRect((int)lroundf(f.X(x)), (int)lroundf(cy - ph / 2), (int)lroundf(w), (int)lroundf(ph), r, col);
}
