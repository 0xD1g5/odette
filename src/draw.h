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

// Upright pill (radius = half the narrower side), scaled vertically by `open` about its centre: an eye.
void pill(TFT_eSprite& s, const Xf& f, float x, float y, float w, float h, float open, uint8_t col);
