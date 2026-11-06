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

void fillConvex(TFT_eSprite& s, const float* xs, const float* ys, int n, uint8_t col) {
  float cx = 0, cy = 0;
  for (int i = 0; i < n; i++) {
    cx += xs[i];
    cy += ys[i];
  }
  cx /= n;
  cy /= n;
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    s.fillTriangle(lroundf(cx), lroundf(cy), lroundf(xs[i]), lroundf(ys[i]), lroundf(xs[j]), lroundf(ys[j]), col);
  }
}

void Path::add(float x, float y) {
  lastX_ = x;
  lastY_ = y;
  if (n_ < MAX_POINTS) {
    xs_[n_] = f_.X(x);
    ys_[n_] = f_.Y(y);
    n_++;
  }
}

Path& Path::move(float x, float y) {
  n_ = 0;
  add(x, y);
  return *this;
}

Path& Path::line(float x, float y) {
  add(x, y);
  return *this;
}

Path& Path::quad(float cx, float cy, float x, float y) {
  float x0 = lastX_, y0 = lastY_;
  for (int i = 1; i <= QUAD_SEGMENTS; i++) {
    float t = (float)i / QUAD_SEGMENTS, u = 1 - t;
    add(u * u * x0 + 2 * u * t * cx + t * t * x, u * u * y0 + 2 * u * t * cy + t * t * y);
  }
  return *this;
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
