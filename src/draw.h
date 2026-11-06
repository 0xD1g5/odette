// Shape helpers for the 4-bit canvas, in the mockups' terms (round-capped strokes, quadratic curves,
// rounded bars). All are built from plain fillCircle/fillRoundRect: anti-aliased TFT_eSPI calls would blend
// into colours that are not in the palette.
#pragma once

#include <TFT_eSPI.h>

// Places mockup coordinates on screen: shift by (dx, dy), and stretch vertically by `squash` around y = PIVOT_Y.
struct Xf {
  static const int PIVOT_Y = 200;  // near the chin, so stretching and squishing keep him on the ground
  float dx = 0, dy = 0, squash = 1;
  float X(float x) const { return x + dx; }
  float Y(float y) const { return PIVOT_Y + (y - PIVOT_Y) * squash + dy; }
  float H(float h) const { return h * squash; }
  Xf moved(float mx, float my) const {
    Xf o = *this;
    o.dx += mx;
    o.dy += my;
    return o;
  }
};

// A line `width` thick with round ends (SVG stroke-linecap="round").
void stroke(TFT_eSprite& s, float x0, float y0, float x1, float y1, float width, uint8_t col);

// SVG "M x0 y0 Q cx cy x1 y1" drawn with stroke().
void quad(TFT_eSprite& s, float x0, float y0, float cx, float cy, float x1, float y1, float width, uint8_t col);

// SVG <rect x y w h rx=h/2> rotated by `deg` about its centre: a brow bar.
void bar(TFT_eSprite& s, const Xf& f, float x, float y, float w, float h, float deg, uint8_t col);

// Filled convex polygon (screen coordinates), as a triangle fan from its centroid.
void fillConvex(TFT_eSprite& s, const float* xs, const float* ys, int n, uint8_t col);

// A closed SVG path of M, L and Q commands in mockup coordinates, filled. The shape must be convex.
//   Path(f).move(122, 148).line(198, 148).quad(194, 204, 160, 204).quad(126, 204, 122, 148).fill(s, P_INK);
class Path {
 public:
  explicit Path(const Xf& f) : f_(f) {}
  Path& move(float x, float y);
  Path& line(float x, float y);
  Path& quad(float cx, float cy, float x, float y);
  void fill(TFT_eSprite& s, uint8_t col) const { fillConvex(s, xs_, ys_, n_, col); }

 private:
  static const int MAX_POINTS = 64, QUAD_SEGMENTS = 10;
  void add(float x, float y);
  Xf f_;
  float xs_[MAX_POINTS], ys_[MAX_POINTS];
  float lastX_ = 0, lastY_ = 0;  // current point, mockup coordinates
  int n_ = 0;
};

// Upright pill (radius = half the narrower side), scaled vertically by `open` about its centre: an eye.
void pill(TFT_eSprite& s, const Xf& f, float x, float y, float w, float h, float open, uint8_t col);
